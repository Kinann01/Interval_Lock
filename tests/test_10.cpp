#include <chrono>
#include <iostream>
#include <semaphore>
#include <thread>
#include <vector>

#include "locker.hpp"

/*
    This test checks whether upgrades and downgrades work as intended.
*/

void test_unit(locker &locker_, std::size_t line, std::pair<std::size_t, std::size_t> interval1, std::pair<std::size_t, std::size_t> interval2) {
    using namespace std::chrono_literals;
    std::binary_semaphore semaphore(0);

    std::thread x;

    std::jthread thread;
    exclusive_lock lock = locker_.lock_exclusive(interval1.first, interval1.second);

    thread = (std::jthread)[&locker_, &semaphore, interval2, line](std::stop_token stoken) {

        semaphore.release();

        exclusive_lock lock2 = locker_.lock_exclusive(interval2.first, interval2.second);

        if (!stoken.stop_requested()) {
            // HERE
            std::cerr << "FAILURE:" << line << std::endl;
            exit(EXIT_FAILURE);
        }
    };

    semaphore.acquire();
    std::this_thread::sleep_for(100ms); // give the `thread` time to fail

    for (std::size_t i = 0; i < 1000; ++i) {
        // do this for a long time so the `thread` is likely to fail
        // if a failure is possible

        auto shared_lock = lock.downgrade();
        lock = shared_lock.upgrade();
    }

    thread.request_stop();
}

void run_test() {
    using namespace std::chrono_literals;
    locker locker_;

    // |---|
    // |---|
    test_unit(locker_, __LINE__, {0, 1000}, {0, 1000});

    // |---|
    //   |-|
    test_unit(locker_, __LINE__, {0, 1000}, {500, 1000});

    //   |-|
    // |---|
    test_unit(locker_, __LINE__, {500, 1000}, {0, 1000});

    // |---|
    // |-|
    test_unit(locker_, __LINE__, {0, 1000}, {0, 500});

    // |-|
    // |---|
    test_unit(locker_, __LINE__, {0, 500}, {0, 1000});

    // |---|
    //   |---|
    test_unit(locker_, __LINE__, {0, 1000}, {500, 1500});

    //   |---|
    // |---|
    test_unit(locker_, __LINE__, {500, 1000}, {0, 1000});

    //  |-|
    // |---|
    test_unit(locker_, __LINE__, {500, 1000}, {0, 1500});

    // |---|
    //  |-|
    test_unit(locker_, __LINE__, {0, 1500}, {500, 1000});

    //0   |---|

    //1 |---|
    //2   |---|
    //3     |---|
    //4   |-|
    //5     |-|
    //6    |-|
    //7  |-----|
    auto lock = locker_.lock_exclusive(500, 2000);

    auto thread = std::jthread([&locker_](std::stop_token stoken) {
        auto lock1 = locker_.lock_shared(0, 1500);
        auto lock2 = locker_.lock_shared(500, 2000);
        auto lock3 = locker_.lock_shared(1000, 3000);
        auto lock4 = locker_.lock_shared(500, 1000);
        auto lock5 = locker_.lock_shared(1000, 2000);
        auto lock6 = locker_.lock_shared(1500, 1750);
        auto lock7 = locker_.lock_shared(250, 2500);

        if (!stoken.stop_requested()) {
            std::cerr << "FAILURE:" << __LINE__ << std::endl;
            exit(EXIT_FAILURE);
        }
    });

    std::this_thread::sleep_for(100ms); // give the thread time to fail
    thread.request_stop();

    auto downgraded = lock.downgrade(); // lock no longer blocks others

    // `thread` joins and (downgraded) `lock` unlocks (in this exact order)
    thread.join();
}

int main() {
    run_test();
    std::cout << "OK" << std::endl;
}
