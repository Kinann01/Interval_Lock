#ifndef INTERVAL_LOCK_LOCKER_HPP
#define INTERVAL_LOCK_LOCKER_HPP

#include <iostream>
#include <mutex>
#include <condition_variable>
#include "interval_tree.hpp"

class locker;
class exclusive_lock;

class shared_lock {

public:

    using interval = std::pair<std::size_t , std::size_t>;

    shared_lock() noexcept {
        p_MainLocker = nullptr;
        interval_={0,0};
    }

    explicit shared_lock(locker* ptr_main_locker, interval interval) {
        p_MainLocker = ptr_main_locker;
        interval_ = std::move(interval);
    }

    shared_lock(const shared_lock&) = delete; // no support for copy
    shared_lock& operator=(const shared_lock&) = delete; // no support for copy

    shared_lock(shared_lock&&) noexcept; // move, invalidate source object
    shared_lock& operator=(shared_lock&&) noexcept; // unlock `*this` (if not invalid), move, invalidate source object

    ~shared_lock(); // unlock (if not invalid), noexcept by default

    void unlock() noexcept;  // unlock (if not invalid), invalidate
    exclusive_lock upgrade();   // BLOCKING, upgrade to exclusive_lock, invalidate `*this`

private:
    locker* p_MainLocker;
    interval interval_;

};

class exclusive_lock {
public:

    using interval = std::pair<std::size_t , std::size_t>;


    exclusive_lock() noexcept {
        p_MainLocker = nullptr;
        interval_={0,0};
    }

    explicit exclusive_lock(locker* ptr_main_locker, interval interval){
        p_MainLocker = ptr_main_locker;
        interval_ = std::move(interval);
    }

    exclusive_lock(const exclusive_lock&) = delete; // no support for copy
    exclusive_lock& operator=(const exclusive_lock&) = delete; // no support for copy

    exclusive_lock(exclusive_lock&&) noexcept; // move, invalidate source object
    exclusive_lock& operator=(exclusive_lock&&) noexcept; // unlock `*this` (if not invalid), move, invalidate source object

    ~exclusive_lock(); // unlock (if not invalid), noexcept by default
    void unlock() noexcept;

    shared_lock downgrade() noexcept;   // downgrade to shared_lock, invalidate `*this`

private:
    locker* p_MainLocker;
    interval interval_;

};

class locker {
public:


    // Allow class exclusive_lock and class shared_lock to access the private members/methods of this class.
    friend class exclusive_lock;
    friend class shared_lock;

    using size_type = std::size_t;

    // Interval node that will be added to the interval tree
    struct LockInfo{

        // Counter to keep track of the reference count of an interval
        std::size_t counter;

        // is_exclusive used to determine if a lock is exclusive or shared
        bool is_exclusive;
    };

    locker() = default;

    locker(const locker&) = delete;
    locker(locker&&) = delete;
    locker& operator=(const locker&) = delete;
    locker& operator=(locker&&) = delete;

    ~locker(){

        // Wait until the tree is empty.
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return inter_tree.empty(); });
    }

    shared_lock lock_shared(size_type b, size_type e) {

        std::unique_lock<std::mutex> lock(mtx);
        // Wait until we can acquire a shared lock
        cv.wait(lock, [&] { return can_acquire_shared_lock(b, e); });
        auto it = inter_tree.find({b, e});

        // If the interval is already in the tree, then increment the reference counter since we can have multiple shared locks over an interval
        if (it != inter_tree.end()){
            it->value.counter++;
        }
        else{

            // If it is not in the tree, then create a new interval node and add it to the tree.
            LockInfo new_shared_lock{1, false};
            inter_tree.emplace({b, e}, new_shared_lock);
        }

        return shared_lock(this , {b, e});
    }

    exclusive_lock lock_exclusive(size_type b, size_type e){

        std::unique_lock<std::mutex> lock(mtx);

        // Wait until we can acquire an exclusive lock
        cv.wait(lock, [&]{ return can_acquire_exclusive_lock(b, e);});

        // Similarly, we create an interval node, and we add it to the tree.
        LockInfo new_exclusive_lock{1, true};

        // We add it again since at every unlock the corresponding interval is erased from the tree.
        inter_tree.emplace({b, e}, new_exclusive_lock);

        return exclusive_lock(this , {b, e});
    }

