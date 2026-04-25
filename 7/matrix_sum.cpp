#include <iostream>
#include <vector>
#include <numeric>
#include <chrono>
#include <random>
#include <string>
#include <cstdint>

using Matrix = std::vector<std::vector<int>>;

int64_t sumRows(const Matrix& m) {
    int64_t total = 0;
    for (size_t i = 0; i < m.size(); ++i) {
        for (size_t j = 0; j < m[i].size(); ++j) {
            total += m[i][j];
        }
    }
    return total;
}

int64_t sumCols(const Matrix& m) {
    int64_t total = 0;
    if (m.empty()) return 0;
    size_t cols = m[0].size();
    for (size_t j = 0; j < cols; ++j) {
        for (size_t i = 0; i < m.size(); ++i) {
            total += m[i][j];
        }
    }
    return total;
}

int64_t sumColsOptimized(const Matrix& m) {
    int64_t total = 0;
    if (m.empty()) return 0;
    
    size_t n = m.size();
    const size_t BLOCK_SIZE = 64; 

    for (size_t jj = 0; jj < n; jj += BLOCK_SIZE) {
        for (size_t ii = 0; ii < n; ii += BLOCK_SIZE) {
            // Внутри блока обходим элементы
            for (size_t j = jj; j < std::min(jj + BLOCK_SIZE, n); ++j) {
                for (size_t i = ii; i < std::min(ii + BLOCK_SIZE, n); ++i) {
                    total += m[i][j];
                }
            }
        }
    }
    return total;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " [rows|cols|opt]\n";
        return 1;
    }

    std::string mode = argv[1];
    const size_t N = 10000;

    std::cout << "Allocating and filling " << N << "x" << N << " matrix...\n";
    Matrix m(N, std::vector<int>(N));
    
    // Заполняем случайными числами
    std::mt19937 gen(42);
    std::uniform_int_distribution<> dis(1, 10);
    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j) {
            m[i][j] = dis(gen);
        }
    }

    std::cout << "Starting summation (" << mode << ")...\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    int64_t result = 0;

    if (mode == "rows") {
        result = sumRows(m);
    } else if (mode == "cols") {
        result = sumCols(m);
    } else if (mode == "opt") {
        result = sumColsOptimized(m);
    } else {
        std::cerr << "Unknown mode!\n";
        return 1;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << "Result: " << result << "\n";
    std::cout << "Time: " << diff.count() << " seconds\n";
}