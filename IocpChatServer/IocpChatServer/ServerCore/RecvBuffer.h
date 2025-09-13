#pragma once

// ==============================
// RecvBuffer
// - ��Ʈ��ũ���� ���� �����͸� �ӽ� ����
// - �б�/���� Ŀ���� ���� �ּ� ȿ�������� ����
// ==============================
class RecvBuffer
{
	// ���ο��� �����ϴ� ���� ��� ���� (�⺻ 10)
	enum { BUFFER_COUNT = 10 };

public:
	// ������: ���� 1�� ũ�⸦ �޾Ƽ� ��ü �뷮 ���
	RecvBuffer(int32 bufferSize);
	~RecvBuffer();

	// ���� ����
	// - ������ ������ Ŀ�� �ʱ�ȭ
	// - ���� ���� �����ϸ� ������ ��ܿ�
	void Clean();

	// �б� �Ϸ� ó�� �� �б� Ŀ�� ����
	bool OnRead(int32 numOfBytes);

	// ���� �Ϸ� ó�� �� ���� Ŀ�� ����
	bool OnWrite(int32 numOfBytes);

	// ���� �б� ��ġ ������
	BYTE* ReadPos() { return &_buffer[_readPos]; }

	// ���� ���� ��ġ ������
	BYTE* WritePos() { return &_buffer[_writePos]; }

	// ���� �ִ� ������ ũ��
	int32 DataSize() { return _writePos - _readPos; }

	// ���� ���� ���� ũ��
	int32 FreeSize() { return _capacity - _writePos; }

private:
	int32 _capacity = 0;      // �� ���� �뷮
	int32 _bufferSize = 0;    // ���� �� ��� ũ��
	int32 _readPos = 0;       // ���� �б� ��ġ
	int32 _writePos = 0;      // ���� ���� ��ġ
	Vector<BYTE> _buffer;     // ���� ������ �����
};
