#include <iostream>
#include <cstring>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <chrono>
#include <openssl/evp.h>

namespace
{
    constexpr int LISTEN_PORT     = 8080;
    constexpr int MAX_CONNECTIONS = 5;
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

class SocketException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class Socket
{
public:
    Socket(int fd) : fd_(fd) {
        if (fd_ < 0) throw SocketException("invalid fd provided");
    }

    Socket() : fd_(::socket(AF_INET, SOCK_STREAM, 0)) {
        if (fd_ < 0) throw SocketException("failed to create socket");
    }

    ~Socket() { close(); }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }
    
    bool write(const char* str) {
        return write(str, strlen(str)); 
    }

    bool write(const std::string& str) {
        return write(str.data(), str.size());
    }
    
    bool write(const char* data, size_t size) {
        ssize_t bytes_sent = ::send(fd_, data, size, 0);
        if (bytes_sent < 0)
            std::cerr << "error sending message: " << strerror(errno) << std::endl;
        return bytes_sent >= 0;
    }

    std::string read_line() {
        std::string line;
        char c;
        while (true) {
            ssize_t bytes_read = ::recv(fd_, &c, 1, 0);
            if (bytes_read > 0) {
                if (c == '\n') break;
                if (c != '\r') line += c;
            } else if (bytes_read == 0) {
                break;
            } else {
                if (errno == EINTR) continue;
                std::cerr << "error reading message: " << strerror(errno) << std::endl;
                break;
            }
        }
        return line;
    }

    int get() const { return fd_; }

private:
    int fd_ = -1;
};

class Server
{
public:
    Server(int port = LISTEN_PORT) : listen_port(port) {
        int opt = 1;
        std::memset(&addr_, 0, sizeof(addr_));

        addr_.sin_family      = AF_INET;
        addr_.sin_addr.s_addr = INADDR_ANY;
        addr_.sin_port        = htons(listen_port);

        setsockopt(socket_.get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        if (::bind(socket_.get(), reinterpret_cast<struct sockaddr*>(&addr_), sizeof(addr_)) < 0)
            throw SocketException("failed to bind socket");

        if (::listen(socket_.get(), MAX_CONNECTIONS) < 0)
            throw SocketException("failed to listen on socket");
    }

    int port() const { return listen_port; }

    void run() {
        while (true) waitForConnection();
    }

private:
    void waitForConnection() {
        sockaddr_in client_addr = {};
        socklen_t sizeof_addr = sizeof(client_addr);

        int client_fd = ::accept(socket_.get(), reinterpret_cast<struct sockaddr*>(&client_addr), &sizeof_addr);

        if (client_fd >= 0) { 
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            // Используем std::endl для принудительного сброса буфера!
            std::cout << "[*] client connected: " << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;
            
            Socket client_socket(client_fd);
            
            std::string client_msg = client_socket.read_line();
            std::cout << "[Клиент -> Сервер] " << client_msg << std::endl;

            if (client_msg.find("READY") == 0) {
                std::this_thread::sleep_for(std::chrono::seconds(1));

                std::string secret_key = "ABC";
                std::string target_hash = sha1_hex(md5_hex(secret_key));
                
                std::string start_key = "AAA";
                std::string end_key = "ZZZ";

                std::string task_msg = "TASK " + target_hash + " " + start_key + " " + end_key + "\n";
                std::cout << "[Сервер -> Клиент] TASK " << target_hash << " " << start_key << " " << end_key << std::endl;
                client_socket.write(task_msg);

                std::string result_msg = client_socket.read_line();
                std::cout << "[Клиент -> Сервер] " << result_msg << std::endl;

                std::this_thread::sleep_for(std::chrono::seconds(1));

                std::string done_msg = "DONE\n";
                std::cout << "[Сервер -> Клиент] DONE" << std::endl;
                client_socket.write(done_msg);
            }
                
            std::cout << "[*] connection closed" << std::endl;
        } else {
            std::cerr << "failed to accept connection: " << strerror(errno) << std::endl;
        }
    }

    int listen_port = 0;
    Socket socket_;
    sockaddr_in addr_ = {};
};

int main(int argc, char* argv[])
{
    try {
        Server server(argc > 1 ? std::atoi(argv[1]) : LISTEN_PORT);
        std::cout << "[*] server started[" << server.port() << "]. Awaiting connections..." << std::endl;
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}