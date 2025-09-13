#include "pch.h"
#include "BufferReader.h"

/*----------------
    BufferReader
    - �־��� �޸𸮿��� �б� ���� �� ����
    - ��ġ(_pos)�� �ڵ����� ������Ű�鼭 �����ϰ� ����
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
    // ���� ũ�⺸�� �� ������ �ϸ� ����
    if (FreeSize() < len)
        return false;

    // �ܼ� ����, _pos�� �������� ����
    ::memcpy(dest, &_buffer[_pos], len);
    return true;
}

bool BufferReader::Read(void* dest, uint32 len)
{
    // Peek �����ؾ߸� ���� �̵�
    if (Peek(dest, len) == false)
        return false;

    _pos += len; // ���� ��ŭ ��ġ ����
    return true;
}
