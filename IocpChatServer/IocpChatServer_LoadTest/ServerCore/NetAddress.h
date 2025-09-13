#pragma once

// ==============================
// NetAddress 클래스
// 네트워크 주소 (IP + Port)를 다루기 위한 유틸 클래스
// ==============================

class NetAddress
{
public:
	// 기본 생성자 (0으로 초기화됨)
	NetAddress() = default;

	// 소켓 주소 구조체(SOCKADDR_IN)로부터 초기화
	NetAddress(SOCKADDR_IN sockAddr);

	// 문자열 IP + 포트로 초기화
	NetAddress(wstring ip, uint16 port);

	// 내부 소켓 주소 구조체 반환
	SOCKADDR_IN& GetSockAddr() { return _sockAddr; }

	// 저장된 IP를 문자열(wstring)로 반환
	wstring GetIpAddress();

	// 저장된 포트를 host byte order로 반환
	uint16 GetPort() { return ::ntohs(_sockAddr.sin_port); }

public:
	// 문자열 IP를 IN_ADDR로 변환 (ex. "127.0.0.1")
	static IN_ADDR Ip2Address(const WCHAR* ip);

private:
	// 실제 네트워크 주소 데이터 (IPv4)
	SOCKADDR_IN _sockAddr = {};
};
