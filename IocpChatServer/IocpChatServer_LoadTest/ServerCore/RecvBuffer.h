#pragma once

// ==============================
// RecvBuffer
// - 네트워크에서 받은 데이터를 임시 저장
// - 읽기/쓰기 커서를 따로 둬서 효율적으로 관리
// ==============================
class RecvBuffer
{
	// 내부에서 관리하는 버퍼 덩어리 개수 (기본 10)
	enum { BUFFER_COUNT = 10 };

public:
	// 생성자: 버퍼 1개 크기를 받아서 전체 용량 계산
	RecvBuffer(int32 bufferSize);
	~RecvBuffer();

	// 버퍼 정리
	// - 데이터 없으면 커서 초기화
	// - 여유 공간 부족하면 앞으로 당겨옴
	void Clean();

	// 읽기 완료 처리 → 읽기 커서 전진
	bool OnRead(int32 numOfBytes);

	// 쓰기 완료 처리 → 쓰기 커서 전진
	bool OnWrite(int32 numOfBytes);

	// 현재 읽기 위치 포인터
	BYTE* ReadPos() { return &_buffer[_readPos]; }

	// 현재 쓰기 위치 포인터
	BYTE* WritePos() { return &_buffer[_writePos]; }

	// 남아 있는 데이터 크기
	int32 DataSize() { return _writePos - _readPos; }

	// 남은 여유 공간 크기
	int32 FreeSize() { return _capacity - _writePos; }

private:
	int32 _capacity = 0;      // 총 버퍼 용량
	int32 _bufferSize = 0;    // 버퍼 한 덩어리 크기
	int32 _readPos = 0;       // 현재 읽기 위치
	int32 _writePos = 0;      // 현재 쓰기 위치
	Vector<BYTE> _buffer;     // 실제 데이터 저장소
};
