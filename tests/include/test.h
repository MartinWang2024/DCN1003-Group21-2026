#pragma once

// ─────────────────────────────────────────────
// TestFrame
// ─────────────────────────────────────────────
static int s_passed = 0;
static int s_failed = 0;

#define TEST(name) static void name()
#define RUN(name)                                                       \
do {                                                                \
try {                                                           \
name();                                                     \
std::cout << "  [PASS] " #name "\n";                       \
++s_passed;                                                 \
} catch (const std::exception& e) {                            \
std::cout << "  [FAIL] " #name " : " << e.what() << "\n"; \
++s_failed;                                                 \
}                                                               \
} while (0)

#define REQUIRE(expr)                                                \
do {                                                             \
if (!(expr))                                                 \
throw std::runtime_error("REQUIRE failed: " #expr);     \
} while (0)
