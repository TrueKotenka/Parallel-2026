#include "client.h"
#include <iostream>
#include <vector>
#include <thread>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/evp.h>

std::atomic<bool> g_shutdown_requested{false};

void signal_handler(int signum) {
    std::cout << "\n[!] Получен сигнал " << signum << ". Завершение работы...\n";
    g_shutdown_requested = true;
}

std::string md5_hex(const std::string& input) {
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_md5();
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    EVP_DigestInit_ex(context, md, nullptr);
    EVP_DigestUpdate(context, input.c_str(), input.length());
    EVP_DigestFinal_ex(context, digest, &md_len);
    EVP_MD_CTX_free(context);

    char mdString[33];
    for(unsigned int i = 0; i < md_len; i++) sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);
    return std::string(mdString);
}

std::string sha1_hex(const std::string& input) {
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_sha1();
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    EVP_DigestInit_ex(context, md, nullptr);
    EVP_DigestUpdate(context, input.c_str(), input.length());
    EVP_DigestFinal_ex(context, digest, &md_len);
    EVP_MD_CTX_free(context);

    char mdString[41];
    for(unsigned int i = 0; i < md_len; i++) sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);
    return std::string(mdString);
}

std::string target_hash_func(const std::string& key) {
    return sha1_hex(md5_hex(key));
}

const std::string ALPHABET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

bool increment_key(std::string& key) {
    for (int i = key.length() - 1; i >= 0; --i) {
        size_t pos = ALPHABET.find(key[i]);
        if (pos == std::string::npos) return false; 
        
        if (pos < ALPHABET.length() - 1) {
            key[i] = ALPHABET[pos + 1];
            return true;
        } else {
            key[i] = ALPHABET[0];
        }
    }
    if (key.length() < 32) {
        key.insert(key.begin(), ALPHABET[0]);
        return true;
    }
    return false;
}

WorkerClient::WorkerClient(std::string host, int port, int hashrate) 
    : host_(std::move(host)), port_(port), hashrate_(hashrate), sock_(-1), 
      task_found_(false), task_completed_(false) {}

WorkerClient::~WorkerClient() {
    if (sock_ != -1) close(sock_);
}

void WorkerClient::send_message(const std::string& msg) {
    std::string payload = msg + "\n";
    send(sock_, payload.c_str(), payload.length(), 0);
}

std::string WorkerClient::read_line() {
    std::string line;
    char c;
    while (!g_shutdown_requested) {
        ssize_t res = recv(sock_, &c, 1, 0);
        if (res > 0) {
            if (c == '\n') break;
            if (c != '\r') line += c;
        } else if (res == 0) {
            std::cerr << "[!] Сервер закрыл соединение.\n";
            g_shutdown_requested = true;
            break;
        } else {
            if (errno == EINTR) continue;
            std::cerr << "[!] Ошибка сети.\n";
            g_shutdown_requested = true;
            break;
        }
    }
    return line;
}

void WorkerClient::worker_thread() {
    const int CHUNK_SIZE = 10000;

    while (!task_found_ && !g_shutdown_requested) {
        std::string local_start, local_end;
        int count = 0;

        {
            std::lock_guard<std::mutex> lock(task_mutex_);
            if (current_key_.empty() || current_key_ > end_key_ || task_completed_) {
                break; 
            }
            
            local_start = current_key_;
            while (count < CHUNK_SIZE && current_key_ <= end_key_) {
                local_end = current_key_;
                if (!increment_key(current_key_)) break;
                count++;
            }
        }

        std::string key = local_start;
        for (int i = 0; i < count; ++i) {
            if (task_found_ || g_shutdown_requested) break;
            
            if (target_hash_func(key) == target_hash_) {
                task_found_ = true;
                found_key_ = key;
                break;
            }
            
            if (key == local_end) break;
            increment_key(key);
        }
    }
}

void WorkerClient::process_task(const std::string& hash, const std::string& start, const std::string& end) {
    target_hash_ = hash;
    current_key_ = start;
    end_key_ = end;
    task_found_ = false;
    task_completed_ = false;
    found_key_ = "";

    std::cout << "[*] Получена задача. Диапазон: [" << start << " ; " << end << "]\n";

    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;
    
    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < num_threads; ++i) {
        threads.emplace_back(&WorkerClient::worker_thread, this);
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    task_completed_ = true;

    if (g_shutdown_requested) return;

    if (task_found_) {
        std::cout << "[+] Ключ найден: " << found_key_ << "\n";
        send_message("FOUND " + found_key_);
    } else {
        std::cout << "[-] Ключ не найден в диапазоне.\n";
        send_message("NOT_FOUND");
    }
}

void WorkerClient::connect_to_server() {
    sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ < 0) throw std::runtime_error("Ошибка создания сокета");

    struct hostent* server = gethostbyname(host_.c_str());
    if (server == nullptr) throw std::runtime_error("Хост не найден");

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char*)server->h_addr, (char*)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(port_);

    if (connect(sock_, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        throw std::runtime_error("Ошибка подключения к серверу");
    }

    std::cout << "[*] Успешное подключение к " << host_ << ":" << port_ << "\n";
    send_message("READY " + std::to_string(hashrate_));
}

void WorkerClient::run() {
    connect_to_server();

    while (!g_shutdown_requested) {
        std::string cmd_line = read_line();
        if (cmd_line.empty() || g_shutdown_requested) break;

        std::istringstream iss(cmd_line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "TASK") {
            std::string hash, start, end;
            if (iss >> hash >> start >> end) {
                process_task(hash, start, end);
            } else {
                std::cerr << "[!] Ошибка: неверный формат команды TASK\n";
            }
        } else if (cmd == "WAIT") {
            int seconds = 0;
            iss >> seconds;
            std::cout << "[*] Команда WAIT. Ожидание " << seconds << " сек...\n";
            sleep(seconds);
            send_message("READY " + std::to_string(hashrate_));
        } else if (cmd == "DONE") {
            std::cout << "[*] Получена команда DONE. Завершение работы...\n";
            break;
        } else {
            std::cerr << "[!] Неизвестная команда: " << cmd << "\n";
        }
    }
}