#include <chrono>
#include <iostream>
#include <semaphore>
#include <thread>

#include "locker.hpp"

/*
    This fails on some hacky solutions.
*/

template<class FirstLock, class SecondLock, std::size_t N>
void test_unit(locker &locker_, std::size_t line, std::pair<std::size_t, std::size_t> interval1, const std::pair<std::size_t, std::size_t> (&intervals)[N]) {
    using namespace std::chrono_literals;
    std::counting_semaphore<N> semaphore(0);

    std::vector<std::jthread> threads;
    FirstLock lock;

    if constexpr (std::is_same_v<FirstLock, exclusive_lock>)
        lock = locker_.lock_exclusive(interval1.first, interval1.second);
    else /* if constexpr (std::is_same_v<FirstLock, shared_lock>) */
        lock = locker_.lock_shared(interval1.first, interval1.second);

    threads.reserve(N);

    for (auto interval : intervals) {
        threads.emplace_back([&locker_, &semaphore, interval, line](std::stop_token stoken) {

            SecondLock lock;

            semaphore.release();

            if constexpr (std::is_same_v<SecondLock, exclusive_lock>)
                lock = locker_.lock_exclusive(interval.first, interval.second);
            else /* if constexpr (std::is_same_v<SecondLock, shared_lock>) */
                lock = locker_.lock_shared(interval.first, interval.second);

            if (!stoken.stop_requested()) {
                std::cerr << "FAILURE:" << line << std::endl;
                exit(EXIT_FAILURE);
            }
        });
    }

    for (std::size_t counter = 0; counter < N; ++counter)
        semaphore.acquire(); // everything prepared

    std::this_thread::sleep_for(100ms); // give `threads` time to fail

    for (auto &&thread : threads) {
        thread.request_stop();
    }
}

template<class FirstLock, class SecondLock>
void multi_test(locker &locker_) {
    //  |-----------------|
    // |1| |3| |-5-| |4| |2|
    test_unit<FirstLock, SecondLock>(locker_, __LINE__, {50, 1000}, {{0, 100}, {950, 1050}, {100, 200}, {850, 950}, {300, 600}});

    //  |-----------------|
    //  |-----------------|
    //    |-------------|
    //      |---------|
    //        |-----|
    //          |-|
    test_unit<FirstLock, SecondLock>(locker_, __LINE__, {0, 1000}, {{0, 1000}, {100, 900}, {200, 800}, {300, 700}, {400, 600}});
}

void run_test() {
    locker locker_;
    multi_test<exclusive_lock, exclusive_lock>(locker_);
    multi_test<shared_lock, exclusive_lock>(locker_);
    multi_test<exclusive_lock, shared_lock>(locker_);
}

int main() {
    run_test();
    std::cout << "OK" << std::endl;
}
