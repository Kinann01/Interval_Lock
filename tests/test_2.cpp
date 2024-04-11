#include <chrono>
#include <iostream>
#include <semaphore>
#include <thread>
#include <vector>

#include "locker.hpp"

/*
    This test just checks single overlaps.

    We also reuse the same locker and use similar intervals in each test,
        failures can be caused by non-removed locks as well.
*/

template<class FirstLock, class SecondLock>
void test_unit(locker &locker_, std::size_t line, std::pair<std::size_t, std::size_t> interval1, std::pair<std::size_t, std::size_t> interval2) {
    using namespace std::chrono_literals;
    std::binary_semaphore semaphore(0);

    std::jthread thread;
    FirstLock lock;

    if constexpr (std::is_same_v<FirstLock, exclusive_lock>)
        lock = locker_.lock_exclusive(interval1.first, interval1.second);
    else /* if constexpr (std::is_same_v<FirstLock, shared_lock>) */
        lock = locker_.lock_shared(interval1.first, interval1.second);

    thread = (std::jthread)[&locker_, &semaphore, interval2, line](std::stop_token stoken) {
        SecondLock lock2;

        semaphore.release();

        if constexpr (std::is_same_v<SecondLock, exclusive_lock>)
            lock2 = locker_.lock_exclusive(interval2.first, interval2.second);
        else /* if constexpr (std::is_same_v<SecondLock, shared_lock>) */
            lock2 = locker_.lock_shared(interval2.first, interval2.second);

        if (!stoken.stop_requested()) {
            std::cerr << "FAILURE:" << line << std::endl;
            exit(EXIT_FAILURE);
        }
    };

    semaphore.acquire(); // everything prepared
    std::this_thread::sleep_for(100ms); // give `thread` time to fail

    thread.request_stop();
    lock.unlock();
}

template<class FirstLock, class SecondLock>
void multi_test(locker &locker_) {

    // full overlap:

    // |---|
    // |---|
    test_unit<FirstLock, SecondLock>(locker_, __LINE__, {0, 1000}, {0, 1000});

    // half-full overlaps:

    // |---|
    //   |-|
    test_unit<FirstLock, SecondLock>(locker_, __LINE__, {0, 1000}, {500, 1000});

    //   |-|
    // |---|
    test_unit<FirstLock, SecondLock>(locker_, __LINE__, {500, 1000}, {0, 1000});

    // |---|
    // |-|
    test_unit<FirstLock, SecondLock>(locker_, __LINE__, {0, 1000}, {0, 500});

    // |-|
    // |---|
    test_unit<FirstLock, SecondLock>(locker_, __LINE__, {0, 500}, {0, 1000});

    // stair overlaps:

    // |---|
    //   |---|
    test_unit<FirstLock, SecondLock>(locker_, __LINE__, {0, 1000}, {500, 1500});

    //   |---|
    // |---|
    test_unit<FirstLock, SecondLock>(locker_, __LINE__, {500, 1500}, {0, 1000});

    // super overlaps:

    // |----|
    //  |--|
    test_unit<FirstLock, SecondLock>(locker_, __LINE__, {0, 2000}, {500, 1500});

    //  |--|
    // |----|
    test_unit<FirstLock, SecondLock>(locker_, __LINE__, {500, 1500}, {0, 2000});
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
