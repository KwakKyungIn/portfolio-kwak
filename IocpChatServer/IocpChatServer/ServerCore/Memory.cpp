#include "pch.h"
#include "Memory.h"
#include "MemoryPool.h"

// �޸� Ǯ �ʱ�ȭ
Memory::Memory()
{
    int32 size = 0;
    int32 tableIndex = 0;

    // 32 ���� Ǯ (~1024����)
    for (size = 32; size <= 1024; size += 32)
    {
        MemoryPool* pool = new MemoryPool(size);
        _pools.push_back(pool);

        // �ش� ũ�� ������ ��� allocSize�� �� Ǯ�� ���
        while (tableIndex <= size)
        {
            _poolTable[tableIndex] = pool;
            tableIndex++;
        }
    }

    // 128 ���� Ǯ (~2048����)
    for (; size <= 2048; size += 128)
    {
        MemoryPool* pool = new MemoryPool(size);
        _pools.push_back(pool);

        while (tableIndex <= size)
        {
            _poolTable[tableIndex] = pool;
            tableIndex++;
        }
    }

    // 256 ���� Ǯ (~4096����)
    for (; size <= 4096; size += 256)
    {
        MemoryPool* pool = new MemoryPool(size);
        _pools.push_back(pool);

        while (tableIndex <= size)
        {
            _poolTable[tableIndex] = pool;
            tableIndex++;
        }
    }
}

// �޸� Ǯ �Ҹ��� (Ǯ ����)
Memory::~Memory()
{
    for (MemoryPool* pool : _pools)
        delete pool;

    _pools.clear();
}

// �޸� �Ҵ�
void* Memory::Allocate(int32 size)
{
    MemoryHeader* header = nullptr;
    const int32 allocSize = size + sizeof(MemoryHeader);

#ifdef _STOMP
    // StompAllocator: ������ (overflow ����)
    header = reinterpret_cast<MemoryHeader*>(StompAllocator::Alloc(allocSize));
#else
    if (allocSize > MAX_ALLOC_SIZE)
    {
        // Ǯ ũ�� �ʰ� �� �Ϲ� malloc
        header = reinterpret_cast<MemoryHeader*>(::_aligned_malloc(allocSize, SLIST_ALIGNMENT));
    }
    else
    {
        // Ǯ���� �޸� ��������
        header = _poolTable[allocSize]->Pop();
    }
#endif    

    // [MemoryHeader][����� ������] ��ȯ
    return MemoryHeader::AttachHeader(header, allocSize);
}

// �޸� ����
void Memory::Release(void* ptr)
{
    MemoryHeader* header = MemoryHeader::DetachHeader(ptr);

    const int32 allocSize = header->allocSize;
    ASSERT_CRASH(allocSize > 0);

#ifdef _STOMP
    // StompAllocator: ������
    StompAllocator::Release(header);
#else
    if (allocSize > MAX_ALLOC_SIZE)
    {
        // Ǯ ũ�� �ʰ� �� �Ϲ� free
        ::_aligned_free(header);
    }
    else
    {
        // �ٽ� �޸� Ǯ�� �ݳ�
        _poolTable[allocSize]->Push(header);
    }
#endif    
}