private:

    std::mutex mtx;
    std::condition_variable cv;
    interval_tree<LockInfo> inter_tree;

    bool can_acquire_shared_lock(size_type b, size_type e, bool ignore_self=false){
        
        // Neither of the overlaps is exclusive == true
        auto overlapping_nodes = inter_tree.get_overlaps({b, e}, ignore_self);
        for (const auto &node : overlapping_nodes){
            if (node->value.is_exclusive){
                return false;
            }
        }
        return true;
    }

    // Not a single overlap == true
    bool can_acquire_exclusive_lock(size_type b, size_type e){
        return inter_tree.get_overlap({b, e}) == inter_tree.end();
    }


    void unlock_shared(size_type b, size_type e){

        std::unique_lock<std::mutex> lock(mtx);

        // Find the interval in the interval tree, if it is there, decrease the counter
        // if it is there and the counter became 0, then this is the last shared_lock. Therefore, we can erase the interval
        // Then we let our condition variable wake up all the threads to check whose predicate is satisfied in order to take over this interval (if there are any).
        auto it = inter_tree.find({b, e});

        if (it != inter_tree.end()){
            it->value.counter--;

            if (it->value.counter == 0){
                inter_tree.erase({b,e});
            }
        }

        cv.notify_all();
    }

    void unlock_exclusive(size_type b, size_type e){
        // Nothing to do with counters since 1 exclusive lock over 1 particular interval
        // Just erase it from the tree and notify_all()
        std::unique_lock<std::mutex> lock(mtx);
        inter_tree.erase({b,e});
        cv.notify_all();
    }



    // Downgrade from exclusive to locked.
    shared_lock actual_downgrade(size_type b, size_type e){

        std::unique_lock<std::mutex> lock(mtx);

        // Wait until we can acquire a shared lock by making sure no exclusive lock is over that interval
        cv.wait(lock, [&] { return can_acquire_shared_lock(b, e, true); });

        // change the is_exclusive to false;
        // counter remains 1
        auto it = inter_tree.find({b, e});
        // it can not be it.end() since we are downgrading,
        it->value.is_exclusive = false;

        // Return
        return shared_lock(this, {b, e});
    }


    exclusive_lock actual_upgrade(size_type b, size_type e){

        std::unique_lock<std::mutex> lock(mtx);

        //
        cv.wait(lock, [&]{

            // Find the interval in the tree
            auto it = inter_tree.find({ b, e});


            // If counter == 1, and no overlaps occur over this interval (excluding self) then return we can upgrade to exclusive.
            return it->value.counter == 1
                   && inter_tree.get_overlap({b, e}, true)
                      == inter_tree.end();
        });

        // Tree doesn't preserve references.
        // So we need to find it again
        auto it = inter_tree.find({b, e});

        // it can not be it.end() since we are upgrading
        // Set is_exclusive to true since we are upgrading
        // Counter remains = 1
        it->value.is_exclusive = true;

        // return
        return exclusive_lock(this, {b, e});

    }

};

//// --------
// EXCLUSIVE LOCK METHODS

//exclusive_lock(exclusive_lock&&) noexcept; // move, invalidate source object
//exclusive_lock& operator=(exclusive_lock&&) noexcept; // unlock `*this` (if not invalid), move, invalidate source object
//
//~exclusive_lock(); // unlock (if not invalid), noexcept by default
//void unlock() noexcept;
//
//shared_lock downgrade() noexcept;   // downgrade to shared_lock, invalidate `*this`

exclusive_lock::exclusive_lock(exclusive_lock &&other) noexcept {
    p_MainLocker = other.p_MainLocker;
    interval_ = other.interval_;
    other.p_MainLocker = nullptr;
    other.interval_ = {0, 0};
}

exclusive_lock &exclusive_lock::operator=(exclusive_lock &&other) noexcept {
    if (this != &other) {
        if (p_MainLocker) {
            p_MainLocker->unlock_exclusive(interval_.first, interval_.second);
        }

        p_MainLocker = other.p_MainLocker;
        interval_ = other.interval_;
        other.p_MainLocker = nullptr;
        other.interval_ = {0, 0};
    }
    return *this;
}

exclusive_lock::~exclusive_lock() {

    if (p_MainLocker != nullptr) {
        p_MainLocker->unlock_exclusive(interval_.first, interval_.second);
    }
}

void exclusive_lock::unlock() noexcept {
    if (p_MainLocker != nullptr) {
        p_MainLocker->unlock_exclusive(interval_.first, interval_.second);
        p_MainLocker = nullptr;
    }
}

shared_lock exclusive_lock::downgrade() noexcept {

    shared_lock result;
    if (p_MainLocker != nullptr){
        result = p_MainLocker->actual_downgrade(interval_.first, interval_.second);
    }

    p_MainLocker->cv.notify_all();
    p_MainLocker = nullptr;

    return result;
}

//// --------
// SHARED LOCK METHODS

//shared_lock(shared_lock&&) noexcept; // move, invalidate source object
//shared_lock& operator=(shared_lock&&) noexcept; // unlock `*this` (if not invalid), move, invalidate source object
//
//~shared_lock(); // unlock (if not invalid), noexcept by default
//
//void unlock() noexcept;  // unlock (if not invalid), invalidate
//exclusive_lock upgrade();   // BLOCKING, upgrade to exclusive_lock, invalidate `*this`

shared_lock::shared_lock(shared_lock &&other ) noexcept {
    p_MainLocker = other.p_MainLocker;
    interval_ = other.interval_;
    other.p_MainLocker = nullptr;
    other.interval_ = {0, 0};
}

shared_lock &shared_lock::operator=(shared_lock &&other) noexcept {
    if (this != &other) {
        if (p_MainLocker) {
            p_MainLocker->unlock_shared(interval_.first, interval_.second);
        }

        p_MainLocker = other.p_MainLocker;
        interval_ = other.interval_;
        other.p_MainLocker = nullptr;
        other.interval_ = {0, 0};
    }
    return *this;
}

shared_lock::~shared_lock() {
    if (p_MainLocker != nullptr) {
        p_MainLocker->unlock_shared(interval_.first, interval_.second);
    }
}

void shared_lock::unlock() noexcept {

    if (p_MainLocker != nullptr) {
        p_MainLocker->unlock_shared(interval_.first, interval_.second);
        p_MainLocker = nullptr;
    }
}

exclusive_lock shared_lock::upgrade() {

    exclusive_lock result;
    if (p_MainLocker != nullptr){
        result = p_MainLocker->actual_upgrade(interval_.first, interval_.second);
        p_MainLocker = nullptr;
    }

    return result;
}

#endif //INTERVAL_LOCK_LOCKER_HPP