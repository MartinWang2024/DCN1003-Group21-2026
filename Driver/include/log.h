#pragma once
#include <chrono>
#include <iostream>
#include <iomanip>


enum log_level
{
    debug = 0,
    info = 1,
    warn = 2,
    error = 3,
};

std::_Put_time<char> get_time();

void print_log(const std::string& log, const log_level level = debug);