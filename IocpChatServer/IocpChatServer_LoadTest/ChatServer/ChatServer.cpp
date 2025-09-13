#include "pch.h"
#include "ThreadManager.h"
#include "Service.h"
#include "Session.h"
#include "ChatSession.h"
#include "ChatSessionManager.h"
#include "BufferWriter.h"
#include "ClientPacketHandler.h"
#include <tchar.h>
#include "Protocol.pb.h"
#include "Room.h"
#include "Player.h"
#include "DBConnectionPool.h"
#include "RoomManager.h"
#include <iostream>

int main()
{
    // 1. DB 연결만 확인 (테이블 drop/create/delete/insert X)
    ASSERT_CRASH(GDBConnectionPool->Connect(
        16,
        L"Driver={ODBC Driver 18 for SQL Server};Server=localhost;Database=test2;Trusted_Connection=Yes;Encrypt=Yes;TrustServerCertificate=Yes;"
    ));

    std::cout << "DB Connection established." << std::endl;

    // 2. 패킷 핸들러 초기화
    ClientPacketHandler::Init();

    // 3. 서버 서비스 시작
    ServerServiceRef service = MakeShared<ServerService>(
        NetAddress(L"127.0.0.1", 7777),
        MakeShared<IocpCore>(),
        MakeShared<ChatSession>,
        500);

    ASSERT_CRASH(service->Start());

    // 4. 스레드 런칭
    for (int32 i = 0; i < std::thread::hardware_concurrency(); i++)  // 예: 8~16
    {
        GThreadManager->Launch([=]() {
            while (true) service->GetIocpCore()->Dispatch();
            });
    }

    GThreadManager->Join();
    return 0;
}
