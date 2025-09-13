#pragma once
#include "Allocator.h"

class MemoryPool;

// �޸� �ý��� (Ǯ ���� + �й�)
class Memory
{
    enum
    {
        // Ǯ ���� ��Ģ
        // ~1024���� 32 ����
        // ~2048���� 128 ����
        // ~4096���� 256 ����
        POOL_COUNT = (1024 / 32) + (1024 / 128) + (2048 / 256),

        // Ǯ�� �ִ� ���� ũ��
        MAX_ALLOC_SIZE = 4096
    };

public:
    // ������: �޸� Ǯ �ʱ�ȭ
    Memory();

    // �Ҹ���: �޸� Ǯ ����
    ~Memory();

    // size ũ�⸸ŭ �޸� �Ҵ�
    void* Allocate(int32 size);

    // ������ ��ȯ
    void Release(void* ptr);

private:
    // Ǯ ������ ���
    vector<MemoryPool*> _pools;

    // allocSize �� �ش� Ǯ ���� (O(1) ����)
    MemoryPool* _poolTable[MAX_ALLOC_SIZE + 1];
};

// placement new�� ��ü ����
template<typename Type, typename... Args>
Type* xnew(Args&&... args)
{
    Type* memory = static_cast<Type*>(PoolAllocator::Alloc(sizeof(Type)));
    new(memory)Type(forward<Args>(args)...);
    return memory;
}

// �Ҹ��� ȣ�� �� �޸� �ݳ�
template<typename Type>
void xdelete(Type* obj)
{
    obj->~Type();
    PoolAllocator::Release(obj);
}

// shared_ptr ���� (�ڵ� ����)
template<typename Type, typename... Args>
shared_ptr<Type> MakeShared(Args&&... args)
{
    return shared_ptr<Type>{ xnew<Type>(forward<Args>(args)...), xdelete<Type> };
}
