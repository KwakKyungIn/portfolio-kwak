#pragma once

// SLIST ���� ���� (16����Ʈ ���� �ʿ�)
enum
{
    SLIST_ALIGNMENT = 16
};

// �޸� ��� ���
DECLSPEC_ALIGN(SLIST_ALIGNMENT)
struct MemoryHeader : public SLIST_ENTRY
{
    // [MemoryHeader][���� ������]
    MemoryHeader(int32 size) : allocSize(size) {}

    // Header �ٿ��� ����ڿ��� ������ ��ȯ
    static void* AttachHeader(MemoryHeader* header, int32 size)
    {
        new(header)MemoryHeader(size); // placement new
        return reinterpret_cast<void*>(++header);
    }

    // ����� �����Ϳ��� Header ã��
    static MemoryHeader* DetachHeader(void* ptr)
    {
        MemoryHeader* header = reinterpret_cast<MemoryHeader*>(ptr) - 1;
        return header;
    }

    int32 allocSize; // ���� ��� ũ��
};

// �޸� Ǯ Ŭ����
DECLSPEC_ALIGN(SLIST_ALIGNMENT)
class MemoryPool
{
public:
    // Ǯ ���� (Ư�� ũ�� ����)
    MemoryPool(int32 allocSize);

    // Ǯ �Ҹ� (���� ��� ����)
    ~MemoryPool();

    // �޸𸮸� Ǯ�� �ݳ�
    void Push(MemoryHeader* ptr);

    // �޸𸮸� Ǯ���� ������
    MemoryHeader* Pop();

private:
    SLIST_HEADER _header;    // lock-free ����Ʈ
    int32 _allocSize = 0;    // ��� ũ��
    atomic<int32> _useCount = 0;     // ��� ���� ��� ��
    atomic<int32> _reserveCount = 0; // ��� ���� ��� ��
};
