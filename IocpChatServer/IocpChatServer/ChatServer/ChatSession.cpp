#include "pch.h"
#include "ChatSession.h"
#include "ChatSessionManager.h"
#include "ClientPacketHandler.h"
#include "Log.h"

// 세션이 연결되었을 때 호출되는 콜백
void ChatSession::OnConnected()
{
    // 새로 연결된 ChatSession을 세션 매니저에 등록
    GSessionManager.Add(static_pointer_cast<ChatSession>(shared_from_this()));
    LOG_SNAPSHOT_CTX(this, 0, 0, "OnConnected()");
}

// 세션이 끊어졌을 때 호출되는 콜백
void ChatSession::OnDisconnected()
{
	LOG_SNAPSHOT_CTX(this, 0, 0, "OnDisconnected()");
    // 연결이 종료된 ChatSession을 세션 매니저에서 제거
    GSessionManager.Remove(static_pointer_cast<ChatSession>(shared_from_this()));
}

// 패킷 수신 시 호출되는 콜백
void ChatSession::OnRecvPacket(BYTE* buffer, int32 len)
{
    PacketSessionRef session = GetPacketSessionRef();          // 현재 세션 참조 얻기
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer); // 버퍼 앞부분을 패킷 헤더로 캐스팅

    // TODO: 패킷 ID 유효 범위 검사 필요 (보안/안정성)
    ClientPacketHandler::HandlePacket(session, buffer, len);   // 패킷 처리기로 전달
}

// 패킷 전송 완료 시 호출되는 콜백
void ChatSession::OnSend(int32 len)
{
    // 현재는 특별한 처리 없음 (로그나 통계용으로 확장 가능)
}
