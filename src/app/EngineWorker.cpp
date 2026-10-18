#include "EngineWorker.h"

#include <future>

namespace papercut {

EngineWorker::EngineWorker() : m_thread([this] { run(); }) {}

EngineWorker::~EngineWorker() {
    {
        std::lock_guard lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

void EngineWorker::post(Job job) {
    {
        std::lock_guard lock(m_mutex);
        m_control.push_back(std::move(job));
    }
    m_cv.notify_one();
}

void EngineWorker::postBackground(Job job) {
    {
        std::lock_guard lock(m_mutex);
        m_background.push_back(std::move(job));
    }
    m_cv.notify_one();
}

void EngineWorker::postRender(int index, bool urgent, Job job) {
    {
        std::lock_guard lock(m_mutex);
        m_renders[index] = RenderJob{urgent, ++m_seq, std::move(job)};
    }
    m_cv.notify_one();
}

void EngineWorker::cancelRendersExcept(int first, int last) {
    std::lock_guard lock(m_mutex);
    for (auto it = m_renders.begin(); it != m_renders.end();) {
        if (it->first < first || it->first > last) it = m_renders.erase(it);
        else ++it;
    }
}

void EngineWorker::drainControl() {
    std::promise<void> done;
    std::future<void> f = done.get_future();
    post([&done] { done.set_value(); });
    f.wait();
}

void EngineWorker::cancelAllRenders() {
    std::lock_guard lock(m_mutex);
    m_renders.clear();
}

void EngineWorker::run() {
    for (;;) {
        Job job;
        {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, [this] {
                return m_stop || !m_control.empty() || !m_background.empty() || !m_renders.empty();
            });
            if (m_stop) return;
            if (!m_control.empty()) {
                job = std::move(m_control.front());
                m_control.pop_front();
            } else {
                auto urgent = m_renders.end();
                for (auto it = m_renders.begin(); it != m_renders.end(); ++it)
                    if (it->second.urgent && (urgent == m_renders.end() || it->second.seq > urgent->second.seq))
                        urgent = it;
                if (urgent != m_renders.end()) {
                    job = std::move(urgent->second.fn);
                    m_renders.erase(urgent);
                } else {
                    const bool haveRender = !m_renders.empty();
                    const bool haveBg = !m_background.empty();
                    if (haveBg && (!haveRender || m_preferBackground)) {
                        job = std::move(m_background.front());
                        m_background.pop_front();
                        m_preferBackground = false;
                    } else {
                        auto oldest = m_renders.begin();
                        for (auto it = m_renders.begin(); it != m_renders.end(); ++it)
                            if (it->second.seq < oldest->second.seq) oldest = it;
                        job = std::move(oldest->second.fn);
                        m_renders.erase(oldest);
                        m_preferBackground = true;
                    }
                }
            }
        }
        if (job) job();
    }
}

} // namespace papercut
