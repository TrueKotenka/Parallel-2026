// Задача H2O
// N потоков, которые могут писать в std::out либо "O" либо "H"
//
// Необходимо синхронизовать потоки таким образом, чтобы каждая тройка букв составляла молекулу
//
// H H O H O H

// ок: HHO HOH
// ок: OHH HHO
// ок: HOH OHH
//
// не ок: OOH HHH и т.п.

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

std::mutex mtx;
std::condition_variable cv_h;
std::condition_variable cv_o;

int h_count = 0;
int o_count = 0;

void hydrogen()
{
    std::unique_lock<std::mutex> lk(mtx);
    if (h_count == 2) {
        cv_h.notify_one();
        cv_o.wait(lk, []{return o_count == 1;});
        h_count = 0;
        o_count = 0;
    }
    lk.unlock();
    std::cout << "H";
    h_count++;
}

void oxygen() 
{
    std::unique_lock<std::mutex> lk(mtx);
    if (o_count == 1) {
        cv_o.notify_one();
        cv_h.wait(lk, []{return h_count == 2;});
        h_count = 0;
        o_count = 0;
    }
    lk.unlock();
    std::cout << "O";
    o_count++;
}

int main()
{
    std::thread t1([&]{ hydrogen(); });
    std::thread t2([&]{ hydrogen(); });
    std::thread t3([&]{ oxygen(); });
    std::thread t4([&]{ hydrogen(); });
    std::thread t5([&]{ oxygen(); });
    std::thread t6([&]{ hydrogen(); });
    
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    t6.join();
    
    std::cout << std::endl;
}