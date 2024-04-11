#include <algorithm>
#include <chrono>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "locker.hpp"
#include "interval_tree.hpp"

/*
    This test spawns a lot of threads (some generate shared locks and some
    generate exclusive locks).

    The shared locks always persist for some time (the generating thread
    always keeps 10 last generated locks) and exclusive locks are always
    unlocked before generating a new one.
*/

void run_test() {
    using namespace std::chrono_literals;

    locker locker_;
    std::vector<std::jthread> threads;
    constexpr std::size_t thread_count = 1000;

    std::size_t max = 100'000;
    std::size_t min_size = 1;
    std::size_t max_size = 1000;

    std::size_t shared_count = 10;

    auto duration = 5000ms;
    auto tolerance = 200ms;

    std::counting_semaphore<thread_count> red_semaphore(0);
    std::counting_semaphore<thread_count> green_semaphore(0);

    auto thread_spawns_shared = [](std::size_t i) -> bool { return i & 1; };

    auto lock = locker_.lock_exclusive(0, max);

    for (std::size_t i = 0; i < thread_count; ++i) {
        threads.emplace_back([=, &locker_,  &red_semaphore, &green_semaphore, spawns_shared = thread_spawns_shared(i)](std::stop_token stoken) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<> dis(0., 1.);

            std::vector<std::pair<std::size_t, std::size_t>> intervals;

            if (spawns_shared) {
                std::vector<shared_lock> shared_locks(shared_count);

                red_semaphore.release();
                green_semaphore.acquire();

                std::size_t n = 0;
                while(!stoken.stop_requested()) {
                    ++n;

                    std::size_t size = min_size + std::size_t(dis(gen) * (max_size - min_size));
                    std::size_t beg = std::size_t(dis(gen) * (max - size));
                    std::pair interval{beg, beg + size};

                    shared_locks[n % shared_count] = locker_.lock_shared(interval.first, interval.second);
                }
            } else {
                exclusive_lock lock;

                red_semaphore.release();
                green_semaphore.acquire();

                while(!stoken.stop_requested()) {
                    std::size_t size = min_size + std::size_t(dis(gen) * (max_size - min_size));
                    std::size_t beg = std::size_t(dis(gen) * (max - size));
                    std::pair interval{beg, beg + size};

                    lock.unlock();
                    lock = locker_.lock_exclusive(interval.first, interval.second);
                }
            }
        });
    }

    for (std::size_t counter = 0; counter < thread_count; ++counter)
        red_semaphore.acquire();
    
    green_semaphore.release(thread_count);

    auto start = std::chrono::high_resolution_clock::now();

    lock.unlock();

    std::this_thread::sleep_for(duration);
    for (auto &&thread : threads) {
        thread.request_stop();
    }

    for (auto &&th : threads) {
        th.join();
    }

    threads.clear();

    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // should take something a little bit over `duration`
    if (dur < duration || dur > duration + tolerance) {
        std::cerr << "FAILURE" << std::endl;
        exit(EXIT_FAILURE);
    }
}

int main() {
    run_test();
    std::cout << "OK" << std::endl;
}
