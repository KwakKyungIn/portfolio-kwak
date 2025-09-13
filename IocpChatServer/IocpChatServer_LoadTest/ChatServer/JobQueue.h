#pragma once
#include <vector>
#include <deque>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

#include "Metrics.h"// [METRICS] 전역 성능 지표 집계용

class JobQueue {
public:
    using Job = std::function<void()>; // 작업을 함수 객체 형태로 저장

    // 생성자: maxQueueSize == 0이면 무제한 큐
    JobQueue(size_t maxQueueSize = 0)
        : stop_(false), maxQueueSize_(maxQueueSize) {
    }

    // 소멸자: Stop() 호출해서 안전하게 스레드 정리
    ~JobQueue() {
        Stop();
    }

    // 스레드 풀 시작: threadCount 개수만큼 워커 스레드 실행
    void Start(size_t threadCount = std::thread::hardware_concurrency()) {
        stop_ = false;
        for (size_t i = 0; i < threadCount; ++i) {
            // 각 워커는 WorkerLoop() 실행
            workers_.emplace_back([this]() {
                WorkerLoop();
                });
        }
    }

    // 모든 스레드 종료
    void Stop() {
        {
            std::unique_lock<std::mutex> lk(mtx_); // 락 잡고
            stop_ = true;                          // 종료 플래그 세팅
            cv_.notify_all();                      // 대기중인 스레드 깨우기
        }
        for (auto& t : workers_) if (t.joinable()) t.join(); // join으로 깔끔히 마무리
        workers_.clear();
    }

    // 블로킹 enqueue: 큐가 가득 차면 기다림
    template<typename F, typename... Args>
    auto Enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>
    {
        using Ret = typename std::result_of<F(Args...)>::type; // 반환 타입 추론
        // packaged_task로 비동기 실행 캡슐화
        auto taskPtr = std::make_shared<std::packaged_task<Ret()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<Ret> res = taskPtr->get_future();

        {
            std::unique_lock<std::mutex> lk(mtx_);
            if (maxQueueSize_ > 0) {
                // 큐가 꽉 차있으면 공간 날 때까지 wait
                cvFull_.wait(lk, [this]() { return stop_ || tasks_.size() < maxQueueSize_; });
                if (stop_) throw std::runtime_error("JobQueue stopped");
            }
            // 태스크를 람다로 감싸서 deque에 push
            tasks_.emplace_back([taskPtr]() { (*taskPtr)(); });

            // [METRICS] 현재 큐 크기, 피크 업데이트
            uint32_t sz = static_cast<uint32_t>(tasks_.size());
            GMetrics.jobs_enqueued.fetch_add(1, std::memory_order_relaxed);
            GMetrics.jobqueue_gauge.store(sz, std::memory_order_relaxed);
            uint32_t prev = GMetrics.jobqueue_peak.load(std::memory_order_relaxed);
            while (sz > prev && !GMetrics.jobqueue_peak.compare_exchange_weak(prev, sz, std::memory_order_relaxed)) {}
        }
        cv_.notify_one(); // 대기중인 워커 스레드 깨우기
        return res;       // future 반환 (결과 받을 수 있음)
    }

    // 비블로킹 enqueue: 꽉 차 있으면 false 반환
    bool TryEnqueue(Job job) {
        std::unique_lock<std::mutex> lk(mtx_);
        if (stop_) return false;                              // 이미 정지 상태
        if (maxQueueSize_ > 0 && tasks_.size() >= maxQueueSize_) return false; // 꽉 찼으면 실패
        tasks_.push_back(std::move(job));                     // 바로 push
        cv_.notify_one();

        // [METRICS] 현재 큐 크기, 피크 업데이트
        GMetrics.jobs_enqueued.fetch_add(1, std::memory_order_relaxed);
        uint32_t sz = static_cast<uint32_t>(tasks_.size());
        GMetrics.jobqueue_gauge.store(sz, std::memory_order_relaxed);
        uint32_t prev = GMetrics.jobqueue_peak.load(std::memory_order_relaxed);
        while (sz > prev && !GMetrics.jobqueue_peak.compare_exchange_weak(prev, sz, std::memory_order_relaxed)) {}

        return true;
    }

    // 현재 큐 사이즈 반환 (락 잡고 확인)
    size_t Size() const {
        std::unique_lock<std::mutex> lk(mtx_);
        return tasks_.size();
    }

private:
    // 워커 스레드가 실행하는 루프
    void WorkerLoop() {
        while (true) {
            Job job;
            {
                std::unique_lock<std::mutex> lk(mtx_);
                // 큐가 비어있으면 cv wait, stop_ 되면 종료
                cv_.wait(lk, [this]() { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) return; // 종료 조건
                job = std::move(tasks_.front());     // 맨 앞 작업 꺼내오기
                tasks_.pop_front();

                // [METRICS] pop 후 남은 큐 길이 갱신
                uint32_t sz = static_cast<uint32_t>(tasks_.size());
                GMetrics.jobqueue_gauge.store(sz, std::memory_order_relaxed);
            }
            try {
                job(); // 실제 작업 실행

                // [METRICS] 실행 완료 카운트
                GMetrics.jobs_executed.fetch_add(1, std::memory_order_relaxed);
            }
            catch (const std::exception& e) {
                // 작업 중 에러 → 로그 출력
                std::cerr << "[JobQueue Error] " << e.what() << std::endl;
            }
            catch (...) {
                // 알 수 없는 예외는 일단 무시 (TODO: 로깅 필요)
            }
        }
    }

    // 내부 상태
    std::vector<std::thread> workers_;   // 워커 스레드들
    std::deque<Job> tasks_;              // 작업 대기열
    mutable std::mutex mtx_;             // 공유 자원 보호용 뮤텍스
    std::condition_variable cv_;         // 작업 대기 CV
    std::condition_variable cvFull_;     // 큐 꽉찼을 때 대기 CV
    std::atomic<bool> stop_;             // 종료 플래그
    size_t maxQueueSize_;                // 최대 큐 크기 (0이면 무제한)
};
