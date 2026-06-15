#include <iostream>
#include <atomic>
#include <cstdint>
#include <vector>
#include <thread>
#include <set>
#include <algorithm>
#include <cassert>

namespace concurrent {

    template<typename T>
    class set {
    private:
        struct alignas(8) Node {
            T value;
            std::atomic<uintptr_t> next;

            Node() : next(0) {}
            Node(T val, Node* nxt) : value(val), next(reinterpret_cast<uintptr_t>(nxt)) {}
        };

        Node* head;

        // Вспомогательные функции для работы с маркированными указателями
        static Node* get_unmarked(uintptr_t val) {
            return reinterpret_cast<Node*>(val & ~1ULL);
        }

        static Node* get_marked(Node* ptr) {
            return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
        }

        static bool is_marked(uintptr_t val) {
            return (val & 1ULL) != 0;
        }

        struct Window {
            Node* pred;
            Node* curr;
        };

        // Внутренняя функция поиска: находит позицию для ключа и 
        // заодно физически удаляет логически удаленные (маркированные) узлы.
        Window find(Node* head_node, const T& value) const {
            Node* pred = nullptr;
            Node* curr = nullptr;
            Node* succ = nullptr;
            bool marked = false;
            bool snip;

        retry:
            while (true) {
                pred = head_node;
                uintptr_t curr_ptr = pred->next.load(std::memory_order_acquire);
                curr = get_unmarked(curr_ptr);

                while (true) {
                    if (curr == nullptr) return Window{pred, curr};
                    
                    uintptr_t succ_ptr = curr->next.load(std::memory_order_acquire);
                    succ = get_unmarked(succ_ptr);
                    marked = is_marked(succ_ptr);

                    if (marked) {
                        // Физическое удаление узла
                        uintptr_t expected = reinterpret_cast<uintptr_t>(curr);
                        uintptr_t desired = reinterpret_cast<uintptr_t>(succ);
                        snip = pred->next.compare_exchange_strong(expected, desired, std::memory_order_release, std::memory_order_relaxed);
                        if (!snip) {
                            break; // Если другой поток вмешался, начинаем сначала
                        }
                        // По хорошему здесь должна быть система сборки мусора
                        
                        curr = succ;
                    } else {
                        if (curr->value >= value) return Window{pred, curr};
                        pred = curr;
                        curr = succ;
                    }
                }
            }
        }

    public:
        set() {
            head = new Node(); // Dummy-head
        }

        ~set() {
            Node* curr = head;
            while (curr != nullptr) {
                Node* next = get_unmarked(curr->next.load());
                delete curr;
                curr = next;
            }
        }

        bool add(T value) {
            while (true) {
                Window window = find(head, value);
                Node* pred = window.pred;
                Node* curr = window.curr;

                if (curr != nullptr && curr->value == value) {
                    return false; // Ключ уже существует
                } else {
                    Node* node = new Node(value, curr);
                    uintptr_t expected = reinterpret_cast<uintptr_t>(curr);
                    uintptr_t desired = reinterpret_cast<uintptr_t>(node);
                    
                    // Пытаемся вставить узел
                    if (pred->next.compare_exchange_strong(expected, desired, std::memory_order_release, std::memory_order_relaxed)) {
                        return true;
                    }
                    delete node; // Если CAS провалился, удаляем локально созданный узел и повторяем
                }
            }
        }

        bool remove(T value) {
            while (true) {
                Window window = find(head, value);
                Node* pred = window.pred;
                Node* curr = window.curr;

                if (curr == nullptr || curr->value != value) {
                    return false; // Ключ не найден
                } else {
                    uintptr_t succ_ptr = curr->next.load(std::memory_order_acquire);
                    Node* succ = get_unmarked(succ_ptr);
                    
                    if (is_marked(succ_ptr)) return false; // Уже логически удален другим потоком

                    // Логическое удаление (установка бита маркировки)
                    uintptr_t expected = succ_ptr;
                    uintptr_t desired = reinterpret_cast<uintptr_t>(get_marked(succ));
                    bool snip = curr->next.compare_exchange_strong(expected, desired, std::memory_order_release, std::memory_order_relaxed);
                    
                    if (!snip) continue; // CAS провалился, повторяем попытку

                    // Попытка физического удаления
                    expected = reinterpret_cast<uintptr_t>(curr);
                    desired = reinterpret_cast<uintptr_t>(succ);
                    pred->next.compare_exchange_strong(expected, desired, std::memory_order_release, std::memory_order_relaxed);
                    
                    return true;
                }
            }
        }

