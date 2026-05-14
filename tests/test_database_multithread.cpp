#include "database.h"
#include "test.h"

#include <atomic>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct TempDb {
    std::string path;

    explicit TempDb(const std::string& name) : path(name) {
        const fs::path db_path(path);
        if (db_path.has_parent_path()) {
            fs::create_directories(db_path.parent_path());
        }
        fs::remove(db_path);
        fs::remove(db_path.string() + "-wal");
        fs::remove(db_path.string() + "-shm");
    }

    ~TempDb() {
        fs::remove(path);
        fs::remove(path + "-wal");
        fs::remove(path + "-shm");
    }
};

void record_failure(std::mutex& failure_mutex,
                    std::vector<std::string>& failures,
                    const std::string& message) {
    std::lock_guard<std::mutex> lock(failure_mutex);
    failures.push_back(message);
}

void require_no_thread_failures(const std::vector<std::string>& failures) {
    if (!failures.empty()) {
        throw std::runtime_error(failures.front());
    }
}

}  // namespace

TEST(test_course_multithread_insert_with_separate_connections) {
    TempDb tmp("data/test_course_mt_insert.db");

    {
        dcn_database::CourseRepository repo(tmp.path);
        REQUIRE(repo.initialize_schema());
    }

    constexpr int kThreadCount = 8;
    constexpr int kInsertPerThread = 50;

    std::atomic<bool> start{false};
    std::mutex failure_mutex;
    std::vector<std::string> failures;
    std::vector<std::thread> threads;

    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([&, t]() {
            dcn_database::CourseRepository repo(tmp.path);
            if (!repo.initialize_schema()) {
                record_failure(failure_mutex, failures,
                               "initialize_schema failed in thread " + std::to_string(t) +
                                   ": " + repo.last_error());
                return;
            }

            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (int i = 0; i < kInsertPerThread; ++i) {
                dcn_database::Course c;
                c.code = "CSMT";
                c.title = "Multi-thread Database";
                c.section = "T" + std::to_string(t) + "_" + std::to_string(i);
                c.instructor = "Worker-" + std::to_string(t);
                c.day = "Monday";
                c.duration = "10:00-11:00";
                c.classroom = "Lab-" + std::to_string(t);

                if (!repo.insert_or_replace(c)) {
                    record_failure(failure_mutex, failures,
                                   "insert_or_replace failed in thread " + std::to_string(t) +
                                       ": " + repo.last_error());
                    return;
                }
            }
        });
    }

    start.store(true, std::memory_order_release);

    for (auto& thread : threads) {
        thread.join();
    }

    require_no_thread_failures(failures);

    dcn_database::CourseRepository verify(tmp.path);
    REQUIRE(verify.initialize_schema());

    const auto courses = verify.search_by_course_code("CSMT");
    REQUIRE(courses.size() == static_cast<std::size_t>(kThreadCount * kInsertPerThread));
}

