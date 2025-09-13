#pragma once

// SLIST 정렬 기준 (16바이트 정렬 필요)
enum
{
    SLIST_ALIGNMENT = 16
};

// 메모리 블록 헤더
DECLSPEC_ALIGN(SLIST_ALIGNMENT)
struct MemoryHeader : public SLIST_ENTRY
{
    // [MemoryHeader][실제 데이터]
    MemoryHeader(int32 size) : allocSize(size) {}

    // Header 붙여서 사용자에게 포인터 반환
    static void* AttachHeader(MemoryHeader* header, int32 size)
    {
        new(header)MemoryHeader(size); // placement new
        return reinterpret_cast<void*>(++header);
    }

    // 사용자 포인터에서 Header 찾기
    static MemoryHeader* DetachHeader(void* ptr)
    {
        MemoryHeader* header = reinterpret_cast<MemoryHeader*>(ptr) - 1;
        return header;
    }

    int32 allocSize; // 현재 블록 크기
};

// 메모리 풀 클래스
DECLSPEC_ALIGN(SLIST_ALIGNMENT)
class MemoryPool
{
public:
    // 풀 생성 (특정 크기 단위)
    MemoryPool(int32 allocSize);

    // 풀 소멸 (남은 블록 정리)
    ~MemoryPool();

    // 메모리를 풀에 반납
    void Push(MemoryHeader* ptr);

    // 메모리를 풀에서 꺼내옴
    MemoryHeader* Pop();

private:
    SLIST_HEADER _header;    // lock-free 리스트
    int32 _allocSize = 0;    // 블록 크기
    atomic<int32> _useCount = 0;     // 사용 중인 블록 수
    atomic<int32> _reserveCount = 0; // 대기 중인 블록 수
};
