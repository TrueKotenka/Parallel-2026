#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <iomanip>

void merge(std::vector<int>& arr, int left, int mid, int right) {
    int n1 = mid - left + 1;
    int n2 = right - mid;
    
    std::vector<int> L(n1), R(n2);
    for (int i = 0; i < n1; ++i) L[i] = arr[left + i];
    for (int i = 0; i < n2; ++i) R[i] = arr[mid + 1 + i];

    int i = 0, j = 0, k = left;
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) { arr[k] = L[i]; i++; } 
        else { arr[k] = R[j]; j++; }
        k++;
    }
    while (i < n1) { arr[k] = L[i]; i++; k++; }
    while (j < n2) { arr[k] = R[j]; j++; k++; }
}

// 1. Однопоточная версия сортировки слиянием
void mergeSort(std::vector<int>& arr, int left, int right) {
    if (left >= right) return;
    int mid = left + (right - left) / 2;
    mergeSort(arr, left, mid);
    mergeSort(arr, mid + 1, right);
    merge(arr, left, mid, right);
}

// 2. Многопоточная версия сортировки слиянием
// Через THRESHOLD контролируем когда вызывать однопоточную версию
const int THRESHOLD = 10000; 

void parallelMergeSort(std::vector<int>& arr, int left, int right, int depth = 0) {
    if (left >= right) return;
    
    int mid = left + (right - left) / 2;

    if (right - left < THRESHOLD || depth > std::thread::hardware_concurrency()) {
        mergeSort(arr, left, right);
        return;
    }

    // Запускаем левую часть в новом потоке, а правую сортируем в текущем
    std::thread leftThread(parallelMergeSort, std::ref(arr), left, mid, depth + 1);
    parallelMergeSort(arr, mid + 1, right, depth + 1);
    
    // Ждем завершения потока перед слиянием
    leftThread.join();
    merge(arr, left, mid, right);
}

std::vector<int> generateRandomArray(size_t size) {
    std::vector<int> arr(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 1000000);
    for (size_t i = 0; i < size; ++i) {
        arr[i] = dis(gen);
    }
    return arr;
}

int main() {
    std::vector<size_t> sizes = {10000, 100000, 1000000, 10000000};

    std::cout << "Amount of logical kernels: " << std::thread::hardware_concurrency() << "\n\n";
    std::cout << std::left << std::setw(15) << "Size (N)" 
              << std::setw(25) << "Single thread (ms)" 
              << std::setw(25) << "Multi thread (ms)" << "\n";
    std::cout << std::string(60, '-') << "\n";

    for (size_t size : sizes) {
        std::vector<int> original = generateRandomArray(size);
        
        std::vector<int> arr1 = original;
        std::vector<int> arr2 = original;

        // Замер однопоточной версии
        auto start1 = std::chrono::high_resolution_clock::now();
        mergeSort(arr1, 0, arr1.size() - 1);
        auto end1 = std::chrono::high_resolution_clock::now();
        auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1).count();

        // Замер многопоточной версии
        auto start2 = std::chrono::high_resolution_clock::now();
        parallelMergeSort(arr2, 0, arr2.size() - 1);
        auto end2 = std::chrono::high_resolution_clock::now();
        auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2).count();

        std::cout << std::left << std::setw(15) << size 
                  << std::setw(25) << duration1 
                  << std::setw(25) << duration2 << "\n";
    }

    return 0;
}