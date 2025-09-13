#include "pch.h"
#include "RecvBuffer.h"

// ==============================
// RecvBuffer
// - 네트워크 Recv 시 사용하는 순차 버퍼
// - 읽기/쓰기 위치를 따로 관리
// ==============================

// 생성자: 버퍼 크기 지정
// - bufferSize * BUFFER_COUNT 만큼 공간 확보
RecvBuffer::RecvBuffer(int32 bufferSize) : _bufferSize(bufferSize)
{
	_capacity = bufferSize * BUFFER_COUNT; // 전체 용량
	_buffer.resize(_capacity);             // 실제 벡터 메모리 할당
}

RecvBuffer::~RecvBuffer()
{
}

// 버퍼 정리
// - 다 읽은 경우: 커서 초기화 (0,0)
// - 여유 공간 부족: 데이터를 앞으로 땡겨서 공간 확보
void RecvBuffer::Clean()
{
	int32 dataSize = DataSize(); // 현재 남아 있는 데이터 크기
	if (dataSize == 0)
	{
		// 데이터가 하나도 없으면 커서 둘 다 리셋
		_readPos = _writePos = 0;
	}
	else
	{
		// 남은 공간이 한 덩어리(bufferSize)보다 작으면 앞으로 당김
		if (FreeSize() < _bufferSize)
		{
			::memcpy(&_buffer[0], &_buffer[_readPos], dataSize);
			_readPos = 0;
			_writePos = dataSize;
		}
	}
}

// 읽기 커서 전진
// - numOfBytes만큼 데이터 소모
bool RecvBuffer::OnRead(int32 numOfBytes)
{
	if (numOfBytes > DataSize()) // 읽을 수 있는 데이터보다 많으면 실패
		return false;

	_readPos += numOfBytes;
	return true;
}

// 쓰기 커서 전진
// - numOfBytes만큼 새로운 데이터 수신 완료
bool RecvBuffer::OnWrite(int32 numOfBytes)
{
	if (numOfBytes > FreeSize()) // 남은 공간보다 많으면 실패
		return false;

	_writePos += numOfBytes;
	return true;
}
