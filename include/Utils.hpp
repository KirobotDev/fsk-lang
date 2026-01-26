#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

template <typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue;
    std::mutex mutex;
    std::condition_variable cond;
    bool closed = false;

public:
    void push(T item) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            queue.push(item);
        }
        cond.notify_one();
    }

    std::optional<T> pop(bool block = true) {
        std::unique_lock<std::mutex> lock(mutex);
        if (!block) {
            if (queue.empty()) return std::nullopt;
            T item = queue.front();
            queue.pop();
            return item;
        }
        
        cond.wait(lock, [this] { return !queue.empty() || closed; });
        if (queue.empty() && closed) return std::nullopt;
        
        T item = queue.front();
        queue.pop();
        return item;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            closed = true;
        }
        cond.notify_all();
    }
};
