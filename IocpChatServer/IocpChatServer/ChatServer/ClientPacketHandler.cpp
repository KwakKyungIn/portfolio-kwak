#include "pch.h"                       // 공용 헤더 (윈도우/런타임/프로젝트 전역 설정)
#include "ClientPacketHandler.h"       // 이 파일에서 등록/직접 호출할 핸들러 선언부
#include "Player.h"                    // 플레이어 엔티티(세션과 1:1로 매칭)
#include "Room.h"                      // 방 엔티티(브로드캐스트 단위)
#include "ChatSession.h"               // 채팅용 세션(패킷 수신/전송 주체)
#include "DBConnectionPool.h"          // DB 커넥션 풀(ODBC 기반)
#include "RoomManager.h"               // 전체 방 관리 (추가/검색/삭제)
#include <chrono>                      // 타임스탬프/지연 측정용
#include "Log.h"                       // [LOG] 스냅샷/이벤트 로깅용

// 패킷 디스패치 테이블 (패킷 ID로 함수 포인터 호출)
// - Init()에서 각 패킷 ID에 맞는 람다를 매핑
PacketHandlerFunc GPacketHandler[UINT16_MAX];

// ======================================================================
// 직접 컨텐츠 작업자 (서버가 "클라 → 서버" 패킷을 처리하는 영역)
// ======================================================================

// 잘못된/미등록 패킷 처리용 기본 핸들러
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);  // 앞 4바이트 [size,id] 해석
    // TODO : Log (패킷 ID 범위/무결성 체크 후 기록)
    return false;                                                     // 처리 실패로 반환 (디폴트)
}

// ===================== LOGIN =====================
// 클라이언트 로그인 요청 처리
bool Handle_C_LOGIN_REQ(PacketSessionRef& session, Protocol::C_LOGIN_REQ& pkt)
{
    ChatSessionRef chatSession = static_pointer_cast<ChatSession>(session); // 실제 채팅 세션으로 캐스팅
    DBConnection* dbConn = GDBConnectionPool->Pop();                        // DB 커넥션 하나 가져오기

    LOG_SNAPSHOT_CTX(chatSession.get(), 0, 0, "C_LOGIN_REQ name=\"%s\"", pkt.name().c_str()); // [LOG] 로그인 요청 스냅샷

    if (dbConn == nullptr) {                                                 // 풀 고갈/오류 시
        Protocol::S_LOGIN_RES resPkt;                                       // 실패 응답 생성
        resPkt.set_status(Protocol::CONNECT_FAIL);                          // 상태코드: 실패
        resPkt.set_reason("Failed to get DB connection. Check server status."); // 에러 사유
        session->Send(ClientPacketHandler::MakeSendBuffer(resPkt));         // 즉시 응답 송신
        return false;                                                       // 처리 실패
    }

    std::string name_str = pkt.name();                                      // 프로토 버퍼 → std::string
    std::wstring wname_str(name_str.begin(), name_str.end());               // ODBC 바인딩용 wide 문자열로 변환
    std::wstring query = L"SELECT playerId FROM Players WHERE name = '" + wname_str + L"'"; // 단순 쿼리 구성

    int64 playerId = 0;
    bool success = dbConn->Execute(query.c_str());                          // 직접 실행 (Prepare 없이)
    if (success) {
        dbConn->BindCol(1, SQL_C_LONG, sizeof(int64), &playerId, nullptr);  // 첫 번째 컬럼을 playerId로 매핑
        dbConn->Fetch();                                                    // 결과 한 행 가져오기
    }

    dbConn->Unbind();                                                       // 바인딩 해제
    GDBConnectionPool->Push(dbConn);                                        // 커넥션 풀에 반환

    Protocol::S_LOGIN_RES resPkt;                                           // 응답 패킷 구성

    if (playerId != -1) {                                                    // 정상적으로 유저 찾음
        PlayerRef player = MakeShared<Player>();                            // 플레이어 객체 생성
        player->playerId = playerId;                                        // DB에서 조회한 ID
        player->name = pkt.name();                                          // 요청에 담긴 이름
        player->ownerSession = chatSession;                                 // 역참조(세션 ↔ 플레이어)

        chatSession->SetPlayer(player);                                     // 세션에 플레이어 붙이기

        resPkt.set_status(Protocol::CONNECT_OK);                            // 로그인 성공
        resPkt.set_playerid(playerId);                                      // ID 회신
        resPkt.set_reason("Login successful.");                             // Reason 텍스트
    }
    else {
        resPkt.set_status(Protocol::CONNECT_FAIL);                          // 실패 코드
        resPkt.set_reason("Player not found or DB error.");                 // 실패 사유
    }
    auto sendBuffer = ClientPacketHandler::MakeSendBuffer(resPkt);          // 버퍼 변환
    session->Send(sendBuffer);                                              // 결과 응답 송신

    LOG_SNAPSHOT_CTX(chatSession.get(), (uint64_t)playerId, 0,              // [LOG] 결과 스냅샷
        "S_LOGIN_RES success=%s", success ? "true" : "false");

    return true;                                                            // 처리 완료
}

