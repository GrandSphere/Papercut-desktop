#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

namespace papercut {

// One MuPDF thread. Priority: control, urgent render, then prefetch.
class EngineWorker {
public:
    using Job = std::function<void()>;

    EngineWorker();
    ~EngineWorker();
    EngineWorker(const EngineWorker &) = delete;
    EngineWorker &operator=(const EngineWorker &) = delete;

    void post(Job job);
    void postBackground(Job job);
    void postRender(int index, bool urgent, Job job);
    void cancelRendersExcept(int first, int last);
    void cancelAllRenders();
    void drainControl();

private:
    void run();

    struct RenderJob {
        bool urgent = false;
        unsigned long long seq = 0;
        Job fn;
    };

    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop = false;
    bool m_preferBackground = false;
    unsigned long long m_seq = 0;
    std::deque<Job> m_control;
    std::deque<Job> m_background;
    std::map<int, RenderJob> m_renders;
};

} // namespace papercut
