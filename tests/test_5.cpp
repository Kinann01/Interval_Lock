#include <chrono>
#include <iostream>
#include <semaphore>
#include <thread>
#include <vector>

#include "locker.hpp"

/*
    This test checks whether ~locker really waits.
*/

void run_test() {
    using namespace std::chrono_literals;
    std::jthread thread;

    {
        locker locker_;
        auto semaphore = std::binary_semaphore(0);

        std::size_t width = 1'000'000;

        thread = (std::jthread)[&locker_, &semaphore, width](){
            std::vector<exclusive_lock> locks;

            for (std::size_t i = 0; i < 7000; i += 7) {
                std::size_t offset = (i % 1000) * width;

                // these shouldn't block
                locks.emplace_back(locker_.lock_exclusive(offset, offset + width));
            }

            semaphore.release(); // everything prepared
            std::this_thread::sleep_for(100ms); // give the main thread time to fail
        };

        semaphore.acquire();

        // we should be blocked here for a while
    }

    std::this_thread::sleep_for(100ms); // give `thread` time to fail

    thread.join();
}

int main(){
    run_test();
    std::cout << "OK" << std::endl;
}
