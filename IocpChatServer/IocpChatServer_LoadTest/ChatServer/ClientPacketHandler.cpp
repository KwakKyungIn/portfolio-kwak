#include "pch.h"                       // 공용 헤더 (윈도우/런타임/프로젝트 전역 설정)
#include "ClientPacketHandler.h"       // 이 파일에서 등록/직접 호출할 핸들러 선언부
#include "Player.h"                    // 플레이어 엔티티(세션과 1:1로 매칭)
#include "Room.h"                      // 방 엔티티(브로드캐스트 단위)
#include "ChatSession.h"               // 채팅용 세션(패킷 수신/전송 주체)
#include "DBConnectionPool.h"          // DB 커넥션 풀(ODBC 기반)
#include "RoomManager.h"               // 전체 방 관리 (추가/검색/삭제)
#include <chrono>                      // 타임스탬프/지연 측정용
#include "Metrics.h"                   // [METRICS] 서버 런타임 지표 집계

// 패킷 디스패치 테이블 (패킷 ID로 함수 포인터 호출)
// - Init()에서 각 패킷 ID에 맞는 람다를 매핑
PacketHandlerFunc GPacketHandler[UINT16_MAX];

// ======================================================================
// 직접 컨텐츠 작업자 (서버가 "클라 → 서버" 패킷을 처리하는 영역)
// ======================================================================

// 잘못된/미등록 패킷 처리용 기본 핸들러
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
    GMetrics.app_packets_in.fetch_add(1, std::memory_order_relaxed); // [METRICS] 앱 레벨 IN 카운트
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);  // 앞 4바이트 [size,id] 해석
    // TODO : Log (패킷 ID 범위/무결성 체크 후 기록)
    return false;                                                     // 처리 실패로 반환 (디폴트)
}

// ===================== LOGIN =====================
// 클라이언트 로그인 요청 처리
bool Handle_C_LOGIN_REQ(PacketSessionRef& session, Protocol::C_LOGIN_REQ& pkt)
{
    GMetrics.app_packets_in.fetch_add(1, std::memory_order_relaxed); // [METRICS] 앱 IN

    ChatSessionRef chatSession = static_pointer_cast<ChatSession>(session); // 실제 채팅 세션으로 캐스팅

    DBConnection* dbConn = GDBConnectionPool->Pop();                 // DB 커넥션 하나 가져오기
    if (!dbConn)                                                     // 풀 고갈/오류 시
    {
        GMetrics.conn_acquire_fail.fetch_add(1, std::memory_order_relaxed); // [METRICS] 획득 실패

        Protocol::S_LOGIN_RES resPkt;                                // 실패 응답 생성
        resPkt.set_status(Protocol::CONNECT_FAIL);                   // 상태코드: 실패
        resPkt.set_reason("Failed to get DB connection. Check server status."); // 에러 사유
        session->Send(ClientPacketHandler::MakeSendBuffer(resPkt));  // 즉시 응답 송신
        return false;                                                // 처리 실패
    }

    GMetrics.conn_pool_pop_total.fetch_add(1, std::memory_order_relaxed); // [METRICS] 풀 pop 카운트

    int64 playerId = -1;                                             // DB에서 찾을 플레이어 ID
    bool success = false;                                            // 최종 성공 여부

    std::string name_str = pkt.name();                               // 프로토 버퍼 → std::string
    std::wstring wname_str(name_str.begin(), name_str.end());        // ODBC 바인딩을 위해 wide 문자열로 변환

    try
    {
        std::wstring query = L"SELECT playerId FROM Players WHERE name = ?"; // 파라미터 바인딩 쿼리
        if (dbConn->Prepare(query.c_str()))                         // 미리 컴파일(PreparedStatement)
        {
            GMetrics.db_query_count.fetch_add(1, std::memory_order_relaxed); // [METRICS] 쿼리 시도

            SQLLEN nameLen = SQL_NTS;                               // 유니코드 문자열 길이(널 종료)
            dbConn->BindParam(1,                                       // 1번 파라미터
                SQL_C_WCHAR,                              // C 타입: 유니코드
                SQL_WVARCHAR,                             // SQL 타입: NVARCHAR
                (SQLULEN)wname_str.length(),              // 길이(문자 단위)
                (SQLPOINTER)wname_str.c_str(),            // 버퍼 포인터
                &nameLen);                                // 길이 인디케이터

            if (dbConn->Execute())                                     // 실제 실행
            {
                dbConn->BindCol(1, SQL_C_LONG, sizeof(int64), &playerId, nullptr); // 1번째 컬럼을 playerId로 매핑
                success = dbConn->Fetch();                              // 결과 한 행 가져오기

                GMetrics.db_exec_ok.fetch_add(1, std::memory_order_relaxed); // [METRICS] 실행 성공
                if (success) GMetrics.db_fetch_ok.fetch_add(1, std::memory_order_relaxed);        // [METRICS] 데이터 있음
                else         GMetrics.db_fetch_no_data.fetch_add(1, std::memory_order_relaxed);   // [METRICS] 데이터 없음
            }
            else
            {
                GMetrics.db_exec_fail.fetch_add(1, std::memory_order_relaxed); // [METRICS] 실행 실패
            }
        }
        else
        {
            GMetrics.db_prepare_fail.fetch_add(1, std::memory_order_relaxed);   // [METRICS] Prepare 실패
        }
    }
    catch (...) { success = false; }                                   // 예외 발생 시 실패로 처리

    dbConn->Unbind();                                                  // 바인딩/커서/컬럼 해제
    GDBConnectionPool->Push(dbConn);                                   // 커넥션 풀에 반환
    GMetrics.conn_pool_push_total.fetch_add(1, std::memory_order_relaxed); // [METRICS] 풀 push

    Protocol::S_LOGIN_RES resPkt;                                      // 응답 패킷 구성
    if (success && playerId != -1)                                     // 정상적으로 유저 찾음
    {
        PlayerRef player = MakeShared<Player>();                        // 플레이어 객체 생성
        player->playerId = playerId;                                    // DB에서 조회한 ID
        player->name = pkt.name();                                      // 요청에 담긴 이름
        player->ownerSession = chatSession;                             // 역참조(세션 ↔ 플레이어)

        chatSession->SetPlayer(player);                                 // 세션에 플레이어 붙이기

        resPkt.set_status(Protocol::CONNECT_OK);                        // 로그인 성공
        resPkt.set_playerid(playerId);                                  // ID 회신
        resPkt.set_reason("Login successful.");                         // Reason 텍스트
        // 접속/플레이어 게이지는 세션 레벨에서 계측하는 흐름(포폴용 분리)
    }
    else
    {
        resPkt.set_status(Protocol::CONNECT_FAIL);                      // 실패 코드
        resPkt.set_reason("Player not found or DB error.");             // 실패 사유
    }

    session->Send(ClientPacketHandler::MakeSendBuffer(resPkt));         // 결과 응답 송신
    return true;                                                        // 처리 완료
}

