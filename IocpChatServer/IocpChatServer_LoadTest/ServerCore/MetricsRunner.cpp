#include "pch.h"
#include "MetricsRunner.h"
#include "Metrics.h"
#include <atomic>
#include <thread>
#include <chrono>

// ============================================
// MetricsRunner
// - 1초마다 TickAndPrint1s() 호출하는 별도 쓰레드
// - 서버 실행 시 StartMetricsTicker()로 시작
//   종료 시 StopMetricsTicker()로 안전하게 정리
// ============================================

static std::atomic<bool> g_run{ false }; // 실행 여부 플래그
static std::thread g_thr;                // 출력 전용 쓰레드

// ------------------------------
// StartMetricsTicker()
// - 1초 주기 모니터링 시작
// ------------------------------
void StartMetricsTicker() {
    if (g_run.exchange(true)) return; // 이미 실행 중이면 무시
    g_thr = std::thread([] {
        while (g_run.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            GMetrics.TickAndPrint1s(); // 1초마다 출력
        }
        });
}

// ------------------------------
// StopMetricsTicker()
// - 모니터링 쓰레드 안전 종료
// ------------------------------
void StopMetricsTicker() {
    if (!g_run.exchange(false)) return;
    if (g_thr.joinable()) g_thr.join();
}
