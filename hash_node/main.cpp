#include <iostream>
#include <fstream>
#include <string>
#include <csignal>
#include "client.h"

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Использование: " << argv[0] << " <host> <port> <path_to_hashrate_file>\n";
        return 1;
    }

    std::string host = argv[1];
    int port = std::stoi(argv[2]);
    std::string hashrate_file = argv[3];

    std::ifstream file(hashrate_file);
    if (!file.is_open()) {
        std::cerr << "Ошибка: не удалось открыть файл " << hashrate_file << "\n";
        return 1;
    }
    int hashrate = 0;
    file >> hashrate;
    file.close();

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = signal_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
    sigaction(SIGTERM, &sigIntHandler, NULL);

    try {
        WorkerClient client(host, port, hashrate);
        client.run();
    } catch (const std::exception& e) {
        std::cerr << "Фатальная ошибка: " << e.what() << "\n";
        return 1;
    }

    std::cout << "[*] Программа успешно завершена.\n";
    return 0;
}