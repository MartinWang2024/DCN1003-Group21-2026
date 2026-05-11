#pragma once
#include <chrono>
#include <iostream>
#include <iomanip>
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