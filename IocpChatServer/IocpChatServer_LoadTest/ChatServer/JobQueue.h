#pragma once
#include <vector>
#include <deque>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

#include "Metrics.h"// [METRICS] ���� ���� ��ǥ �����

class JobQueue {
public:
    using Job = std::function<void()>; // �۾��� �Լ� ��ü ���·� ����

    // ������: maxQueueSize == 0�̸� ������ ť
    JobQueue(size_t maxQueueSize = 0)
        : stop_(false), maxQueueSize_(maxQueueSize) {
    }

    // �Ҹ���: Stop() ȣ���ؼ� �����ϰ� ������ ����
    ~JobQueue() {
        Stop();
    }

    // ������ Ǯ ����: threadCount ������ŭ ��Ŀ ������ ����
    void Start(size_t threadCount = std::thread::hardware_concurrency()) {
        stop_ = false;
        for (size_t i = 0; i < threadCount; ++i) {
            // �� ��Ŀ�� WorkerLoop() ����
            workers_.emplace_back([this]() {
                WorkerLoop();
                });
        }
    }

    // ��� ������ ����
    void Stop() {
        {
            std::unique_lock<std::mutex> lk(mtx_); // �� ���
            stop_ = true;                          // ���� �÷��� ����
            cv_.notify_all();                      // ������� ������ �����
        }
        for (auto& t : workers_) if (t.joinable()) t.join(); // join���� ����� ������
        workers_.clear();
    }

    // ���ŷ enqueue: ť�� ���� ���� ��ٸ�
    template<typename F, typename... Args>
    auto Enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>
    {
        using Ret = typename std::result_of<F(Args...)>::type; // ��ȯ Ÿ�� �߷�
        // packaged_task�� �񵿱� ���� ĸ��ȭ
        auto taskPtr = std::make_shared<std::packaged_task<Ret()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<Ret> res = taskPtr->get_future();

        {
            std::unique_lock<std::mutex> lk(mtx_);
            if (maxQueueSize_ > 0) {
                // ť�� �� �������� ���� �� ������ wait
                cvFull_.wait(lk, [this]() { return stop_ || tasks_.size() < maxQueueSize_; });
                if (stop_) throw std::runtime_error("JobQueue stopped");
            }
            // �½�ũ�� ���ٷ� ���μ� deque�� push
            tasks_.emplace_back([taskPtr]() { (*taskPtr)(); });

            // [METRICS] ���� ť ũ��, ��ũ ������Ʈ
            uint32_t sz = static_cast<uint32_t>(tasks_.size());
            GMetrics.jobs_enqueued.fetch_add(1, std::memory_order_relaxed);
            GMetrics.jobqueue_gauge.store(sz, std::memory_order_relaxed);
            uint32_t prev = GMetrics.jobqueue_peak.load(std::memory_order_relaxed);
            while (sz > prev && !GMetrics.jobqueue_peak.compare_exchange_weak(prev, sz, std::memory_order_relaxed)) {}
        }
        cv_.notify_one(); // ������� ��Ŀ ������ �����
        return res;       // future ��ȯ (��� ���� �� ����)
    }

    // ����ŷ enqueue: �� �� ������ false ��ȯ
    bool TryEnqueue(Job job) {
        std::unique_lock<std::mutex> lk(mtx_);
        if (stop_) return false;                              // �̹� ���� ����
        if (maxQueueSize_ > 0 && tasks_.size() >= maxQueueSize_) return false; // �� á���� ����
        tasks_.push_back(std::move(job));                     // �ٷ� push
        cv_.notify_one();

        // [METRICS] ���� ť ũ��, ��ũ ������Ʈ
        GMetrics.jobs_enqueued.fetch_add(1, std::memory_order_relaxed);
        uint32_t sz = static_cast<uint32_t>(tasks_.size());
        GMetrics.jobqueue_gauge.store(sz, std::memory_order_relaxed);
        uint32_t prev = GMetrics.jobqueue_peak.load(std::memory_order_relaxed);
        while (sz > prev && !GMetrics.jobqueue_peak.compare_exchange_weak(prev, sz, std::memory_order_relaxed)) {}

        return true;
    }

    // ���� ť ������ ��ȯ (�� ��� Ȯ��)
    size_t Size() const {
        std::unique_lock<std::mutex> lk(mtx_);
        return tasks_.size();
    }

private:
    // ��Ŀ �����尡 �����ϴ� ����
    void WorkerLoop() {
        while (true) {
            Job job;
            {
                std::unique_lock<std::mutex> lk(mtx_);
                // ť�� ��������� cv wait, stop_ �Ǹ� ����
                cv_.wait(lk, [this]() { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) return; // ���� ����
                job = std::move(tasks_.front());     // �� �� �۾� ��������
                tasks_.pop_front();

                // [METRICS] pop �� ���� ť ���� ����
                uint32_t sz = static_cast<uint32_t>(tasks_.size());
                GMetrics.jobqueue_gauge.store(sz, std::memory_order_relaxed);
            }
            try {
                job(); // ���� �۾� ����

                // [METRICS] ���� �Ϸ� ī��Ʈ
                GMetrics.jobs_executed.fetch_add(1, std::memory_order_relaxed);
            }
            catch (const std::exception& e) {
                // �۾� �� ���� �� �α� ���
                std::cerr << "[JobQueue Error] " << e.what() << std::endl;
            }
            catch (...) {
                // �� �� ���� ���ܴ� �ϴ� ���� (TODO: �α� �ʿ�)
            }
        }
    }

    // ���� ����
    std::vector<std::thread> workers_;   // ��Ŀ �������
    std::deque<Job> tasks_;              // �۾� ��⿭
    mutable std::mutex mtx_;             // ���� �ڿ� ��ȣ�� ���ؽ�
    std::condition_variable cv_;         // �۾� ��� CV
    std::condition_variable cvFull_;     // ť ��á�� �� ��� CV
    std::atomic<bool> stop_;             // ���� �÷���
    size_t maxQueueSize_;                // �ִ� ť ũ�� (0�̸� ������)
};
