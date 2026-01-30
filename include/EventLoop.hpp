#pragma once
#include <functional>
#include <vector>
#include <queue>
#include <set>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <thread>
#include <atomic>

struct TimerTask {
    int id;
    std::function<void()> callback;
    std::chrono::steady_clock::time_point executeAt;
    bool repeat;
    std::chrono::milliseconds interval;

    bool operator>(const TimerTask& other) const {
        return executeAt > other.executeAt;
    }
};

class EventLoop {
public:
    EventLoop() : stop(false), nextTimerId(1) {}

    void post(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            tasks.push(std::move(task));
        }
        cv.notify_one();
    }

    int setTimeout(std::function<void()> callback, std::chrono::milliseconds delay) {
        int id;
        {
            std::lock_guard<std::mutex> lock(mutex);
            id = nextTimerId++;
            timers.push({id, callback, std::chrono::steady_clock::now() + delay, false, delay});
        }
        cv.notify_one();
        return id;
    }

    int setInterval(std::function<void()> callback, std::chrono::milliseconds interval) {
        int id;
        {
            std::lock_guard<std::mutex> lock(mutex);
            id = nextTimerId++;
            timers.push({id, callback, std::chrono::steady_clock::now() + interval, true, interval});
        }
        cv.notify_one();
        return id;
    }

    void cancelTimer(int id) {
        std::lock_guard<std::mutex> lock(mutex);
        cancelledTimerIds.insert(id);
    }

    bool processOne(bool wait) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex);
            
            auto now = std::chrono::steady_clock::now();
            while (!timers.empty() && timers.top().executeAt <= now) {
                TimerTask t = timers.top();
                timers.pop();
                
                if (cancelledTimerIds.count(t.id)) {
                    cancelledTimerIds.erase(t.id);
                    continue;
                }

                lock.unlock();
                t.callback();
                lock.lock();

                if (t.repeat && !cancelledTimerIds.count(t.id)) {
                    t.executeAt = std::chrono::steady_clock::now() + t.interval;
                    timers.push(t);
                }
                now = std::chrono::steady_clock::now();
                return true; 
            }

            if (!tasks.empty()) {
                task = std::move(tasks.front());
                tasks.pop();
            } else {
                if (!wait) return false;

                if (timers.empty() && activeWorkCount == 0) {
                    return false;
                }

                if (timers.empty()) {
                    cv.wait(lock);
                } else {
                    cv.wait_until(lock, timers.top().executeAt);
                }
                return true; // Woke up, maybe work ready next spin
            }
        }

        if (task) {
            task();
            return true;
        }
        return false;
    }

    void run() {
        while (!stop) {
            if (!processOne(true)) {
                 if (activeWorkCount == 0 && tasks.empty() && timers.empty()) break;
            }
        }
    }

    void stopLoop() {
        stop = true;
        cv.notify_all();
    }

    void incrementWorkCount() { activeWorkCount++; }
    void decrementWorkCount() { 
        activeWorkCount--; 
        if (activeWorkCount == 0) cv.notify_all();
    }

private:
    std::queue<std::function<void()>> tasks;
    std::priority_queue<TimerTask, std::vector<TimerTask>, std::greater<TimerTask>> timers;
    std::set<int> cancelledTimerIds;
    int nextTimerId;
    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> stop;
    std::atomic<int> activeWorkCount{0};
};
