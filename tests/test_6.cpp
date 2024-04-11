#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "locker.hpp"

/*
    If there are multiple shared locks blocked by an exclusive lock,
    its unlock/destruction should unlock all of the shared locks.
*/

void run_test() {
    using namespace std::chrono_literals;

    locker locker_;
    std::size_t count = 100;
    std::size_t width = 1000;
    auto wait_time = 200ms;
    auto tolerance = 100ms;

    std::vector<std::thread> threads;
    threads.reserve(count);

    auto lock = locker_.lock_exclusive(0, count * width);


    // |------------------------------------------------|
    // |-|
    //   |-|
    //     |-|
    // ...
    //                                                |-|
    for (std::size_t i = 0; i < count; ++i) {
        threads.emplace_back([&locker_, offset = i * width, width, wait_time](){
            auto lock = locker_.lock_exclusive(offset, offset + width);
            std::this_thread::sleep_for(wait_time);
        });
    }

    auto start = std::chrono::high_resolution_clock::now();

    lock.unlock();

    for (auto &&th : threads) {
        th.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    threads.clear();

    // should take something a little bit over `wait_time`
    if (dur < wait_time || dur > wait_time + tolerance) {
        std::cerr << "FAILURE" << std::endl;
        exit(EXIT_FAILURE);
    }
}

int main() {
    run_test();
    std::cout << "OK" << std::endl;
}
