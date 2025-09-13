# 📨 IocpChatServer

📑 이 프로젝트의 상세 설명과 성능 그래프/표는 PDF 포트폴리오에서 확인할 수 있습니다.  
👉 [자세한 내용 보러가기 (PDF)](./assets/IocpChatServer_Portfolio.pdf)

---

## 📌 개요
Windows IOCP 기반의 초저지연 채팅 서버입니다.  
세션 I/O 파이프라인, 송신 **Scatter–Gather WSASend** 배치, **커스텀 메모리/오브젝트 풀**을 직접 설계·구현하여 성능을 극대화했습니다.  

룸 단위 브로드캐스트는 **짧은 READ-락 + 스냅샷 전송**으로 경합을 최소화하며, 로그/DB 기록은 **Room JobQueue**에서 비동기 처리합니다.  

---

## ✨ 주요 기능
- IOCP 세션/이벤트 디스패치, 비동기 Recv/Send 파이프라인  
- SG-WSASend 기반 배치 전송  
  - (락 내부 = 큐 비우기, 락 외부 = WSABUF 구성)  
- `SendBuffer`(thread_local 청크), MemoryPool/ObjectPool(사이즈 클래스)  
- Room / RoomManager / Player 도메인, READ 스냅샷 기반 브로드캐스트  
- 로그인 · 방 생성 · 입장 · 채팅 · 나가기 전체 플로우 지원  
- DBConnectionPool(ODBC) 기반 인증/로그 기록  
- Room JobQueue를 통한 파일/DB 로그 비동기화  
- DummyClient를 활용한 동시 접속 · RPS · 메시지 페이로드 부하 시뮬레이션  

---

## 🏗 아키텍처
중앙의 **Session I/O Core** → 패킷 파싱 → `ClientPacketHandler` 라우팅 → `Room/RoomManager` 상태 변경 → 응답/브로드캐스트는 `Session::Send()`로 SG-WSASend 배치.  

- 브로드캐스트: **READ-락 스냅샷 후 해제 → 즉시 전송**  
- 로그/DB: **Room JobQueue**에서 비동기 처리  

📊 다이어그램: [`docs/iocpchatserver_core_centric.png`](./docs/iocpchatserver_core_centric.png)  

---

## 🛠 기술 스택
- **Language/Build**: C++17, Visual Studio  
- **Networking**: Win32 IOCP, Winsock2 (AcceptEx/ConnectEx/DisconnectEx, WSASend/WSARecv)  
- **Serialization**: Protobuf  
- **Database**: MS SQL Server (ODBC) + Connection Pool  
- **Infrastructure**: Custom MemoryPool/ObjectPool, RW Spinlock, Room JobQueue  
- **Custom Network Library**: 직접 설계한 Session, Buffer, JobQueue, MemoryPool 기반 네트워크 엔진  

---

## 📊 성능 지표
- **Broadcast scale**: ~255k deliveries/s (수신자 건수 기준)  
- **Throughput (app in/s)**: ~5.1k req/s  
- **Latency (server-side)**: 평균 ~0.45 ms, p99 ~1.97 ms  
- 상세 그래프/표는 별도 PDF 참고 (bc-deliv/s, app in/s, 지연 히스토그램 포함)  

---

## 🎥 시연 영상
👉 [시연 영상](https://…/iocpchatserver-demo.mp4)  
- 시나리오: 로그인 → 방 생성 → 채팅 → 나가기  

---

## 💻 대표 코드 스니펫
```cpp
// Room.cpp — Broadcast with READ-snapshot (no JobQueue on send path)
void Room::Broadcast(SendBufferRef sendBuffer)
{
    std::vector<PacketSessionRef> targets;
    targets.reserve(_players.size());
    {
        READ_LOCK; // short critical section
        for (const auto& kv : _players)
        {
            const auto& ply = kv.second;
            if (ply && ply->ownerSession)
                targets.push_back(ply->ownerSession);
        }
    }
    if (targets.empty()) return;

    LOG_SNAPSHOT_CTX(nullptr, 0, _roomId,
        "Broadcast snapshot members=%zu deliveries=%zu",
        targets.size(), targets.size());

    for (auto& sess : targets)
        sess->Send(sendBuffer);
}
````

---

## 🔮 향후 개선 방향

* 단일 노드 → **분산/멀티존 확장** (샤딩, 게이트웨이, 룸 파티셔닝)
* **모니터링/로깅 고도화** (지표 수집/대시보드, 구조화 로그)
* **게임 서버 기능 확장** (매치메이킹, 인벤토리, 추가 DB 연동)

---

## 📂 프로젝트 구조

```
IocpChatServer/
 ┣ ServerCore/       # IOCP Core, Session, RecvBuffer/SendBuffer, MemoryPool/ObjectPool, RW Spinlock, SocketUtils
 ┣ ChatServer/       # ChatServer, ChatSession, Player, Room, RoomManager, ClientPacketHandler, DBConnection, DBConnectionPool, Room JobQueue
 ┣ DummyClient/      # ServerSession, ServerPacketHandler, Load Profile (kClients/kRooms/kRps)
 ┗ docs/             # 다이어그램, 문서
```
