#include <iostream>
#include <limits>
#include <mutex>
#include <thread>
#include <vector>
#include <random>
#include <atomic>
#include <chrono>

class ISet {
public:
    virtual ~ISet() = default;
    virtual bool add(int key) = 0;
    virtual bool remove(int key) = 0;
    virtual bool contains(int key) = 0;
};

class CoarseSet : public ISet {
private:
    struct Node {
        int key;
        Node* next;
        Node(int k) : key(k), next(nullptr) {}
    };

    Node* head;
    std::mutex mtx;

public:
    CoarseSet() {
        head = new Node(std::numeric_limits<int>::min());
        head->next = new Node(std::numeric_limits<int>::max());
    }

    ~CoarseSet() {
        Node* curr = head;
        while (curr != nullptr) {
            Node* temp = curr;
            curr = curr->next;
            delete temp;
        }
    }

    bool add(int key) override {
        std::lock_guard<std::mutex> lock(mtx);
        Node* pred = head;
        Node* curr = head->next;
        
        while (curr->key < key) {
            pred = curr;
            curr = curr->next;
        }
        
        if (curr->key == key) {
            return false; // Элемент уже существует
        }
        
        Node* newNode = new Node(key);
        newNode->next = curr;
        pred->next = newNode;
        return true;
    }

    bool remove(int key) override {
        std::lock_guard<std::mutex> lock(mtx);
        Node* pred = head;
        Node* curr = head->next;
        
        while (curr->key < key) {
            pred = curr;
            curr = curr->next;
        }
        
        if (curr->key == key) {
            pred->next = curr->next;
            delete curr;
            return true;
        }
        
        return false;
    }

    bool contains(int key) override {
        std::lock_guard<std::mutex> lock(mtx);
        Node* curr = head->next;
        
        while (curr->key < key) {
            curr = curr->next;
        }
        
        return curr->key == key;
    }
};

class FineGrainedSet : public ISet {
private:
    struct Node {
        int key;
        Node* next;
        std::mutex mtx; // Индивидуальный мьютекс для каждого узла

        Node(int k) : key(k), next(nullptr) {}
    };

    Node* head;

public:
    FineGrainedSet() {
        head = new Node(std::numeric_limits<int>::min());
        head->next = new Node(std::numeric_limits<int>::max());
    }

    ~FineGrainedSet() {
        Node* curr = head;
        while (curr != nullptr) {
            Node* temp = curr;
            curr = curr->next;
            delete temp;
        }
    }

    bool add(int key) override {
        head->mtx.lock();
        Node* pred = head;
        Node* curr = pred->next;
        curr->mtx.lock();

        while (curr->key < key) {
            pred->mtx.unlock(); // Отпускаем предыдущий
            pred = curr;
            curr = curr->next;
            curr->mtx.lock();   // Захватываем следующий
        }

        if (curr->key == key) {
            curr->mtx.unlock();
            pred->mtx.unlock();
            return false;
        }

        Node* newNode = new Node(key);
        newNode->next = curr;
        pred->next = newNode;
        
        curr->mtx.unlock();
        pred->mtx.unlock();
        return true;
    }

    bool remove(int key) override {
        head->mtx.lock();
        Node* pred = head;
        Node* curr = pred->next;
        curr->mtx.lock();

        while (curr->key < key) {
            pred->mtx.unlock();
            pred = curr;
            curr = curr->next;
            curr->mtx.lock();
        }

        if (curr->key == key) {
            pred->next = curr->next;
            // в C++ нельзя удалять объект с заблокированным мьютексом
            curr->mtx.unlock(); 
            pred->mtx.unlock();
            delete curr;
            return true;
        }

        curr->mtx.unlock();
        pred->mtx.unlock();
        return false;
    }

    bool contains(int key) override {
        head->mtx.lock();
        Node* pred = head;
        Node* curr = pred->next;
        curr->mtx.lock();

        while (curr->key < key) {
            pred->mtx.unlock();
            pred = curr;
            curr = curr->next;
            curr->mtx.lock();
        }

        bool found = (curr->key == key);
        curr->mtx.unlock();
        pred->mtx.unlock();
        return found;
    }
};

