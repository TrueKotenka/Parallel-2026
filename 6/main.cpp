#include <atomic>
#include <thread>
#include <iostream>
#include <semaphore>

// Используем выравнивание, чтобы переменные не попали в одну кэш-линию (False Sharing)
alignas(64) std::atomic<int> X{0};
alignas(64) std::atomic<int> Y{0};
int r1 = 0, r2 = 0;

// Синхронизация итераций
std::binary_semaphore sem_begin1{0}, sem_begin2{0};
std::binary_semaphore sem_end1{0}, sem_end2{0};

void thread1() {
    while (true) {
        sem_begin1.acquire(); // Ждем старта итерации
        
        X.store(1, std::memory_order_seq_cst); // Store X
        r1 = Y.load(std::memory_order_seq_cst); // Load Y
        
        sem_end1.release(); // Сообщаем о завершении
    }
}

void thread2() {
    while (true) {
        sem_begin2.acquire(); // Ждем старта итерации
        
        Y.store(1, std::memory_order_seq_cst); // Store Y
        r2 = X.load(std::memory_order_seq_cst); // Load X
        
        sem_end2.release(); // Сообщаем о завершении
    }
}

int main() {
    std::thread t1(thread1);
    std::thread t2(thread2);

    int detected = 0;
    const int iterations = 500000;

    for (int i = 0; i < iterations; ++i) {
        X.store(0, std::memory_order_seq_cst);
        Y.store(0, std::memory_order_seq_cst);

        // Запускаем оба потока одновременно
        sem_begin1.release();
        sem_begin2.release();

        // Ждем их завершения
        sem_end1.acquire();
        sem_end2.acquire();

        // Если оба прочитали 0 — произошло переупорядочивание Store-Load!
        if (r1 == 0 && r2 == 0) {
            detected++;
        }
    }

    std::cout << "Store-Load reordering detected: " 
              << detected << " times out of " << iterations << "\n";

    // Убиваем потоки (для простоты примера)
    t1.detach();
    t2.detach();

    return 0;
}