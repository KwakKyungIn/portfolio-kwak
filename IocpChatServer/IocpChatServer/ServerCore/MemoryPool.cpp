#include "pch.h"
#include "MemoryPool.h"

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
    }
    else
    {
        ASSERT_CRASH(memory->allocSize == 0); // �̹� ��� ���� �ƴϾ�� ��
        _reserveCount.fetch_sub(1);
    }

    _useCount.fetch_add(1);

    return memory;
}