// ===================== CREATE ROOM =====================
// 방 생성 요청 처리
bool Handle_C_CREATE_ROOM_REQ(PacketSessionRef& session, Protocol::C_CREATE_ROOM_REQ& pkt)
{
    GMetrics.app_packets_in.fetch_add(1, std::memory_order_relaxed);    // [METRICS] 앱 IN

    ChatSessionRef chatSession = static_pointer_cast<ChatSession>(session); // 세션 캐스팅

    PlayerRef player = chatSession->GetPlayer();                        // 세션에 매핑된 플레이어
    if (!player)                                                        // 로그인/세션 상태 점검
    {
        Protocol::S_CREATE_ROOM_RES resPkt;                             // 실패 응답
        resPkt.set_success(false);
        resPkt.set_reason("Player not found in session.");
        session->Send(ClientPacketHandler::MakeSendBuffer(resPkt));
        return false;
    }

    RoomRef newRoom = MakeShared<Room>();                               // 새 Room 인스턴스 생성
    int32 roomId = GRoomManager.AddRoom(newRoom);                       // 매니저에 등록하고 ID 할당

    newRoom->SetId(roomId);                                             // 방 ID 적용(로그 파일 오픈도 함께)
    newRoom->SetName(pkt.roomname());                                   // 클라에서 요청한 이름
    newRoom->SetType(pkt.type());                                       // 방 타입 설정(일반/길드 등)
    newRoom->Enter(player);                                             // 방에 플레이어 입장 처리

    Protocol::S_CREATE_ROOM_RES resPkt;                                 // 성공 응답 패킷
    resPkt.set_success(true);
    resPkt.set_reason("Room created successfully.");

    Protocol::RoomInfo* roomInfo = resPkt.mutable_room();               // Room 정보 채우기
    roomInfo->set_roomid(roomId);
    roomInfo->set_roomname(newRoom->GetName());
    roomInfo->set_type(newRoom->GetType());

    Protocol::PlayerInfo* playerInfo = roomInfo->add_members();         // 자신을 멤버 리스트에 추가
    playerInfo->set_playerid(player->playerId);
    playerInfo->set_name(player->name);

    session->Send(ClientPacketHandler::MakeSendBuffer(resPkt));         // 생성 결과 송신

    std::cout << "[RoomCreated] " << player->name << " -> Room " << roomId << std::endl; // 서버 로그
    return true;
}

