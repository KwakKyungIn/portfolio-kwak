#include "pch.h"
#include "MetricsRunner.h"
#include "Metrics.h"
#include <atomic>
#include <thread>
#include <chrono>

// ============================================
// MetricsRunner
// - 1�ʸ��� TickAndPrint1s() ȣ���ϴ� ���� ������
// - ���� ���� �� StartMetricsTicker()�� ����
//   ���� �� StopMetricsTicker()�� �����ϰ� ����
// ============================================

static std::atomic<bool> g_run{ false }; // ���� ���� �÷���
static std::thread g_thr;                // ��� ���� ������

// ------------------------------
// StartMetricsTicker()
// - 1�� �ֱ� ����͸� ����
// ------------------------------
void StartMetricsTicker() {
    if (g_run.exchange(true)) return; // �̹� ���� ���̸� ����
    g_thr = std::thread([] {
        while (g_run.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            GMetrics.TickAndPrint1s(); // 1�ʸ��� ���
        }
        });
}

// ------------------------------
// StopMetricsTicker()
// - ����͸� ������ ���� ����
// ------------------------------
void StopMetricsTicker() {
    if (!g_run.exchange(false)) return;
    if (g_thr.joinable()) g_thr.join();
}
