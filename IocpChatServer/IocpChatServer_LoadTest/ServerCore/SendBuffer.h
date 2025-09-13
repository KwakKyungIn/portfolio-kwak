#pragma once

class SendBufferChunk;

// ==============================
// SendBuffer
// - Chunk 내부의 특정 구간을 가리키는 핸들
// - Scatter–Gather WSASend에서 사용됨
// ==============================
class SendBuffer
{
public:
    SendBuffer(SendBufferChunkRef owner, BYTE* buffer, uint32 allocSize);
    ~SendBuffer();

    // 실제 버퍼 포인터 (WSABUF에서 사용)
    BYTE* Buffer() const { return _buffer; }

    // 예약된 크기
    uint32 AllocSize() const { return _allocSize; }

    // 실제 사용 크기 (Close 호출 후 확정됨)
    uint32 WriteSize() const { return _writeSize; }

    // writeSize만큼 사용량을 확정하고 Chunk에 반영
    void Close(uint32 writeSize);

    // 이 SendBuffer의 소유 Chunk
    SendBufferChunkRef GetOwner() const { return _owner; }

private:
    BYTE* _buffer = nullptr;              // Chunk 내부 데이터 시작 포인터
    uint32 _allocSize = 0;                // 예약 크기
    uint32 _writeSize = 0;                // 실제 기록된 크기
    SendBufferChunkRef _owner;            // 소유 Chunk (shared_ptr)
};

// ==============================
// SendBufferChunk
// - 고정 크기(6KB) 메모리 덩어리
// - 여러 SendBuffer를 잘라서 제공
// - enable_shared_from_this: Pool 반환 시 shared_ptr 관리
// ==============================
class SendBufferChunk : public enable_shared_from_this<SendBufferChunk>
{
    enum { SEND_BUFFER_CHUNK_SIZE = 6000 }; // 한 Chunk 크기

public:
    SendBufferChunk();
    ~SendBufferChunk();

    // Chunk 재사용 준비
    void Reset();

    // allocSize만큼 공간 잘라 SendBuffer 생성
    SendBufferRef Open(uint32 allocSize);

    // 사용량 확정
    void Close(uint32 writeSize);

    // 현재 오픈 여부
    bool IsOpen() const { return _open; }

    // 현재 Chunk 내에서 쓸 수 있는 버퍼 시작 포인터
    BYTE* Buffer() { return &_buffer[_usedSize]; }

    // 남은 여유 공간
    uint32 FreeSize() const { return static_cast<uint32>(_buffer.size()) - _usedSize; }

private:
    Array<BYTE, SEND_BUFFER_CHUNK_SIZE> _buffer = {}; // 고정 크기 버퍼
    bool   _open = false;                            // 현재 Open 상태
    uint32 _usedSize = 0;                            // 지금까지 사용된 크기
};

// ==============================
// SendBufferManager
// - Global Pool 관리
// - TLS LocalChunk 사용 후 Global로 반납
// - Scatter–Gather WSASend 성능 극대화
// ==============================
class SendBufferManager
{
public:
    // size 크기의 SendBuffer 요청
    SendBufferRef Open(uint32 size);

    // 소멸자용 deleter: Global Pool에 반환
    static void PushGlobal(SendBufferChunk* buffer);

private:
    // Global Pool에서 Chunk 하나 가져오기
    SendBufferChunkRef Pop();

    // Chunk를 Global Pool에 반환
    void Push(SendBufferChunkRef buffer);

private:
    USE_LOCK;                                // Global Pool 동기화용 락
    Vector<SendBufferChunkRef> _sendBufferChunks; // Chunk 보관
};
