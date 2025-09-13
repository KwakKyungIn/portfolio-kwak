#include "pch.h"
#include "Memory.h"
#include "MemoryPool.h"

// 메모리 풀 초기화
Memory::Memory()
{
    int32 size = 0;
    int32 tableIndex = 0;

    // 32 단위 풀 (~1024까지)
    for (size = 32; size <= 1024; size += 32)
    {
        MemoryPool* pool = new MemoryPool(size);
        _pools.push_back(pool);

        // 해당 크기 이하의 모든 allocSize는 이 풀을 사용
        while (tableIndex <= size)
        {
            _poolTable[tableIndex] = pool;
            tableIndex++;
        }
    }

    // 128 단위 풀 (~2048까지)
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

    // 256 단위 풀 (~4096까지)
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

// 메모리 풀 소멸자 (풀 해제)
Memory::~Memory()
{
    for (MemoryPool* pool : _pools)
        delete pool;

    _pools.clear();
}

// 메모리 할당
void* Memory::Allocate(int32 size)
{
    MemoryHeader* header = nullptr;
    const int32 allocSize = size + sizeof(MemoryHeader);

#ifdef _STOMP
    // StompAllocator: 디버깅용 (overflow 검출)
    header = reinterpret_cast<MemoryHeader*>(StompAllocator::Alloc(allocSize));
#else
    if (allocSize > MAX_ALLOC_SIZE)
    {
        // 풀 크기 초과 → 일반 malloc
        header = reinterpret_cast<MemoryHeader*>(::_aligned_malloc(allocSize, SLIST_ALIGNMENT));
    }
    else
    {
        // 풀에서 메모리 가져오기
        header = _poolTable[allocSize]->Pop();
    }
#endif    

    // [MemoryHeader][사용자 데이터] 반환
    return MemoryHeader::AttachHeader(header, allocSize);
}

// 메모리 해제
void Memory::Release(void* ptr)
{
    MemoryHeader* header = MemoryHeader::DetachHeader(ptr);

    const int32 allocSize = header->allocSize;
    ASSERT_CRASH(allocSize > 0);

#ifdef _STOMP
    // StompAllocator: 디버깅용
    StompAllocator::Release(header);
#else
    if (allocSize > MAX_ALLOC_SIZE)
    {
        // 풀 크기 초과 → 일반 free
        ::_aligned_free(header);
    }
    else
    {
        // 다시 메모리 풀로 반납
        _poolTable[allocSize]->Push(header);
    }
#endif    
}
