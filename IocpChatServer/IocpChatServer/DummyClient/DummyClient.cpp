#include "pch.h"
#include "ThreadManager.h"
#include "Service.h"
#include "Session.h"
#include "BufferReader.h"
#include "ServerPacketHandler.h"
#include "Protocol.pb.h"
#include <iostream>
#include <string> // std::string, std::getline 사용을 위해 추가
#include <atomic>
#include <limits>   // std::numeric_limits
#include <ios>      // std::streamsize
// 로그인 성공 여부를 저장하는 전역 플래그를 '정의'합니다.
std::atomic<bool> g_isLoggedIn = false;
// [추가] 방에 입장했는지 여부를 저장하는 전역 플래그를 '정의'합니다.
std::atomic<bool> g_isInRoom = false;

// 연결된 세션을 저장하는 전역 변수를 '정의'합니다.
PacketSessionRef g_session = nullptr;




class ServerSession : public PacketSession
{
public:
	~ServerSession()
	{
		std::cout << "~ServerSession" << std::endl;
	}

	virtual void OnConnected() override
	{
		g_session = GetPacketSessionRef();

		std::cout << "로그인할 플레이어 이름을 입력하세요: ";
		std::string playerName;
		std::cin >> playerName;

		Protocol::C_LOGIN_REQ pkt;
		pkt.set_name(playerName);

		auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
		Send(sendBuffer);

		std::cout << "서버에 '" << playerName << "' 플레이어의 로그인 요청을 보냈습니다." << std::endl;
	}

	virtual void OnRecvPacket(BYTE* buffer, int32 len) override
	{
		PacketSessionRef session = GetPacketSessionRef();
		PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);

		ServerPacketHandler::HandlePacket(session, buffer, len);
	}

	virtual void OnSend(int32 len) override
	{
		//std::cout << "OnSend Len = " << len << std::endl;
	}

	virtual void OnDisconnected() override
	{
		g_isLoggedIn = false;
		g_isInRoom = false; // [추가] 연결 끊어지면 방에서도 나간 상태로 변경
		g_session = nullptr;
		std::cout << "서버와 연결이 끊어졌습니다." << std::endl;
	}
};

int main()
{
	ServerPacketHandler::Init();

	std::this_thread::sleep_for(std::chrono::seconds(1));

	ClientServiceRef service = MakeShared<ClientService>(
		NetAddress(L"127.0.0.1", 7777),
		MakeShared<IocpCore>(),
		MakeShared<ServerSession>,
		1);

	ASSERT_CRASH(service->Start());

	GThreadManager->Launch([=]()
		{
			while (true)
			{
				service->GetIocpCore()->Dispatch();
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		});

	// [변경] 메인 스레드의 사용자 입력 처리 로직 전체 수정
	while (true)
	{
		// 로그인 된 상태에서만 입력 처리
		if (g_isLoggedIn)
		{
			// 방에 들어가 있는 상태라면 채팅 입력 모드
			if (g_isInRoom)
			{
				std::string message;
				std::getline(std::cin, message); // 공백 포함 한 줄 전체 입력받기

				if (message.empty()) // 로그인 후나 방 생성/입장 후 버퍼에 남아있는 개행문자 무시
					continue;

				// "EXIT" 입력 시 방 나가기 요청
				if (message == "EXIT")
				{
					// 방 나가기 요청 패킷을 생성하고 보냅니다.
					Protocol::C_LEAVE_ROOM_REQ reqPkt;
					if (g_session != nullptr)
					{
						auto sendBuffer = ServerPacketHandler::MakeSendBuffer(reqPkt);
						g_session->Send(sendBuffer);
						std::cout << "방 나가기 요청을 보냈습니다." << std::endl;
					}
					else
					{
						std::cout << "세션이 없어 방 나가기 요청을 보낼 수 없습니다." << std::endl;
					}
				}
				else
				{
					// 채팅 메시지 패킷을 생성하고 보냅니다.
					Protocol::C_ROOM_CHAT_REQ reqPkt;
					reqPkt.set_message(message);

					if (g_session != nullptr)
					{
						auto sendBuffer = ServerPacketHandler::MakeSendBuffer(reqPkt);
						g_session->Send(sendBuffer);
					}
				}
			}
			// 방에 들어가 있지 않다면 방 생성/입장 메뉴 표시
			else
			{
				std::cout << std::endl;
				std::cout << "[방 생성: 1] [방 입장: 2] -> ";
				std::string userInput;
				std::cin >> userInput;

				if (userInput == "1")
				{
					std::cout << "생성할 방 이름을 입력하세요: ";
					std::string roomName;
					std::cin >> roomName;
					//std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // 버퍼 비우기
					

					Protocol::C_CREATE_ROOM_REQ reqPkt;
					reqPkt.set_roomname(roomName);
					reqPkt.set_type(Protocol::ROOM_GROUP);

					if (g_session != nullptr)
					{
						auto sendBuffer = ServerPacketHandler::MakeSendBuffer(reqPkt);
						g_session->Send(sendBuffer);
						std::cout << "'" << roomName << "' 방 생성 요청을 보냈습니다." << std::endl;
					}
				}
				else if (userInput == "2")
				{
					std::cout << "입장할 방 번호를 입력하세요: ";
					int32 roomId;
					std::cin >> roomId;

					// 입력 버퍼에 숫자가 아닌 값이 들어왔을 경우 처리
					if (std::cin.fail())
					{
						std::cin.clear();
						//std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
						std::cout << "잘못된 입력입니다. 숫자를 입력해주세요." << std::endl;
						continue;
					}

					Protocol::C_JOIN_ROOM_REQ reqPkt;
					reqPkt.set_roomid(roomId);

					if (g_session != nullptr)
					{
						auto sendBuffer = ServerPacketHandler::MakeSendBuffer(reqPkt);
						g_session->Send(sendBuffer);
						std::cout << "방 ID " << roomId << " 입장 요청을 보냈습니다." << std::endl;
					}
				}
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	GThreadManager->Join();
}