// ===================== CREATE ROOM =====================
// 방 생성 요청 처리
bool Handle_C_CREATE_ROOM_REQ(PacketSessionRef& session, Protocol::C_CREATE_ROOM_REQ& pkt)
{
    ChatSessionRef chatSession = static_pointer_cast<ChatSession>(session); // 세션 캐스팅
    PlayerRef player = chatSession->GetPlayer();                            // 세션에 매핑된 플레이어

    LOG_SNAPSHOT_CTX(chatSession.get(), player->playerId, 0,                // [LOG] 방 생성 요청 스냅샷
        "C_CREATE_ROOM_REQ name=\"%s\"", pkt.roomname().c_str());

    if (player == nullptr)                                                  // 로그인/세션 상태 점검
    {
        Protocol::S_CREATE_ROOM_RES resPkt;                                 // 실패 응답
        resPkt.set_success(false);
        resPkt.set_reason("Player not found in session.");
        session->Send(ClientPacketHandler::MakeSendBuffer(resPkt));
        return false;
    }

    RoomRef newRoom = MakeShared<Room>();                                   // 새 Room 인스턴스 생성
    int32 roomId = GRoomManager.AddRoom(newRoom);                           // 매니저에 등록하고 ID 할당

    LOG_SNAPSHOT_CTX(chatSession.get(), player->playerId, roomId,           // [LOG] 방 ID 할당 로그
        "RoomManager.AddRoom → roomId=%d", roomId);

    newRoom->SetId(roomId);                                                 // 방 ID 적용(로그 파일 오픈도 함께)
    newRoom->SetName(pkt.roomname());                                       // 클라에서 요청한 이름
    newRoom->SetType(pkt.type());                                           // 방 타입 설정
    newRoom->Enter(player);                                                 // 방에 플레이어 입장 처리

    Protocol::S_CREATE_ROOM_RES resPkt;                                     // 성공 응답 패킷
    resPkt.set_success(true);
    resPkt.set_reason("Room created successfully.");

    Protocol::RoomInfo* roomInfo = resPkt.mutable_room();                   // Room 정보 채우기
    roomInfo->set_roomid(roomId);
    roomInfo->set_roomname(newRoom->GetName());
    roomInfo->set_type(newRoom->GetType());

    Protocol::PlayerInfo* playerInfo = roomInfo->add_members();             // 자신을 멤버 리스트에 추가
    playerInfo->set_playerid(player->playerId);
    playerInfo->set_name(player->name);

    LOG_SNAPSHOT_CTX(chatSession.get(), player->playerId, roomId,           // [LOG] 방 초기화 완료 로그
        "Room.Init SetId/Name/Type; Enter(player=%llu)", (unsigned long long)player->playerId);

    session->Send(ClientPacketHandler::MakeSendBuffer(resPkt));             // 생성 결과 송신
    LOG_SNAPSHOT_CTX(chatSession.get(), player->playerId, roomId,           // [LOG] 생성 성공 스냅샷
        "S_CREATE_ROOM_RES success=true");

    std::cout << "Player " << player->name << " created a room. Room ID: " << roomId << std::endl; // 서버 로그
    return true;
}

