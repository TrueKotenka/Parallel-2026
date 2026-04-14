FROM ubuntu:22.04

# Устанавливаем обычный компилятор (x86), кросс-компилятор (ARM64), QEMU и утилиту file
RUN apt-get update && apt-get install -y \
    g++ \
    g++-aarch64-linux-gnu \
    qemu-user-static \
    file

WORKDIR /app
COPY main.cpp .

# Собираем под x86_64
RUN g++ -std=c++20 -O2 -pthread main.cpp -o test_x86

# Собираем под ARM64
RUN aarch64-linux-gnu-g++ -std=c++20 -O2 -pthread main.cpp -o test_arm

# Показываем архитектуры бинарников
RUN echo "\n=== Архитектуры бинарников ===" && \
    file test_x86 && file test_arm

# Скрипт запуска (исправлен вызов QEMU)
CMD echo "\n=== Запуск на x86 (Native) ===" && ./test_x86 && \
    echo "\n=== Запуск на ARM (QEMU) ===" && qemu-aarch64-static -L /usr/aarch64-linux-gnu ./test_arm