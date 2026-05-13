#include "log.h"

time_t get_now_time()
{
    time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    return now;
}

std::_Put_time<char> get_time()
{
    time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    return std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S ");
}

static void print_level(const log_level level)
{
    switch (level)
    {
    case debug:
        std::cout << "[DEBUG] ";
        break;
    case info:
        std::cout << "[INFO] ";
        break;
    case warn:
        std::cout << "[WARN] ";
        break;
    case error:
        std::cout << "[ERROR] ";
        break;
    }
}

void print_log(const std::string& log, const log_level level)
{
    std::cout << get_time();
    print_level(level);
    std::cout << log << std::endl;
}

void print_log(const Error::ErrorInfo& err, const log_level level)
{
    std::cout << get_time();
    print_level(level);
    std::cout << err.message << std::endl;
}