// ===================== LEAVE ROOM =====================
// 방 퇴장 요청 처리
bool Handle_C_LEAVE_ROOM_REQ(PacketSessionRef& session, Protocol::C_LEAVE_ROOM_REQ& pkt)
{
    ChatSessionRef chatSession = std::static_pointer_cast<ChatSession>(session); // 캐스팅

    if (chatSession == nullptr || chatSession->GetPlayer() == nullptr)           // 세션/플레이어 확인
    {
        return false;
    }
    PlayerRef player = chatSession->GetPlayer();                                 // 현재 플레이어

    RoomRef room = player->_room;                                                // 플레이어가 속한 방
    LOG_SNAPSHOT_CTX(chatSession.get(), player ? player->playerId : 0, room ? room->GetId() : 0,
        "C_LEAVE_ROOM_REQ");                                                     // [LOG] 퇴장 요청

    if (room == nullptr)                                                         // 방이 없으면 성공으로 처리(멱등)
    {
        Protocol::S_LEAVE_ROOM_ACK ackPkt;                                       // 바로 ACK
        ackPkt.set_success(true);
        auto resBuffer = ClientPacketHandler::MakeSendBuffer(ackPkt);
        session->Send(resBuffer);
        return true;
    }

    room->Leave(player);                                                         // 방에서 제거(락 내부 처리)
    if (room->GetPlayers().empty())                                              // 방이 비었으면
    {
        GRoomManager.RemoveRoom(room->GetId());                                  // 매니저에서 제거
        std::cout << "방 ID " << room->GetId() << "이(가) 모든 플레이어가 나가서 삭제되었습니다." << std::endl; // 서버 로그
    }

    Protocol::S_ROOM_CHAT_NTF ntfPkt;                                            // 시스템 메시지로 알림
    static std::atomic<uint64> s_messageId{ 1 };                                 // 서버 전역 메시지 ID(단순증가)
    Protocol::ChatMessage* chatMsg = ntfPkt.mutable_chat();

    chatMsg->set_messageid(s_messageId.fetch_add(1));                            // 고유 메시지 ID
    chatMsg->set_senderid(0);                                                    // 0 = 시스템 발신자
    chatMsg->set_message("SYSTEM: " + player->name + " has left the room.");     // 안내 문구

    auto now = std::chrono::system_clock::now();                                 // 현재 시각
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    chatMsg->set_timestamp(ms);                                                  // 타임스탬프 기록

    auto ntfBuffer = ClientPacketHandler::MakeSendBuffer(ntfPkt);                // 패킷을 송신 버퍼로 변환
    room->BroadcastWithoutSelf(ntfBuffer, player->playerId);                     // 본인 제외하고 방송

    Protocol::S_LEAVE_ROOM_ACK resPkt;                                           // 최종 ACK
    resPkt.set_success(true);
    auto resBuffer = ClientPacketHandler::MakeSendBuffer(resPkt);
    session->Send(resBuffer);
    LOG_SNAPSHOT_CTX(chatSession.get(), player->playerId, room->GetId(),         // [LOG] 퇴장 성공 로그
        "S_LEAVE_ROOM_ACK success=true");
    return true;
}

