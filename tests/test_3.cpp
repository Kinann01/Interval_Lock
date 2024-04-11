#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "locker.hpp"

/*
    This test checks for transitively blocked exclusive locks,
    blocking is not intended to be transitive (if a creation
    of a lock is currently blocked, it cannot block any other lock).
*/

void run_test() {
    using namespace std::chrono_literals;

    locker locker_;
    std::size_t count = 1000;
    std::size_t half_width = 500;
    auto wait_time = 200ms;
    auto tolerance = 100ms;

    std::vector<std::thread> threads;
    threads.reserve(count);

    auto start = std::chrono::high_resolution_clock::now();

    // |---|
    //   |---|
    //     |---|
    //       |---|
    // ... (random shuffled)
    for (std::size_t i = 0; i < count; ++i) {
        threads.emplace_back([&locker_, offset = (i * 7 % 1000) * half_width, width = 2 * half_width, wait_time](){
            auto lock = locker_.lock_exclusive(offset, offset + width);
            std::this_thread::sleep_for(wait_time);
        });
    }

    for (auto &&th : threads) {
        th.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    threads.clear();

    // should take either something a little bit over `(2 * wait_time)` or `(3 * wait_time)`
    // (both are equally correct, first is very unlikely)
    if (dur < 2 * wait_time || // too soon
        (dur < 3 * wait_time && dur > 2 * wait_time + tolerance) || // too late for 2*
        dur > 3 * wait_time + tolerance // too late
    ) {
        std::cerr << "FAILURE" << std::endl;
        exit(EXIT_FAILURE);
    }
}

int main() {
    run_test();
    std::cout << "OK" << std::endl;
}
