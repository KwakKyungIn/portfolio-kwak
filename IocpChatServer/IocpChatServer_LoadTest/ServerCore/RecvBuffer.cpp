#include "pch.h"
#include "RecvBuffer.h"

// ==============================
// RecvBuffer
// - ��Ʈ��ũ Recv �� ����ϴ� ���� ����
// - �б�/���� ��ġ�� ���� ����
// ==============================

// ������: ���� ũ�� ����
// - bufferSize * BUFFER_COUNT ��ŭ ���� Ȯ��
RecvBuffer::RecvBuffer(int32 bufferSize) : _bufferSize(bufferSize)
{
	_capacity = bufferSize * BUFFER_COUNT; // ��ü �뷮
	_buffer.resize(_capacity);             // ���� ���� �޸� �Ҵ�
}

RecvBuffer::~RecvBuffer()
{
}

// ���� ����
// - �� ���� ���: Ŀ�� �ʱ�ȭ (0,0)
// - ���� ���� ����: �����͸� ������ ���ܼ� ���� Ȯ��
void RecvBuffer::Clean()
{
	int32 dataSize = DataSize(); // ���� ���� �ִ� ������ ũ��
	if (dataSize == 0)
	{
		// �����Ͱ� �ϳ��� ������ Ŀ�� �� �� ����
		_readPos = _writePos = 0;
	}
	else
	{
		// ���� ������ �� ���(bufferSize)���� ������ ������ ���
		if (FreeSize() < _bufferSize)
		{
			::memcpy(&_buffer[0], &_buffer[_readPos], dataSize);
			_readPos = 0;
			_writePos = dataSize;
		}
	}
}

// �б� Ŀ�� ����
// - numOfBytes��ŭ ������ �Ҹ�
bool RecvBuffer::OnRead(int32 numOfBytes)
{
	if (numOfBytes > DataSize()) // ���� �� �ִ� �����ͺ��� ������ ����
		return false;

	_readPos += numOfBytes;
	return true;
}

// ���� Ŀ�� ����
// - numOfBytes��ŭ ���ο� ������ ���� �Ϸ�
bool RecvBuffer::OnWrite(int32 numOfBytes)
{
	if (numOfBytes > FreeSize()) // ���� �������� ������ ����
		return false;

	_writePos += numOfBytes;
	return true;
}
