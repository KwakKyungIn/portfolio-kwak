#include "pch.h"
#include "SendBuffer.h"
#include "Metrics.h" // 메트릭 계측

// ==============================
// SendBuffer
// - 실제 전송할 데이터 조각을 표현
// - 하나의 Chunk 내부 일부 구간을 참조
// ==============================

SendBuffer::SendBuffer(SendBufferChunkRef owner, BYTE* buffer, uint32 allocSize)
    : _owner(owner), _buffer(buffer), _allocSize(allocSize)
{
    // owner(Chunk)를 잡고 있는 동안은 메모리 유효
    // buffer는 owner가 관리하는 배열 내부 포인터
}

SendBuffer::~SendBuffer()
{
    // 여기서 별도 free 없음
    // 소멸 시점에 owner가 chunk 수명 관리
}

// 전송할 데이터의 실제 크기 확정
// - allocSize 이하로만 닫을 수 있음
// - 닫힌 순간, Chunk에 사용량 기록
void SendBuffer::Close(uint32 writeSize)
{
    ASSERT_CRASH(_allocSize >= writeSize); // 할당 크기 초과 방지
    _writeSize = writeSize;
    _owner->Close(writeSize);
}

// ==============================
// SendBufferChunk
// - 고정 크기(6KB) 메모리 블록
// - 여러 SendBuffer가 같은 Chunk를 공유 가능
// - Scatter-Gather WSASend 최적화에 활용
// ==============================

SendBufferChunk::SendBufferChunk()
{
}

SendBufferChunk::~SendBufferChunk()
{
}

// Chunk 초기화
// - 재사용 시 오픈 여부/사용량 리셋
void SendBufferChunk::Reset()
{
    _open = false;
    _usedSize = 0;
}

// Chunk 내부에서 일정 크기만큼 잘라 SendBuffer 반환
// - 한 번에 하나만 open 가능
// - 실패 시 nullptr 반환
SendBufferRef SendBufferChunk::Open(uint32 allocSize)
{
    ASSERT_CRASH(allocSize <= SEND_BUFFER_CHUNK_SIZE); // 6KB 제한
    ASSERT_CRASH(_open == false);                      // 중복 오픈 방지

    if (allocSize > FreeSize()) // 남은 공간 부족
        return nullptr;

    _open = true;
    return ObjectPool<SendBuffer>::MakeShared(shared_from_this(), Buffer(), allocSize);
}

// Chunk 닫기
// - writeSize만큼 사용량 누적
void SendBufferChunk::Close(uint32 writeSize)
{
    ASSERT_CRASH(_open == true);
    _open = false;
    _usedSize += writeSize;
}

// ==============================
// SendBufferManager
// - Chunk 풀을 전역적으로 관리
// - TLS(Local) + Global Pool 2단 구조
// ==============================

// 요청한 크기에 맞는 SendBuffer 반환
// - TLS Chunk에서 먼저 제공
// - 공간 부족 시 Global Pool에서 새 Chunk 획득
SendBufferRef SendBufferManager::Open(uint32 size)
{
    // TLS에 Chunk가 없으면 새로 Pop
    if (LSendBufferChunk == nullptr)
    {
        LSendBufferChunk = Pop(); // WRITE_LOCK으로 보호
        LSendBufferChunk->Reset();
    }

    ASSERT_CRASH(LSendBufferChunk->IsOpen() == false);

    // 현재 Chunk 공간 부족 → 버리고 새 Chunk 가져옴
    if (LSendBufferChunk->FreeSize() < size)
    {
        LSendBufferChunk = Pop(); // WRITE_LOCK으로 보호
        LSendBufferChunk->Reset();
    }

    return LSendBufferChunk->Open(size);
}

// Global Pool에서 Chunk 하나 가져오기
// - 있으면 재사용
// - 없으면 새로 생성
SendBufferChunkRef SendBufferManager::Pop()
{
    {
        WRITE_LOCK;
        if (_sendBufferChunks.empty() == false)
        {
            // Global Pool에서 하나 꺼냄
            SendBufferChunkRef sendBufferChunk = _sendBufferChunks.back();
            _sendBufferChunks.pop_back();

            // 메트릭: Pop 횟수 + InUse 카운트 + 피크 갱신
            GMetrics.sendbuf_global_pop.fetch_add(1, std::memory_order_relaxed);
            uint32_t inuse = GMetrics.sendbuf_inuse_gauge.fetch_add(1, std::memory_order_relaxed) + 1;
            uint32_t prev = GMetrics.sendbuf_inuse_peak.load(std::memory_order_relaxed);
            while (inuse > prev && !GMetrics.sendbuf_inuse_peak.compare_exchange_weak(prev, inuse, std::memory_order_relaxed)) {}

            return sendBufferChunk;
        }
    }

    // 풀에 없으면 새로 생성 (초기 부팅 시 자주 발생)
    SendBufferChunkRef ref = SendBufferChunkRef(xnew<SendBufferChunk>(), PushGlobal);

    // 생성 경로도 Pop으로 간주 (메트릭 처리)
    GMetrics.sendbuf_global_pop.fetch_add(1, std::memory_order_relaxed);
    {
        uint32_t inuse = GMetrics.sendbuf_inuse_gauge.fetch_add(1, std::memory_order_relaxed) + 1;
        uint32_t prev = GMetrics.sendbuf_inuse_peak.load(std::memory_order_relaxed);
        while (inuse > prev && !GMetrics.sendbuf_inuse_peak.compare_exchange_weak(prev, inuse, std::memory_order_relaxed)) {}
    }

    return ref;
}

// Chunk를 Global Pool에 반납
void SendBufferManager::Push(SendBufferChunkRef buffer)
{
    WRITE_LOCK;
    _sendBufferChunks.push_back(buffer);
}

// 소멸 시 호출되는 커스텀 deleter
// - Global Pool로 반환
// - 메트릭 갱신
void SendBufferManager::PushGlobal(SendBufferChunk* buffer)
{
    GMetrics.sendbuf_global_push.fetch_add(1, std::memory_order_relaxed);
    GMetrics.sendbuf_inuse_gauge.fetch_sub(1, std::memory_order_relaxed);

    GSendBufferManager->Push(SendBufferChunkRef(buffer, PushGlobal));
}
