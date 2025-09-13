#include "pch.h"
#include "Allocator.h"
#include "Memory.h"

// ------------------------------------
// BaseAllocator
//  - ���� �ܼ��� �޸� �Ҵ���
//  - malloc / free �״�� ���
//  - �����/���� �м������� �ٸ� Allocator�� ���� �� ����
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
//  - �޸� ����(overflow/underflow) ������
//  - ������ ������ �Ҵ��ؼ�, �߸��� ���� �� �ٷ� ũ���� ������ ����
//  - ���� ������ �� ������, ���� ���� �� ����
// ------------------------------------
void* StompAllocator::Alloc(int32 size)
{
    // �ʿ��� ������ ���� ���
    const int64 pageCount = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    // ������ ���ʿ� �����Ͱ� ��ġ�ϵ��� offset ���
    const int64 dataOffset = pageCount * PAGE_SIZE - size;

    // Windows API�� ���� �޸� ������ �Ҵ�
    void* baseAddress = ::VirtualAlloc(NULL, pageCount * PAGE_SIZE,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    // offset ���� �� ���� ������ ��ȯ
    return static_cast<void*>(static_cast<int8*>(baseAddress) + dataOffset);
}

void StompAllocator::Release(void* ptr)
{
    // baseAddress = ������ ���� ���� �ּ�
    const int64 address = reinterpret_cast<int64>(ptr);
    const int64 baseAddress = address - (address % PAGE_SIZE);

    // VirtualFree�� ����
    ::VirtualFree(reinterpret_cast<void*>(baseAddress), 0, MEM_RELEASE);
}

// ------------------------------------
// PoolAllocator
//  - ���� �������� ����ϴ� �⺻ �޸� �Ҵ���
//  - ���������� Memory/MemoryPool�� Ȱ��
//  - ���� ũ���� �޸𸮸� �����ϱ� ������
//    ���� ��� + ����ȭ ���� ȿ��
// ------------------------------------
void* PoolAllocator::Alloc(int32 size)
{
    return GMemory->Allocate(size);
}

void PoolAllocator::Release(void* ptr)
{
    GMemory->Release(ptr);
}
