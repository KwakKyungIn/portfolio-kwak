#include "pch.h"
#include "NetAddress.h"

// ==============================
// NetAddress 구현부
// ==============================

// 소켓 주소를 직접 받아 초기화 (주로 Accept 시 사용)
NetAddress::NetAddress(SOCKADDR_IN sockAddr) : _sockAddr(sockAddr)
{
}

// 문자열 IP와 포트번호로 초기화
// ex) "127.0.0.1", 7777
NetAddress::NetAddress(wstring ip, uint16 port)
{
	::memset(&_sockAddr, 0, sizeof(_sockAddr));    // 구조체 초기화
	_sockAddr.sin_family = AF_INET;                // IPv4 사용
	_sockAddr.sin_addr = Ip2Address(ip.c_str());   // 문자열 → IN_ADDR 변환
	_sockAddr.sin_port = ::htons(port);            // 포트는 네트워크 바이트 오더로 저장
}

// 현재 저장된 IP를 문자열(wstring) 형태로 반환
wstring NetAddress::GetIpAddress()
{
	WCHAR buffer[100];
	::InetNtopW(AF_INET, &_sockAddr.sin_addr, buffer, len32(buffer));
	// sockaddr_in 안에 있는 IP → 사람이 읽을 수 있는 문자열
	return wstring(buffer);
}

// 문자열 IP를 IN_ADDR 구조체로 변환
// ex) "127.0.0.1" → 0x7F000001
IN_ADDR NetAddress::Ip2Address(const WCHAR* ip)
{
	IN_ADDR address;
	::InetPtonW(AF_INET, ip, &address); // 문자열 → 이진수 IP 변환
	return address;
}