class OptimisticSet : public ISet {
private:
    struct Node {
        int key;
        Node* next;
        std::mutex mtx;

        Node(int k) : key(k), next(nullptr) {}
    };

    Node* head;

    // Функция валидации: проверяет, что pred всё ещё в списке 
    // и указывает прямо на curr
    bool validate(Node* pred, Node* curr) {
        Node* node = head;
        while (node->key <= pred->key) {
            if (node == pred) {
                return pred->next == curr;
            }
            node = node->next;
        }
        return false;
    }

public:
    OptimisticSet() {
        head = new Node(std::numeric_limits<int>::min());
        head->next = new Node(std::numeric_limits<int>::max());
    }

    ~OptimisticSet() {
        // Очистка при завершении (здесь утекают узлы, удаленные во время работы, 
        // но для бенчмарка мы этим пренебрегаем)
        Node* curr = head;
        while (curr != nullptr) {
            Node* temp = curr;
            curr = curr->next;
            delete temp;
        }
    }

    bool add(int key) override {
        while (true) {
            Node* pred = head;
            Node* curr = pred->next;

            // Ищем без блокировок
            while (curr->key < key) {
                pred = curr;
                curr = curr->next;
            }

            std::lock_guard<std::mutex> pred_lock(pred->mtx);
            std::lock_guard<std::mutex> curr_lock(curr->mtx);

            // Валидируем
            if (validate(pred, curr)) {
                if (curr->key == key) {
                    return false;
                }
                Node* newNode = new Node(key);
                newNode->next = curr;
                pred->next = newNode;
                return true;
            }
            // Если валидация не прошла, lock_guard'ы снимутся, 
            // и цикл while(true) запустит поиск заново
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred = head;
            Node* curr = pred->next;

            while (curr->key < key) {
                pred = curr;
                curr = curr->next;
            }

            std::lock_guard<std::mutex> pred_lock(pred->mtx);
            std::lock_guard<std::mutex> curr_lock(curr->mtx);

            if (validate(pred, curr)) {
                if (curr->key == key) {
                    pred->next = curr->next;
                    // намеренно не делаем delete curr; 
                    // чтобы не уронить читающие без блокировок потоки.
                    return true;
                }
                return false;
            }
        }
    }

    bool contains(int key) override {
        while (true) {
            Node* pred = head;
            Node* curr = pred->next;

            while (curr->key < key) {
                pred = curr;
                curr = curr->next;
            }

            // Даже для чтения нам приходится брать блокировки и валидировать,
            // чтобы убедиться, что узел действительно в списке на данный момент
            std::lock_guard<std::mutex> pred_lock(pred->mtx);
            std::lock_guard<std::mutex> curr_lock(curr->mtx);

            if (validate(pred, curr)) {
                return curr->key == key;
            }
        }
    }
};

class LazySet : public ISet {
private:
    struct Node {
        int key;
        Node* next;
        std::mutex mtx;
        std::atomic<bool> marked; // Флаг логического удаления

        Node(int k) : key(k), next(nullptr), marked(false) {}
    };

    Node* head;

    // Валидация теперь выполняется за О(1), без обхода списка
    bool validate(Node* pred, Node* curr) {
        return !pred->marked.load(std::memory_order_relaxed) && 
               !curr->marked.load(std::memory_order_relaxed) && 
               pred->next == curr;
    }

public:
    LazySet() {
        head = new Node(std::numeric_limits<int>::min());
        head->next = new Node(std::numeric_limits<int>::max());
    }

    ~LazySet() {
        Node* curr = head;
        while (curr != nullptr) {
            Node* temp = curr;
            curr = curr->next;
            delete temp;
        }
    }

