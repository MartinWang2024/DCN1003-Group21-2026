#pragma once
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "error.h"



enum log_level
{
    debug = 0,
    info = 1,
    warn = 2,
    error = 3,
};

time_t get_now_time();
std::_Put_time<char> get_time();

void print_log(const std::string& log, log_level level = debug);

void print_log(const Error::ErrorInfo&, log_level level = debug);

template<typename... Args>
void print_log(log_level level, const char* fmt, Args&&... args)
{
    std::cout << get_time();

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

    char buffer[1024];
    snprintf(buffer, sizeof(buffer), fmt, std::forward<Args>(args)...);
    std::cout << buffer << std::endl;
}