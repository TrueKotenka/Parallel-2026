#pragma once

#include <string>
#include <mutex>
#include <atomic>

extern std::atomic<bool> g_shutdown_requested;
void signal_handler(int signum);

class WorkerClient {
private:
    std::string host_;
    int port_;
    int hashrate_;
    int sock_;
    
    std::mutex task_mutex_;
    std::string current_key_;
    std::string end_key_;
    std::string target_hash_;
    
    std::atomic<bool> task_found_;
    std::atomic<bool> task_completed_;
    std::string found_key_;

    void send_message(const std::string& msg);
    std::string read_line();
    void worker_thread();
    void process_task(const std::string& hash, const std::string& start, const std::string& end);

public:
    WorkerClient(std::string host, int port, int hashrate);
    ~WorkerClient();

    void connect_to_server();
    void run();
};