// ===================== LEAVE ROOM =====================
// 방 퇴장 요청 처리
bool Handle_C_LEAVE_ROOM_REQ(PacketSessionRef& session, Protocol::C_LEAVE_ROOM_REQ& pkt)
{
    GMetrics.app_packets_in.fetch_add(1, std::memory_order_relaxed);    // [METRICS] 앱 IN

    ChatSessionRef chatSession = std::static_pointer_cast<ChatSession>(session); // 캐스팅
    if (!chatSession || !chatSession->GetPlayer())                      // 세션/플레이어 확인
        return false;

    PlayerRef player = chatSession->GetPlayer();                         // 현재 플레이어
    RoomRef room = player->_room;                                        // 플레이어가 속한 방

    if (!room)                                                           // 방이 없으면 성공으로 처리(멱등)
    {
        Protocol::S_LEAVE_ROOM_ACK ackPkt;                               // 바로 ACK
        ackPkt.set_success(true);
        session->Send(ClientPacketHandler::MakeSendBuffer(ackPkt));
        return true;
    }

    room->Leave(player);                                                 // 방에서 제거(락 내부 처리)
    GMetrics.room_leave.fetch_add(1, std::memory_order_relaxed);         // [METRICS] 퇴장 카운트
    player->_room = nullptr;                                             // 역참조 해제

    if (room->GetPlayers().empty())                                      // 방이 비었으면
    {
        GRoomManager.RemoveRoom(room->GetId());                          // 매니저에서 제거
        std::cout << "[RoomRemoved] Room " << room->GetId() << " deleted (empty)." << std::endl; // 서버 로그
        // rooms_gauge/peak는 RoomManager에서 관리 (역할 분리)
    }

    Protocol::S_ROOM_CHAT_NTF ntfPkt;                                    // 시스템 메시지로 나갔음을 알림
    static std::atomic<uint64> s_messageId{ 1 };                         // 서버 전역 메시지 ID(단순증가)
    Protocol::ChatMessage* chatMsg = ntfPkt.mutable_chat();

    chatMsg->set_messageid(s_messageId.fetch_add(1));                    // 고유 메시지 ID
    chatMsg->set_senderid(0);                                            // 0 = 시스템 발신자
    chatMsg->set_message("SYSTEM: " + player->name + " has left the room."); // 시스템 안내 문구

    auto now = std::chrono::system_clock::now();                         // 현재 시각
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count(); // 밀리초
    chatMsg->set_timestamp(ms);                                          // 타임스탬프 기록

    auto ntfBuffer = ClientPacketHandler::MakeSendBuffer(ntfPkt);        // 패킷을 송신 버퍼로 변환
    room->BroadcastWithoutSelf(ntfBuffer, player->playerId);             // 본인 제외하고 방송

    Protocol::S_LEAVE_ROOM_ACK resPkt;                                   // 최종 ACK
    resPkt.set_success(true);
    session->Send(ClientPacketHandler::MakeSendBuffer(resPkt));          // 전송

    return true;
}

// ===================== JOIN ROOM =====================
// 방 입장 요청 처리
bool Handle_C_JOIN_ROOM_REQ(PacketSessionRef& session, Protocol::C_JOIN_ROOM_REQ& pkt)
{
    GMetrics.app_packets_in.fetch_add(1, std::memory_order_relaxed);     // [METRICS] 앱 IN

    ChatSessionRef chatSession = std::static_pointer_cast<ChatSession>(session); // 캐스팅
    if (!chatSession || !chatSession->GetPlayer())                        // 세션/플레이어 점검
        return false;

    PlayerRef player = chatSession->GetPlayer();                          // 현재 플레이어
    RoomRef room = GRoomManager.FindRoom(pkt.roomid());                   // 매니저에서 해당 ID 방 찾기

    Protocol::S_JOIN_ROOM_RES resPkt;                                     // 응답 패킷
    resPkt.set_success(false);                                            // 기본값은 실패

    if (!room)                                                            // 방이 없으면
    {
        resPkt.set_reason("Cannot find the room");                        // 사유 기입
        session->Send(ClientPacketHandler::MakeSendBuffer(resPkt));       // 즉시 응답
        return true;                                                      // 처리 종료
    }

    bool joinSuccess = room->Enter(player);                               // 방에 입장 시도

    if (joinSuccess)
    {
        GMetrics.room_enter_ok.fetch_add(1, std::memory_order_relaxed);   // [METRICS] 입장 성공

        resPkt.set_success(true);                                         // 성공 표시

        Protocol::RoomInfo* roomInfo = resPkt.mutable_room();             // 방 정보 채우기
        roomInfo->set_roomid(room->GetId());
        roomInfo->set_type(room->GetType());
        roomInfo->set_roomname(room->GetName());

        for (const auto& member : room->GetPlayers())                     // 현재 멤버 리스트 추가
        {
            Protocol::PlayerInfo* memberInfo = roomInfo->add_members();
            memberInfo->set_playerid(member.second->playerId);
            memberInfo->set_name(member.second->name);
        }
    }
    else
    {
        GMetrics.room_enter_fail.fetch_add(1, std::memory_order_relaxed); // [METRICS] 입장 실패
        resPkt.set_reason("Failed to join the room");                     // 실패 사유
    }

    session->Send(ClientPacketHandler::MakeSendBuffer(resPkt));           // 결과 회신

    if (joinSuccess)                                                      // 성공 시 방에 방송
    {
        Protocol::S_JOIN_ROOM_NTF ntfPkt;                                 // 누가 들어왔는지 알림
        ntfPkt.set_name(player->name);
        auto ntfBuffer = ClientPacketHandler::MakeSendBuffer(ntfPkt);     // 버퍼화
        room->BroadcastWithoutSelf(ntfBuffer, player->playerId);          // 본인 제외 방송
    }

    return true;
}

