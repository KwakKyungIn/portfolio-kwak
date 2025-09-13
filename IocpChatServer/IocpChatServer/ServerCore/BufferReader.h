#pragma once

/*----------------
    BufferReader
-----------------*/

class BufferReader
{
public:
    BufferReader();
    BufferReader(BYTE* buffer, uint32 size, uint32 pos = 0);
    ~BufferReader();

    BYTE* Buffer() { return _buffer; }
    uint32 Size() { return _size; }
    uint32 ReadSize() { return _pos; }
    uint32 FreeSize() { return _size - _pos; }

    template<typename T>
    bool Peek(T* dest) { return Peek(dest, sizeof(T)); }
    bool Peek(void* dest, uint32 len);

    template<typename T>
    bool Read(T* dest) { return Read(dest, sizeof(T)); }
    bool Read(void* dest, uint32 len);

    template<typename T>
    BufferReader& operator>>(OUT T& dest);

private:
    BYTE* _buffer = nullptr; // 읽기 대상 버퍼
    uint32 _size = 0;       // 전체 버퍼 크기
    uint32 _pos = 0;       // 현재 읽기 위치
};

template<typename T>
inline BufferReader& BufferReader::operator>>(OUT T& dest)
{
    // 버퍼에서 바로 구조체/타입 복사
    dest = *reinterpret_cast<T*>(&_buffer[_pos]);
    _pos += sizeof(T);
    return *this;
}
