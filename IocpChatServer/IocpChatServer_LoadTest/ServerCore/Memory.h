#pragma once
#include "Allocator.h"

class MemoryPool;

// 메모리 시스템 (풀 관리 + 분배)
class Memory
{
    enum
    {
        // 풀 구성 규칙
        // ~1024까지 32 단위
        // ~2048까지 128 단위
        // ~4096까지 256 단위
        POOL_COUNT = (1024 / 32) + (1024 / 128) + (2048 / 256),

        // 풀링 최대 지원 크기
        MAX_ALLOC_SIZE = 4096
    };

public:
    // 생성자: 메모리 풀 초기화
    Memory();

    // 소멸자: 메모리 풀 정리
    ~Memory();

    // size 크기만큼 메모리 할당
    void* Allocate(int32 size);

    // 포인터 반환
    void Release(void* ptr);

private:
    // 풀 포인터 목록
    vector<MemoryPool*> _pools;

    // allocSize → 해당 풀 매핑 (O(1) 접근)
    MemoryPool* _poolTable[MAX_ALLOC_SIZE + 1];
};

// placement new로 객체 생성
template<typename Type, typename... Args>
Type* xnew(Args&&... args)
{
    Type* memory = static_cast<Type*>(PoolAllocator::Alloc(sizeof(Type)));
    new(memory)Type(forward<Args>(args)...);
    return memory;
}

// 소멸자 호출 후 메모리 반납
template<typename Type>
void xdelete(Type* obj)
{
    obj->~Type();
    PoolAllocator::Release(obj);
}

// shared_ptr 버전 (자동 해제)
template<typename Type, typename... Args>
shared_ptr<Type> MakeShared(Args&&... args)
{
    return shared_ptr<Type>{ xnew<Type>(forward<Args>(args)...), xdelete<Type> };
}
