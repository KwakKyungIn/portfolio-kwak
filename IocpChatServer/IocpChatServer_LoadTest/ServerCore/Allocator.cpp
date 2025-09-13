#include "pch.h"
#include "Allocator.h"
#include "Memory.h"

// ------------------------------------
// BaseAllocator
//  - 가장 단순한 메모리 할당자
//  - malloc / free 그대로 사용
//  - 디버깅/성능 분석용으로 다른 Allocator와 비교할 때 쓰임
// ------------------------------------
void* BaseAllocator::Alloc(int32 size)
{
    return ::malloc(size);
}

void BaseAllocator::Release(void* ptr)
{
    ::free(ptr);
}

// ------------------------------------
// StompAllocator
//  - 메모리 오염(overflow/underflow) 디버깅용
//  - 페이지 단위로 할당해서, 잘못된 접근 시 바로 크래시 나도록 유도
//  - 보통 성능은 안 좋지만, 버그 잡을 때 유용
// ------------------------------------
void* StompAllocator::Alloc(int32 size)
{
    // 필요한 페이지 개수 계산
    const int64 pageCount = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    // 페이지 끝쪽에 데이터가 위치하도록 offset 계산
    const int64 dataOffset = pageCount * PAGE_SIZE - size;

    // Windows API로 가상 메모리 페이지 할당
    void* baseAddress = ::VirtualAlloc(NULL, pageCount * PAGE_SIZE,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    // offset 적용 후 실제 포인터 반환
    return static_cast<void*>(static_cast<int8*>(baseAddress) + dataOffset);
}

void StompAllocator::Release(void* ptr)
{
    // baseAddress = 페이지 단위 시작 주소
    const int64 address = reinterpret_cast<int64>(ptr);
    const int64 baseAddress = address - (address % PAGE_SIZE);

    // VirtualFree로 해제
    ::VirtualFree(reinterpret_cast<void*>(baseAddress), 0, MEM_RELEASE);
}

// ------------------------------------
// PoolAllocator
//  - 실제 서버에서 사용하는 기본 메모리 할당자
//  - 내부적으로 Memory/MemoryPool을 활용
//  - 같은 크기의 메모리를 재사용하기 때문에
//    성능 향상 + 단편화 방지 효과
// ------------------------------------
void* PoolAllocator::Alloc(int32 size)
{
    return GMemory->Allocate(size);
}

void PoolAllocator::Release(void* ptr)
{
    GMemory->Release(ptr);
}
