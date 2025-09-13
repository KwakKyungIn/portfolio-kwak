#pragma once

// ==============================
// NetAddress Ŭ����
// ��Ʈ��ũ �ּ� (IP + Port)�� �ٷ�� ���� ��ƿ Ŭ����
// ==============================

class NetAddress
{
public:
	// �⺻ ������ (0���� �ʱ�ȭ��)
	NetAddress() = default;

	// ���� �ּ� ����ü(SOCKADDR_IN)�κ��� �ʱ�ȭ
	NetAddress(SOCKADDR_IN sockAddr);

	// ���ڿ� IP + ��Ʈ�� �ʱ�ȭ
	NetAddress(wstring ip, uint16 port);

	// ���� ���� �ּ� ����ü ��ȯ
	SOCKADDR_IN& GetSockAddr() { return _sockAddr; }

	// ����� IP�� ���ڿ�(wstring)�� ��ȯ
	wstring GetIpAddress();

	// ����� ��Ʈ�� host byte order�� ��ȯ
	uint16 GetPort() { return ::ntohs(_sockAddr.sin_port); }

public:
	// ���ڿ� IP�� IN_ADDR�� ��ȯ (ex. "127.0.0.1")
	static IN_ADDR Ip2Address(const WCHAR* ip);

private:
	// ���� ��Ʈ��ũ �ּ� ������ (IPv4)
	SOCKADDR_IN _sockAddr = {};
};
