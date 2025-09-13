#pragma once
#include "Types.h"
#include "MemoryPool.h"

// ���ø� ��� ObjectPool
// - ��ü ���� Ǯ��
// - new/delete ��� Pop/Push ���
template<typename Type>
class ObjectPool
{
public:
    // ��ü ���� (placement new ���)
    template<typename... Args>
    static Type* Pop(Args&&... args)
    {
#ifdef _STOMP
        // StompAllocator ��� (����� ���)
        MemoryHeader* ptr = reinterpret_cast<MemoryHeader*>(StompAllocator::Alloc(s_allocSize));
        Type* memory = static_cast<Type*>(MemoryHeader::AttachHeader(ptr, s_allocSize));
#else
        // MemoryPool ��� (���� ���)
        Type* memory = static_cast<Type*>(MemoryHeader::AttachHeader(s_pool.Pop(), s_allocSize));
#endif
        new(memory)Type(forward<Args>(args)...); // placement new
        return memory;
    }

    // ��ü �ݳ� (�Ҹ��� ȣ�� �� Ǯ�� ��ȯ)
    static void Push(Type* obj)
    {
        obj->~Type();
#ifdef _STOMP
        StompAllocator::Release(MemoryHeader::DetachHeader(obj));
#else
        s_pool.Push(MemoryHeader::DetachHeader(obj));
#endif
    }

    // shared_ptr ���·� ��ȯ (�ڵ� ��ȯ ����)
    template<typename... Args>
    static shared_ptr<Type> MakeShared(Args&&... args)
    {
        shared_ptr<Type> ptr = { Pop(forward<Args>(args)...), Push };
        return ptr;
    }

private:
    // ��ü ũ�� + ���
    static int32 s_allocSize;

    // ���� �޸� Ǯ
    static MemoryPool s_pool;
};

// ���� ��� �ʱ�ȭ
template<typename Type>
int32 ObjectPool<Type>::s_allocSize = sizeof(Type) + sizeof(MemoryHeader);

template<typename Type>
MemoryPool ObjectPool<Type>::s_pool{ s_allocSize };
