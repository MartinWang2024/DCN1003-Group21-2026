#include "test.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <iomanip>
#include "log.h"

// ─────────────────────────────────────────────
// CourseRepository 测试
// ─────────────────────────────────────────────
TEST(test_time)
{
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cout << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") << std::endl;
}
TEST(test_now_time)
{
    std::cout << get_now_time() << std::endl;
}
TEST(test_log)
{
    print_log("This is a debug message.", debug);
    print_log("This is an info message.", info);
    print_log("This is a warning message.", warn);
    print_log("This is an error message.", error);
}

// ─────────────────────────────────────────────
// main
// ─────────────────────────────────────────────
int main() {
    std::cout << "=== timetest ===\n";
    RUN(test_time);
    RUN(test_now_time);
    RUN(test_log);


    std::cout << "\n--- Result: "
              << s_passed << " passed, "
              << s_failed << " failed ---\n";

    return s_failed == 0 ? 0 : 1;
}
