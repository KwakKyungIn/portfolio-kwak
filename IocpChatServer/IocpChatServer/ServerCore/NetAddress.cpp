#include "pch.h"
#include "NetAddress.h"

// ==============================
// NetAddress ������
// ==============================

// ���� �ּҸ� ���� �޾� �ʱ�ȭ (�ַ� Accept �� ���)
NetAddress::NetAddress(SOCKADDR_IN sockAddr) : _sockAddr(sockAddr)
{
}

// ���ڿ� IP�� ��Ʈ��ȣ�� �ʱ�ȭ
// ex) "127.0.0.1", 7777
NetAddress::NetAddress(wstring ip, uint16 port)
{
	::memset(&_sockAddr, 0, sizeof(_sockAddr));    // ����ü �ʱ�ȭ
	_sockAddr.sin_family = AF_INET;                // IPv4 ���
	_sockAddr.sin_addr = Ip2Address(ip.c_str());   // ���ڿ� �� IN_ADDR ��ȯ
	_sockAddr.sin_port = ::htons(port);            // ��Ʈ�� ��Ʈ��ũ ����Ʈ ������ ����
}

// ���� ����� IP�� ���ڿ�(wstring) ���·� ��ȯ
wstring NetAddress::GetIpAddress()
{
	WCHAR buffer[100];
	::InetNtopW(AF_INET, &_sockAddr.sin_addr, buffer, len32(buffer));
	// sockaddr_in �ȿ� �ִ� IP �� ����� ���� �� �ִ� ���ڿ�
	return wstring(buffer);
}

// ���ڿ� IP�� IN_ADDR ����ü�� ��ȯ
// ex) "127.0.0.1" �� 0x7F000001
IN_ADDR NetAddress::Ip2Address(const WCHAR* ip)
{
	IN_ADDR address;
	::InetPtonW(AF_INET, ip, &address); // ���ڿ� �� ������ IP ��ȯ
	return address;
}