    bool add(int key) override {
        while (true) {
            Node* pred = head;
            Node* curr = head->next;

            while (curr->key < key) {
                pred = curr;
                curr = curr->next;
            }

            std::lock_guard<std::mutex> pred_lock(pred->mtx);
            std::lock_guard<std::mutex> curr_lock(curr->mtx);

            if (validate(pred, curr)) {
                if (curr->key == key) {
                    return false;
                }
                Node* newNode = new Node(key);
                newNode->next = curr;
                pred->next = newNode;
                return true;
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred = head;
            Node* curr = head->next;

            while (curr->key < key) {
                pred = curr;
                curr = curr->next;
            }

            std::lock_guard<std::mutex> pred_lock(pred->mtx);
            std::lock_guard<std::mutex> curr_lock(curr->mtx);

            if (validate(pred, curr)) {
                if (curr->key == key) {
                    // 1. Логическое удаление
                    curr->marked.store(true, std::memory_order_relaxed);
                    // 2. Физическое удаление
                    pred->next = curr->next;
                    // Память (delete) снова не освобождаем ради lock-free читателей
                    return true;
                }
                return false;
            }
        }
    }

    bool contains(int key) override {
        Node* curr = head;
        while (curr->key < key) {
            curr = curr->next;
        }
        // проверяем ключ и флаг без блокировок
        return curr->key == key && !curr->marked.load(std::memory_order_relaxed);
    }
};

void runBenchmark(ISet& set, int num_threads, int write_percent, int duration_seconds, const std::string& name) {
    std::atomic<bool> start_flag{false};
    std::atomic<bool> stop_flag{false};
    std::atomic<uint64_t> total_ops{0};

    auto worker = [&](int thread_id) {
        // Локальный генератор случайных чисел для каждого потока
        std::mt19937 rng(std::random_device{}() ^ thread_id);
        std::uniform_int_distribution<int> op_dist(1, 100);
        std::uniform_int_distribution<int> key_dist(0, 1000); // Ограниченный диапазон ключей

        uint64_t ops = 0;

        // Ждем сигнала к старту, чтобы все потоки начали одновременно
        while (!start_flag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        while (!stop_flag.load(std::memory_order_relaxed)) {
            int op_chance = op_dist(rng);
            int key = key_dist(rng);

            if (op_chance <= write_percent) {
                // Половина записей - вставка, половина - удаление
                if (op_chance <= write_percent / 2) {
                    set.add(key);
                } else {
                    set.remove(key);
                }
            } else {
                set.contains(key);
            }
            ops++;
        }
        total_ops.fetch_add(ops, std::memory_order_relaxed);
    };

    // Предзаполнение множества, чтобы поиск не работал по пустому списку
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> key_dist(0, 1000);
    for (int i = 0; i < 500; ++i) {
        set.add(key_dist(rng));
    }

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, i);
    }

    // Даем потокам время на инициализацию, затем запускаем
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    start_flag.store(true, std::memory_order_release);

    // Ждем заданное время
    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));

    // Останавливаем
    stop_flag.store(true, std::memory_order_relaxed);

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "[" << name << "] Threads: " << num_threads 
              << " | Operations per second: " << (total_ops.load() / duration_seconds) << "\n";
}

int main() {
    const int duration = 2; // секунды на тест
    const int write_ratio = 20; // 20% запись, 80% чтение

    std::cout << "--- Benchmark: " << write_ratio << "% Write / " << (100 - write_ratio) << "% Read ---\n\n";

    std::vector<int> thread_counts = {1, 2, 4, 8, 16};

    for (int threads : thread_counts) {
        CoarseSet coarse_set;
        runBenchmark(coarse_set, threads, write_ratio, duration, "Coarse-Grained");
    }

    std::cout << "\n";

    for (int threads : thread_counts) {
        FineGrainedSet fine_set;
        runBenchmark(fine_set, threads, write_ratio, duration, "Fine-Grained  ");
    }

    std::cout << "\n";
    for (int threads : thread_counts) {
        OptimisticSet opt_set;
        runBenchmark(opt_set, threads, write_ratio, duration, "Optimistic    ");
    }

    std::cout << "\n";
    for (int threads : thread_counts) {
        LazySet lazy_set;
        runBenchmark(lazy_set, threads, write_ratio, duration, "Lazy          ");
    }

    return 0;
}