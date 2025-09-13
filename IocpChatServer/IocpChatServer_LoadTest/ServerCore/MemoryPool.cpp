#include "pch.h"
#include "MemoryPool.h"
#include "Metrics.h"

// 메모리 풀 생성자
MemoryPool::MemoryPool(int32 allocSize) : _allocSize(allocSize)
{
    // SLIST 초기화 (윈도우 lock-free 단일 연결 리스트)
    ::InitializeSListHead(&_header);
}

// 메모리 풀 소멸자
MemoryPool::~MemoryPool()
{
    // 남은 메모리 모두 해제
    while (MemoryHeader* memory = static_cast<MemoryHeader*>(::InterlockedPopEntrySList(&_header)))
        ::_aligned_free(memory);
}

// 메모리를 풀에 반납
void MemoryPool::Push(MemoryHeader* ptr)
{
    // allocSize=0으로 표시 (사용 안 함)
    ptr->allocSize = 0;

    // lock-free push
    ::InterlockedPushEntrySList(&_header, static_cast<PSLIST_ENTRY>(ptr));

    // 메트릭: inuse--, free_total++
    GMetrics.mpool_inuse_gauge.fetch_sub(1, std::memory_order_relaxed);
    GMetrics.mpool_free_total.fetch_add(1, std::memory_order_relaxed);

    // 내부 카운터 갱신
    _useCount.fetch_sub(1);
    _reserveCount.fetch_add(1);
}

// 메모리를 풀에서 꺼냄
MemoryHeader* MemoryPool::Pop()
{
    MemoryHeader* memory = static_cast<MemoryHeader*>(::InterlockedPopEntrySList(&_header));

    // 없으면 새로 할당
    if (memory == nullptr)
    {
        memory = reinterpret_cast<MemoryHeader*>(::_aligned_malloc(_allocSize, SLIST_ALIGNMENT));
        GMetrics.mpool_alloc_total.fetch_add(1, std::memory_order_relaxed);
    }
    else
    {
        ASSERT_CRASH(memory->allocSize == 0); // 이미 사용 중이 아니어야 함
        _reserveCount.fetch_sub(1);
    }

    _useCount.fetch_add(1);

    // 메트릭: inuse++ 및 피크 갱신
    uint32_t inuse = GMetrics.mpool_inuse_gauge.fetch_add(1, std::memory_order_relaxed) + 1;
    uint32_t prev = GMetrics.mpool_inuse_peak.load(std::memory_order_relaxed);
    while (inuse > prev && !GMetrics.mpool_inuse_peak.compare_exchange_weak(prev, inuse, std::memory_order_relaxed)) {}

    return memory;
}