        bool contains(T value) const {
            // Идем по списку, игнорируя маркировки, и не используем CAS циклы.
            Node* curr = get_unmarked(head->next.load(std::memory_order_acquire));
            while (curr != nullptr && curr->value < value) {
                curr = get_unmarked(curr->next.load(std::memory_order_acquire));
            }
            if (curr == nullptr || curr->value != value) return false;
            
            // Если узел найден, возвращаем true только если он не маркирован (не удален логически)
            return !is_marked(curr->next.load(std::memory_order_acquire));
        }

        bool isEmpty() const {
            Node* curr = get_unmarked(head->next.load(std::memory_order_acquire));
            while (curr != nullptr) {
                if (!is_marked(curr->next.load(std::memory_order_acquire))) {
                    return false; // Нашли хотя бы один не удаленный элемент
                }
                curr = get_unmarked(curr->next.load(std::memory_order_acquire));
            }
            return true;
        }
    };
} // namespace concurrent

// =========================================================================
// 10.2 Нагрузочное тестирование на линеаризуемость
// =========================================================================

enum OpType { ADD, REMOVE, CONTAINS, ISEMPTY };

struct Operation {
    int id;
    OpType type;
    int value; // Для ISEMPTY значение игнорируется
};

using TraceResult = std::vector<bool>;

int main() {
    // Создаем набор операций для тестирования
    std::vector<Operation> ops = {
        {0, ADD, 10},
        {1, ADD, 20},
        {2, CONTAINS, 10},
        {3, REMOVE, 10},
        {4, ISEMPTY, 0},
        {5, ADD, 30}
    };

    std::set<TraceResult> valid_traces;

    std::sort(ops.begin(), ops.end(), [](const Operation& a, const Operation& b) { return a.id < b.id; });

    std::cout << "Generating all valid sequential traces (Linearizability baselines)..." << std::endl;
    do {
        std::set<int> seq_set;
        TraceResult current_trace(ops.size());
        
        for (const auto& op : ops) {
            bool res = false;
            switch (op.type) {
                case ADD: 
                    res = seq_set.insert(op.value).second; 
                    break;
                case REMOVE: 
                    res = seq_set.erase(op.value) > 0; 
                    break;
                case CONTAINS: 
                    res = seq_set.find(op.value) != seq_set.end(); 
                    break;
                case ISEMPTY: 
                    res = seq_set.empty(); 
                    break;
            }
            current_trace[op.id] = res;
        }
        valid_traces.insert(current_trace);
    } while (std::next_permutation(ops.begin(), ops.end(), [](const Operation& a, const Operation& b) { return a.id < b.id; }));

    std::cout << "Generated " << valid_traces.size() << " unique valid traces." << std::endl;

    // Запускаем тот же набор операций в многопоточной среде
    std::cout << "Running multithreaded test..." << std::endl;
    
    concurrent::set<int> lock_free_set;
    TraceResult multithreaded_trace(ops.size());
    std::vector<std::thread> threads;

    for (const auto& op : ops) {
        threads.emplace_back([&lock_free_set, &multithreaded_trace, op]() {
            bool res = false;
            switch (op.type) {
                case ADD: 
                    res = lock_free_set.add(op.value); 
                    break;
                case REMOVE: 
                    res = lock_free_set.remove(op.value); 
                    break;
                case CONTAINS: 
                    res = lock_free_set.contains(op.value); 
                    break;
                case ISEMPTY: 
                    res = lock_free_set.isEmpty(); 
                    break;
            }
            multithreaded_trace[op.id] = res;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 4. Проверяем линеаризуемость
    if (valid_traces.find(multithreaded_trace) != valid_traces.end()) {
        std::cout << "SUCCESS: Multithreaded execution is LINEARIZABLE.\n";
        std::cout << "The multithreaded trace perfectly matches one of the valid sequential permutations." << std::endl;
    } else {
        std::cout << "FAILURE: Multithreaded trace is NOT linearizable!" << std::endl;
    }

    return 0;
}