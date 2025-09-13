#include "pch.h"
#include "MemoryPool.h"
#include "Metrics.h"

// �޸� Ǯ ������
MemoryPool::MemoryPool(int32 allocSize) : _allocSize(allocSize)
{
    // SLIST �ʱ�ȭ (������ lock-free ���� ���� ����Ʈ)
    ::InitializeSListHead(&_header);
}

// �޸� Ǯ �Ҹ���
MemoryPool::~MemoryPool()
{
    // ���� �޸� ��� ����
    while (MemoryHeader* memory = static_cast<MemoryHeader*>(::InterlockedPopEntrySList(&_header)))
        ::_aligned_free(memory);
}

// �޸𸮸� Ǯ�� �ݳ�
void MemoryPool::Push(MemoryHeader* ptr)
{
    // allocSize=0���� ǥ�� (��� �� ��)
    ptr->allocSize = 0;

    // lock-free push
    ::InterlockedPushEntrySList(&_header, static_cast<PSLIST_ENTRY>(ptr));

    // ��Ʈ��: inuse--, free_total++
    GMetrics.mpool_inuse_gauge.fetch_sub(1, std::memory_order_relaxed);
    GMetrics.mpool_free_total.fetch_add(1, std::memory_order_relaxed);

    // ���� ī���� ����
    _useCount.fetch_sub(1);
    _reserveCount.fetch_add(1);
}

// �޸𸮸� Ǯ���� ����
MemoryHeader* MemoryPool::Pop()
{
    MemoryHeader* memory = static_cast<MemoryHeader*>(::InterlockedPopEntrySList(&_header));

    // ������ ���� �Ҵ�
    if (memory == nullptr)
    {
        memory = reinterpret_cast<MemoryHeader*>(::_aligned_malloc(_allocSize, SLIST_ALIGNMENT));
        GMetrics.mpool_alloc_total.fetch_add(1, std::memory_order_relaxed);
    }
    else
    {
        ASSERT_CRASH(memory->allocSize == 0); // �̹� ��� ���� �ƴϾ�� ��
        _reserveCount.fetch_sub(1);
    }

    _useCount.fetch_add(1);

    // ��Ʈ��: inuse++ �� ��ũ ����
    uint32_t inuse = GMetrics.mpool_inuse_gauge.fetch_add(1, std::memory_order_relaxed) + 1;
    uint32_t prev = GMetrics.mpool_inuse_peak.load(std::memory_order_relaxed);
    while (inuse > prev && !GMetrics.mpool_inuse_peak.compare_exchange_weak(prev, inuse, std::memory_order_relaxed)) {}

    return memory;
}
