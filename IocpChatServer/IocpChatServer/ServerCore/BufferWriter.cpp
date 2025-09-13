#include "pch.h"
#include "BufferWriter.h"

/*----------------
    BufferWriter
    - 메모리에 순차적으로 데이터를 씀
    - 남은 공간 체크 후 쓰기 가능
-----------------*/

BufferWriter::BufferWriter()
{
}

BufferWriter::BufferWriter(BYTE* buffer, uint32 size, uint32 pos)
    : _buffer(buffer), _size(size), _pos(pos)
{
}

BufferWriter::~BufferWriter()
{
}

bool BufferWriter::Write(void* src, uint32 len)
{
    // 공간 부족 시 실패
    if (FreeSize() < len)
        return false;

    // 현재 위치에 쓰고 포인터 이동
    ::memcpy(&_buffer[_pos], src, len);
    _pos += len;
    return true;
}
