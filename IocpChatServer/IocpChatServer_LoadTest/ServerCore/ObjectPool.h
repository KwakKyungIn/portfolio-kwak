#pragma once
#include "Types.h"
#include "MemoryPool.h"

// 템플릿 기반 ObjectPool
// - 객체 단위 풀링
// - new/delete 대신 Pop/Push 사용
template<typename Type>
class ObjectPool
{
public:
    // 객체 생성 (placement new 사용)
    template<typename... Args>
    static Type* Pop(Args&&... args)
    {
#ifdef _STOMP
        // StompAllocator 사용 (디버깅 모드)
        MemoryHeader* ptr = reinterpret_cast<MemoryHeader*>(StompAllocator::Alloc(s_allocSize));
        Type* memory = static_cast<Type*>(MemoryHeader::AttachHeader(ptr, s_allocSize));
#else
        // MemoryPool 사용 (실제 모드)
        Type* memory = static_cast<Type*>(MemoryHeader::AttachHeader(s_pool.Pop(), s_allocSize));
#endif
        new(memory)Type(forward<Args>(args)...); // placement new
        return memory;
    }

    // 객체 반납 (소멸자 호출 후 풀에 반환)
    static void Push(Type* obj)
    {
        obj->~Type();
#ifdef _STOMP
        StompAllocator::Release(MemoryHeader::DetachHeader(obj));
#else
        s_pool.Push(MemoryHeader::DetachHeader(obj));
#endif
    }

    // shared_ptr 형태로 반환 (자동 반환 지원)
    template<typename... Args>
    static shared_ptr<Type> MakeShared(Args&&... args)
    {
        shared_ptr<Type> ptr = { Pop(forward<Args>(args)...), Push };
        return ptr;
    }

private:
    // 객체 크기 + 헤더
    static int32 s_allocSize;

    // 내부 메모리 풀
    static MemoryPool s_pool;
};

// 정적 멤버 초기화
template<typename Type>
int32 ObjectPool<Type>::s_allocSize = sizeof(Type) + sizeof(MemoryHeader);

template<typename Type>
MemoryPool ObjectPool<Type>::s_pool{ s_allocSize };