// ===================== JOIN ROOM =====================
// 방 입장 요청 처리
bool Handle_C_JOIN_ROOM_REQ(PacketSessionRef& session, Protocol::C_JOIN_ROOM_REQ& pkt)
{
    ChatSessionRef chatSession = std::static_pointer_cast<ChatSession>(session); // 캐스팅
    if (chatSession == nullptr || chatSession->GetPlayer() == nullptr) {          // 세션/플레이어 점검
        return false;
    }
    PlayerRef player = chatSession->GetPlayer();                                 // 현재 플레이어

    LOG_SNAPSHOT_CTX(chatSession.get(), player ? player->playerId : 0, 0,        // [LOG] 조인 요청 로그
        "C_JOIN_ROOM_REQ roomId=%d", pkt.roomid());

    int64 roomId = pkt.roomid();
    RoomRef room = GRoomManager.FindRoom(roomId);                                // 매니저에서 해당 ID 방 찾기

    Protocol::S_JOIN_ROOM_RES resPkt;                                            // 응답 패킷
    resPkt.set_success(false);                                                   // 기본값은 실패

    if (room == nullptr) {                                                        // 방이 없으면
        resPkt.set_reason("Cannot find the room");                               // 사유 기입
        auto sendBuffer = ClientPacketHandler::MakeSendBuffer(resPkt);           // 즉시 응답
        session->Send(sendBuffer);
        return true;
    }

    bool joinSuccess = room->Enter(player);                                      // 방에 입장 시도

    LOG_SNAPSHOT_CTX(chatSession.get(), player->playerId, roomId,                // [LOG] 입장 시도 로그
        "Enter OK (WRITE)");

    if (joinSuccess) {                                                            // 성공 시
        resPkt.set_success(true);

        Protocol::RoomInfo* roomInfo = resPkt.mutable_room();                    // 방 정보 채우기
        roomInfo->set_roomid(room->GetId());
        roomInfo->set_type(room->GetType());
        roomInfo->set_roomname(room->GetName());

        for (const auto& member : room->GetPlayers()) {                           // 현재 멤버 리스트 추가
            Protocol::PlayerInfo* memberInfo = roomInfo->add_members();
            memberInfo->set_playerid(member.second->playerId);
            memberInfo->set_name(member.second->name);
        }
    }
    else {
        resPkt.set_reason("방 입장에 실패했습니다.");                                 // 실패 사유
    }

    auto sendBuffer = ClientPacketHandler::MakeSendBuffer(resPkt);               // 결과 회신
    session->Send(sendBuffer);

    if (joinSuccess) {                                                            // 성공 시 방에 방송
        Protocol::S_JOIN_ROOM_NTF ntfPkt;                                        // 누가 들어왔는지 알림
        ntfPkt.set_name(player->name);
        auto ntfBuffer = ClientPacketHandler::MakeSendBuffer(ntfPkt);            // 버퍼화
        room->BroadcastWithoutSelf(ntfBuffer, player->playerId);                 // 본인 제외 방송
    }

    LOG_SNAPSHOT_CTX(chatSession.get(), player->playerId, roomId,                // [LOG] 결과 스냅샷
        "S_JOIN_ROOM_RES success=true");

    return true;                                                                 // 처리 완료
}

// ===================== ROOM CHAT =====================
// 방 채팅 요청 처리
bool Handle_C_ROOM_CHAT_REQ(PacketSessionRef& session, Protocol::C_ROOM_CHAT_REQ& pkt)
{
    ChatSessionRef chatSession = std::static_pointer_cast<ChatSession>(session); // 캐스팅
    if (chatSession == nullptr || chatSession->GetPlayer() == nullptr)           // 로그인/세션 점검
        return false;

    PlayerRef player = chatSession->GetPlayer();                                 // 보내는 사람
    RoomRef room = player->_room;                                                // 현재 방

    if (room == nullptr)                                                         // 방이 없으면 무시(정상 흐름)
        return true;

    int32 roomId = room->GetId();                                                // 방 ID 보관

    Protocol::S_ROOM_CHAT_NTF resPkt;                                            // 수신자들에게 뿌릴 패킷
    resPkt.set_roomid(roomId);

    Protocol::ChatMessage* chatMsg = resPkt.mutable_chat();                      // 메시지 필드 참조
    static std::atomic<uint64> s_messageId{ 1 };                                 // 서버 전체 채팅 메시지 ID
    chatMsg->set_messageid(s_messageId.fetch_add(1));                            // 고유 ID 할당
    chatMsg->set_senderid(player->playerId);                                     // 발신자 ID
    chatMsg->set_message(pkt.message());                                         // 본문(클라가 보낸 텍스트)

    auto now = std::chrono::system_clock::now();                                 // 서버 기준 시각
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count(); // ms 단위
    chatMsg->set_timestamp(ms);                                                  // 타임스탬프 기록

    auto sendBuffer = ClientPacketHandler::MakeSendBuffer(resPkt);               // 송신 버퍼 생성
    room->Broadcast(sendBuffer);                                                 // 방 전체 브로드캐스트

    LOG_SNAPSHOT_CTX(chatSession.get(), player->playerId, roomId,                // [LOG] 채팅 로그
        "C_ROOM_CHAT_REQ bytes=%zu", pkt.message().size());

    return true;                                                                 // 처리 완료
}

// ===================== ADMIN =====================
// 운영/관리자 명령(샘플) — 현재는 노출만
bool Handle_C_ADMIN_COMMAND_REQ(PacketSessionRef& session, Protocol::C_ADMIN_COMMAND_REQ& pkt)
{
    return false;                                                                // 아직 미구현
}
