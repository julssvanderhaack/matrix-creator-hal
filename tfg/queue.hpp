// FILE   : queue.hpp
// AUTHOR : Julio Albisua
// INFO   : SafeQueue utils  with some special functions like wait_pop
//          Designed for multithreading and trying to be a package lossless tool

#ifndef UTILS_HPP
#define UTILS_HPP
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class SafeQueue {
public:
    std::atomic_bool run_async{true};
private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
public:
    void push(const T& item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(item);
        }
        cond_.notify_one();
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        item = queue_.front();
        queue_.pop();
        return true;
    }

    int wait_pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        // Note that we can have spureus wakeups, in addition to notified ones.
        // That means that this may not work perfectly, but it should generally
        // work. And even if this wakes up we have the return values to
        // communicate the situation.
        cond_.wait(lock, [&] {
            // Exit the wait when the queue has data or we are no longer
            // running. If we don't use run_async, we can have a deadlock if we
            // are waiting and the threads are no longer producing (running =
            // false;). This way we wake up when running == false and return 0.
            return queue_.empty() == false || run_async == false;
        });
        if(queue_.empty()) {
            // If the queue if empty, return 0 to indicate we have no more values.
            // This is only possible if we are woken up with run_async == false.
            return 0;
        }
        item = queue_.front();
        queue_.pop();
        if(!run_async) {
            // Return -1 if we have stopped producing, but we have more data in the queue.
            return -1;
        }
        // Return 1 if we are producing more values, and we have data in the queue.
        return 1;
    }

    bool empty() {
        std::unique_lock<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    void start_async() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            run_async = true;
        }
        cond_.notify_all();
    }

    void stop_async() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            run_async = false;
        }
        cond_.notify_all();
    }


};

struct AudioBlock {
    std::vector<std::vector<int16_t>> samples;
};

#endif