TEST(test_course_multithread_read_write_with_separate_connections) {
    TempDb tmp("data/test_course_mt_read_write.db");

    {
        dcn_database::CourseRepository repo(tmp.path);
        REQUIRE(repo.initialize_schema());

        dcn_database::Course seed;
        seed.code = "BASE";
        seed.title = "Seed Course";
        seed.section = "A";
        seed.instructor = "Seeder";
        seed.day = "Tuesday";
        seed.duration = "08:00-09:00";
        seed.classroom = "R0";
        REQUIRE(repo.insert_or_replace(seed));
    }

    constexpr int kReaderCount = 4;
    constexpr int kWriterInsertCount = 120;
    constexpr int kReadRounds = 150;

    std::atomic<bool> start{false};
    std::mutex failure_mutex;
    std::vector<std::string> failures;
    std::vector<std::thread> readers;

    std::thread writer([&]() {
        dcn_database::CourseRepository repo(tmp.path);
        if (!repo.initialize_schema()) {
            record_failure(failure_mutex, failures,
                           "writer initialize_schema failed: " + repo.last_error());
            return;
        }

        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        for (int i = 0; i < kWriterInsertCount; ++i) {
            dcn_database::Course c;
            c.code = "RW";
            c.title = "Read/Write Stress";
            c.section = "S" + std::to_string(i);
            c.instructor = "Writer";
            c.day = "Wednesday";
            c.duration = "13:00-14:00";
            c.classroom = "RW-LAB";

            if (!repo.insert_or_replace(c)) {
                record_failure(failure_mutex, failures,
                               "writer insert_or_replace failed at round " + std::to_string(i) +
                                   ": " + repo.last_error());
                return;
            }
        }
    });

    for (int r = 0; r < kReaderCount; ++r) {
        readers.emplace_back([&, r]() {
            dcn_database::CourseRepository repo(tmp.path);
            if (!repo.initialize_schema()) {
                record_failure(failure_mutex, failures,
                               "reader initialize_schema failed in thread " + std::to_string(r) +
                                   ": " + repo.last_error());
                return;
            }

            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (int round = 0; round < kReadRounds; ++round) {
                const auto seed_courses = repo.search_by_course_code("BASE");
                if (seed_courses.size() != 1 || seed_courses[0].title != "Seed Course") {
                    record_failure(failure_mutex, failures,
                                   "reader observed inconsistent seed course in thread " +
                                       std::to_string(r) + " at round " + std::to_string(round));
                    return;
                }

                const auto all_courses = repo.view_all_courses();
                if (all_courses.empty()) {
                    record_failure(failure_mutex, failures,
                                   "reader observed empty database in thread " + std::to_string(r) +
                                       " at round " + std::to_string(round));
                    return;
                }
            }
        });
    }

    start.store(true, std::memory_order_release);

    writer.join();
    for (auto& reader : readers) {
        reader.join();
    }

    require_no_thread_failures(failures);

    dcn_database::CourseRepository verify(tmp.path);
    REQUIRE(verify.initialize_schema());
    REQUIRE(verify.search_by_course_code("BASE").size() == 1);
    REQUIRE(verify.search_by_course_code("RW").size() == static_cast<std::size_t>(kWriterInsertCount));
}

TEST(test_admin_multithread_insert_and_verify_with_separate_connections) {
    TempDb tmp("data/test_admin_mt.db");

    {
        dcn_database::AdministratorRepository repo(tmp.path);
        REQUIRE(repo.initialize_schema());
    }

    constexpr int kThreadCount = 6;

    std::atomic<bool> start{false};
    std::mutex failure_mutex;
    std::vector<std::string> failures;
    std::vector<std::thread> threads;
    std::vector<std::string> usernames;
    usernames.reserve(kThreadCount);

    for (int i = 0; i < kThreadCount; ++i) {
        usernames.push_back("admin_" + std::to_string(i));
    }

    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&, i]() {
            dcn_database::AdministratorRepository repo(tmp.path);
            if (!repo.initialize_schema()) {
                record_failure(failure_mutex, failures,
                               "admin initialize_schema failed in thread " + std::to_string(i) +
                                   ": " + repo.last_error());
                return;
            }

            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            dcn_database::Administrator admin;
            admin.username = usernames[static_cast<std::size_t>(i)];
            admin.password = "pw_" + std::to_string(i);

            if (!repo.insert_or_replace(admin)) {
                record_failure(failure_mutex, failures,
                               "admin insert_or_replace failed in thread " + std::to_string(i) +
                                   ": " + repo.last_error());
                return;
            }
        });
    }

    start.store(true, std::memory_order_release);

    for (auto& thread : threads) {
        thread.join();
    }

    require_no_thread_failures(failures);

    dcn_database::AdministratorRepository verify(tmp.path);
    REQUIRE(verify.initialize_schema());
    for (int i = 0; i < kThreadCount; ++i) {
        REQUIRE(verify.verify_login(usernames[static_cast<std::size_t>(i)], "pw_" + std::to_string(i)));
        REQUIRE(!verify.verify_login(usernames[static_cast<std::size_t>(i)], "wrong"));
    }
}

int main() {
    std::cout << "=== Multi-thread Database Tests ===\n\n";

    std::cout << "--- CourseRepository ---\n";
    RUN(test_course_multithread_insert_with_separate_connections);
    RUN(test_course_multithread_read_write_with_separate_connections);

    std::cout << "\n--- AdministratorRepository ---\n";
    RUN(test_admin_multithread_insert_and_verify_with_separate_connections);

    std::cout << "\n--- Result: "
              << s_passed << " passed, "
              << s_failed << " failed ---\n";

    return s_failed == 0 ? 0 : 1;
}

