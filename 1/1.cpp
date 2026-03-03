#include <iostream>
#include <chrono>
#include <cstdlib>
#include <immintrin.h>
#include <malloc.h>

const size_t N = 1e6;

void add_naive(const float* __restrict a, const float* __restrict b, float* __restrict c, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        c[i] = a[i] + b[i];
    }
}

void add_sse(const float* a, const float* b, float* c, size_t n) {
    size_t i = 0;
    for (; i + 3 < n; i += 4) {
        __m128 va = _mm_load_ps(&a[i]);
        __m128 vb = _mm_load_ps(&b[i]);
        __m128 vc = _mm_add_ps(va, vb);
        _mm_store_ps(&c[i], vc);
    }
    for (; i < n; ++i) {
        c[i] = a[i] + b[i];
    }
}

void add_avx(const float* a, const float* b, float* c, size_t n) {
    size_t i = 0;
    for (; i + 7 < n; i += 8) {
        __m256 va = _mm256_load_ps(&a[i]);
        __m256 vb = _mm256_load_ps(&b[i]);
        __m256 vc = _mm256_add_ps(va, vb);
        _mm256_store_ps(&c[i], vc);
    }
    for (; i < n; ++i) {
        c[i] = a[i] + b[i];
    }
}

int main() {
    float* a = (float*)_mm_malloc(N * sizeof(float), 32);
    float* b = (float*)_mm_malloc(N * sizeof(float), 32);
    float* c = (float*)_mm_malloc(N * sizeof(float), 32);

    if (!a || !b || !c) {
        std::cerr << "Malloc error" << std::endl;
        return 1;
    }

    for (size_t i = 0; i < N; ++i) {
        a[i] = static_cast<float>(rand()) / RAND_MAX;
        b[i] = static_cast<float>(rand()) / RAND_MAX;
    }

    auto measure = [&](auto func, const char* name) {
        auto start = std::chrono::high_resolution_clock::now();
        func(a, b, c, N);
        auto end = std::chrono::high_resolution_clock::now();

        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    };

    double naive_time = measure(add_naive, "Naive");
    double sse_time   = measure(add_sse,   "SSE");
    double avx_time   = measure(add_avx,   "AVX");

    std::cout << "Naive: " << naive_time << " мкс\n";
    std::cout << "SSE:   " << sse_time   << " мкс (ускорение " << naive_time / sse_time << "x)\n";
    std::cout << "AVX:   " << avx_time   << " мкс (ускорение " << naive_time / avx_time << "x)\n";

    _mm_free(a);
    _mm_free(b);
    _mm_free(c);
    return 0;
}