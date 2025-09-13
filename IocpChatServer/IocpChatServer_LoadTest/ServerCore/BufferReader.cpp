#include "pch.h"
#include "BufferReader.h"

/*----------------
    BufferReader
    - 주어진 메모리에서 읽기 전용 뷰 제공
    - 위치(_pos)를 자동으로 증가시키면서 안전하게 읽음
-----------------*/

BufferReader::BufferReader()
{
}

BufferReader::BufferReader(BYTE* buffer, uint32 size, uint32 pos)
    : _buffer(buffer), _size(size), _pos(pos)
{
}

BufferReader::~BufferReader()
{
}

bool BufferReader::Peek(void* dest, uint32 len)
{
    // 남은 크기보다 더 읽으려 하면 실패
    if (FreeSize() < len)
        return false;

    // 단순 복사, _pos는 증가하지 않음
    ::memcpy(dest, &_buffer[_pos], len);
    return true;
}

bool BufferReader::Read(void* dest, uint32 len)
{
    // Peek 성공해야만 실제 이동
    if (Peek(dest, len) == false)
        return false;

    _pos += len; // 읽은 만큼 위치 증가
    return true;
}
