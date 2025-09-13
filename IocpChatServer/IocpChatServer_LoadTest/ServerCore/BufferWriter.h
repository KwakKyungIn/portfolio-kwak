#pragma once

/*----------------
    BufferWriter
-----------------*/

class BufferWriter
{
public:
    BufferWriter();
    BufferWriter(BYTE* buffer, uint32 size, uint32 pos = 0);
    ~BufferWriter();

    BYTE* Buffer() { return _buffer; }
    uint32 Size() { return _size; }
    uint32 WriteSize() { return _pos; }
    uint32 FreeSize() { return _size - _pos; }

    template<typename T>
    bool Write(T* src) { return Write(src, sizeof(T)); }
    bool Write(void* src, uint32 len);

    template<typename T>
    T* Reserve(uint16 count = 1);

    template<typename T>
    BufferWriter& operator<<(T&& src);

private:
    BYTE* _buffer = nullptr; // 쓰기 대상 버퍼
    uint32 _size = 0;       // 전체 버퍼 크기
    uint32 _pos = 0;       // 현재 쓰기 위치
};

template<typename T>
T* BufferWriter::Reserve(uint16 count)
{
    // 지정한 타입 공간 확보
    if (FreeSize() < (sizeof(T) * count))
        return nullptr;

    T* ret = reinterpret_cast<T*>(&_buffer[_pos]);
    _pos += (sizeof(T) * count);
    return ret;
}

template<typename T>
BufferWriter& BufferWriter::operator<<(T&& src)
{
    // 타입 그대로 버퍼에 기록
    using DataType = std::remove_reference_t<T>;
    *reinterpret_cast<DataType*>(&_buffer[_pos]) = std::forward<DataType>(src);
    _pos += sizeof(T);
    return *this;
}