// ===================== ROOM CHAT =====================
// 방 채팅 요청 처리
bool Handle_C_ROOM_CHAT_REQ(PacketSessionRef& session, Protocol::C_ROOM_CHAT_REQ& pkt)
{
    GMetrics.app_packets_in.fetch_add(1, std::memory_order_relaxed);      // [METRICS] 앱 IN

    ChatSessionRef chatSession = std::static_pointer_cast<ChatSession>(session); // 캐스팅
    if (!chatSession || !chatSession->GetPlayer())                        // 로그인/세션 점검
        return false;

    PlayerRef player = chatSession->GetPlayer();                           // 보내는 사람
    RoomRef room = player->_room;                                          // 현재 방

    if (!room)                                                             // 방이 없으면 무시(정상 흐름)
        return true;

    auto t0 = std::chrono::steady_clock::now();                            // [METRICS] 브로드캐스트 경로 지연 시작

    int32 roomId = room->GetId();                                          // 방 ID 보관

    Protocol::S_ROOM_CHAT_NTF resPkt;                                      // 수신자들에게 뿌릴 패킷
    resPkt.set_roomid(roomId);

    Protocol::ChatMessage* chatMsg = resPkt.mutable_chat();                // 메시지 필드 참조
    static std::atomic<uint64> s_messageId{ 1 };                           // 서버 전체 채팅 메시지 ID
    chatMsg->set_messageid(s_messageId.fetch_add(1));                      // 고유 ID 할당
    chatMsg->set_senderid(player->playerId);                               // 발신자 ID
    chatMsg->set_message(pkt.message());                                   // 본문(클라가 보낸 텍스트)

    auto now = std::chrono::system_clock::now();                           // 서버 기준 시각
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count(); // ms 단위
    chatMsg->set_timestamp(ms);                                            // 타임스탬프 기록

    auto sendBuffer = ClientPacketHandler::MakeSendBuffer(resPkt);         // 송신 버퍼 생성
    room->Broadcast(sendBuffer);                                           // 방 전체 브로드캐스트(락 내부에서 세션 모음)

    auto t1 = std::chrono::steady_clock::now();                            // [METRICS] 지연 측정 종료
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count(); // us 단위로 변환
    uint32_t us32 = (us <= 0) ? 0u : (us > INT32_MAX ? static_cast<uint32_t>(INT32_MAX) : static_cast<uint32_t>(us)); // 범위 보정
    GMetrics.ObserveBroadcastLatencyUS(us32);                              // 히스토그램에 기록

    room->LogChat(player->playerId, player->name, pkt.message());          // 파일 로그(비동기 JobQueue)
    room->LogChatToDB(player->playerId, player->name, pkt.message());      // DB 로그(비동기 JobQueue)

    return true;                                                           // 처리 완료
}

// ===================== ADMIN =====================
// 운영/관리자 명령(샘플) — 현재는 노출만
bool Handle_C_ADMIN_COMMAND_REQ(PacketSessionRef& session, Protocol::C_ADMIN_COMMAND_REQ& pkt)
{
    GMetrics.app_packets_in.fetch_add(1, std::memory_order_relaxed);       // [METRICS] 앱 IN
    return false;                                                          // 아직 미구현
}





