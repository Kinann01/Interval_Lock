#include <chrono>
#include <iostream>
#include <semaphore>
#include <thread>
#include <vector>

#include "locker.hpp"

/*
    This is an advanced version of test_2.
    It checks whether a creation of unique lock unblocks prematurely. More specifically,
    if it is blocked by two shared locks, it should wait for both to be unlocked.
*/

template<bool Reverse>
void test_unit(locker &locker_, std::size_t line, std::pair<std::size_t, std::size_t> interval1, std::pair<std::size_t, std::size_t> interval2, std::pair<std::size_t, std::size_t> interval3) {
    using namespace std::chrono_literals;
    std::binary_semaphore semaphore(0);

    std::jthread thread;
    shared_lock lock1 = locker_.lock_shared(interval1.first, interval1.second);
    shared_lock lock2 = locker_.lock_shared(interval2.first, interval2.second);

    thread = (std::jthread)[&locker_, &semaphore, interval3, line](std::stop_token stoken) {
        semaphore.release();

        exclusive_lock lock = locker_.lock_exclusive(interval3.first, interval3.second);

        if (!stoken.stop_requested()) {
            std::cerr << "FAILURE:" << line << std::endl;
            exit(EXIT_FAILURE);
        }
    };

    semaphore.acquire();

    if constexpr (Reverse) {
        lock2.unlock();
    } else {
        lock1.unlock();
    }

    std::this_thread::sleep_for(100ms); // give `thread` time to fail

    thread.request_stop();
}

void run_test() {
    locker locker_;

    // full overlap:

    // |---| <- unlock first
    // |---|
    // |---|
    test_unit<false>(locker_, __LINE__, {0, 1000}, {0, 1000}, {0, 1000});

    // |---|
    // |---| <- unlock first
    // |---|
    test_unit<true>(locker_, __LINE__, {0, 1000}, {0, 1000}, {0, 1000});

    // half-full overlaps:

    // |---| <- unlock first
    // |---|
    //   |-|
    test_unit<false>(locker_, __LINE__, {0, 1000}, {0, 1000}, {500, 1000});

    // |---|
    // |---| <- unlock first
    //   |-|
    test_unit<true>(locker_, __LINE__, {0, 1000}, {0, 1000}, {500, 1000});

    // |---| <- unlock first
    // |-|
    // |-|
    test_unit<false>(locker_, __LINE__, {0, 1000}, {500, 1000}, {500, 1000});

    // |---|
    // |-| <- unlock first
    // |-|
    test_unit<true>(locker_, __LINE__, {0, 1000}, {500, 1000}, {500, 1000});

    // |---| <- unlock first
    // |---|
    // |-|
    test_unit<false>(locker_, __LINE__, {0, 1000}, {0, 1000}, {0, 500});

    // |---|
    // |---| <- unlock first
    // |-|
    test_unit<true>(locker_, __LINE__, {0, 1000}, {0, 1000}, {0, 500});

    // |---| <- unlock first
    // |-|
    // |-|
    test_unit<false>(locker_, __LINE__, {0, 1000}, {0, 500}, {0, 500});

    // |---|
    // |-| <- unlock first
    // |-|
    test_unit<true>(locker_, __LINE__, {0, 1000}, {0, 500}, {0, 500});

    // super overlaps:

    // |------| <- unlock first
    //  |----|
    //   |--|
    test_unit<false>(locker_, __LINE__, {0, 2000}, {250, 1750}, {500, 1500});

    //   |--| <- unlock first
    //  |----|
    // |------|
    test_unit<false>(locker_, __LINE__, {500, 1500}, {250, 1750}, {0, 2000});
}

int main() {
    run_test();
    std::cout << "OK" << std::endl;
}
