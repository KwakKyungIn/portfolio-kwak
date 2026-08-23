# 📨 IocpChatServer

📑 이 프로젝트의 상세 설명과 성능 그래프/표는 PDF 포트폴리오에서 확인할 수 있습니다.  
👉 [자세한 내용 보러가기 (PDF)](./assets/IocpChatServer_Portfolio.pdf)

👉 [코드 샘플 확인하기](./CodeSample)


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
- Protobuf 기반 패킷 정의 및 Python 스크립트 자동화 (핸들러 헤더 생성)
- DBConnectionPool(ODBC) 기반 인증/로그 기록  
- Room JobQueue를 통한 파일/DB 로그 비동기화  
- DummyClient를 활용한 동시 접속 · RPS · 메시지 페이로드 부하 시뮬레이션  

---

## 🏗 아키텍처
중앙의 **Session I/O Core** → 패킷 파싱 → `ClientPacketHandler` 라우팅 → `Room/RoomManager` 상태 변경 → 응답/브로드캐스트는 `Session::Send()`로 SG-WSASend 배치.  

- 브로드캐스트: **READ-락 스냅샷 후 해제 → 즉시 전송**  
- 로그/DB: **Room JobQueue**에서 비동기 처리
  
👉 아래 컴포넌트 다이어그램은 위 구조를 시각화한 것으로, 주요 모듈 간 관계와 데이터 흐름을 한눈에 확인할 수 있습니다.  
<br>

📊 다이어그램: <br><br>
![`assets/component.svg`](./assets/IocpChatServer_Component.svg)  

---
## 📂 프로젝트 구조

```
IocpChatServer/
 ┣ ServerCore/       # IOCP Core, Session, RecvBuffer/SendBuffer, MemoryPool/ObjectPool, RW Spinlock, SocketUtils
 ┣ ChatServer/       # ChatServer, ChatSession, Player, Room, RoomManager, ClientPacketHandler, DBConnection, DBConnectionPool, Room JobQueue
 ┗  DummyClient/      # ServerSession, ServerPacketHandler, Load Profile (kClients/kRooms/kRps)
```


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
👉 [시연 영상](https://youtu.be/LaYTojuuVZ0)  
![`assets/TestThumbnail.png`](./assets/TestThumbnail.png)  

- 시나리오: 로그인 → 방 생성 → 채팅 → 나가기  

👉 [부하테스트 시연 영상](https://youtu.be/05-56TYfodA)  
![`assets/LoadTestThumbnail.png`](./assets/LoadTestThumbnail.png) 
- WorkLoad : 500 Client, 10 Room, 10.0 Rps. 

---

## 🔮 향후 개선 방향

* 단일 노드 → **분산/멀티존 확장** (샤딩, 게이트웨이, 룸 파티셔닝)
* **모니터링/로깅 고도화** (지표 수집/대시보드, 구조화 로그)
* **게임 서버 기능 확장** (매치메이킹, 인벤토리, 추가 DB 연동)

---

## 💻 대표 코드 스니펫

### 1) SG-WSASend Batch Send (Session::RegisterSend)

**Source**: `ServerCore/Session.cpp`
**Role**: IOCP 송신 파이프라인의 핵심.

* (락 내부) `_sendQueue` 비우기 → `_sendEvent.sendBuffers`로 배치 이동 (상한선: `MAX_SEND_BATCH_BYTES/COUNT`)”
* (락 외부) WSABUF 배열 구성 → **Scatter–Gather WSASend(Overlapped)** 단일 호출
* 오류 시 WSA\_IO\_PENDING 외에는 즉시 복구 (레퍼런스/플래그 정리)

```cpp

void Session::RegisterSend()
{
	if (IsConnected() == false)
		return;

	_sendEvent.Init();
	_sendEvent.owner = shared_from_this(); // ADD_REF

	// 배치 작업
	{
		WRITE_LOCK;

		int32 batchedBytes = 0;
		while (_sendQueue.empty() == false)
		{
			SendBufferRef sb = _sendQueue.front();
			const int32 sz = static_cast<int32>(sb->WriteSize());

			// 첫 번째는 무조건 포함, 이후는 상한선 체크
			if (!_sendEvent.sendBuffers.empty())
			{
				if (batchedBytes + sz > MAX_SEND_BATCH_BYTES) break;
				if ((int32)_sendEvent.sendBuffers.size() >= MAX_SEND_BATCH_COUNT) break;
			}

			_sendQueue.pop();
			_sendEvent.sendBuffers.push_back(sb);
			batchedBytes += sz;

			// 백로그 장부 차감
			_sendBacklogBytes -= sz;
			_sendBacklogCount -= 1;
		}
	}

	// Scatter-Gather WSASend 준비
	Vector<WSABUF> wsaBufs;
	wsaBufs.reserve(_sendEvent.sendBuffers.size());
	for (SendBufferRef sb : _sendEvent.sendBuffers)
	{
		WSABUF b;
		b.buf = reinterpret_cast<char*>(sb->Buffer());
		b.len = static_cast<ULONG>(sb->WriteSize());
		wsaBufs.push_back(b);
	}

	DWORD numOfBytes = 0;
	if (SOCKET_ERROR == ::WSASend(_socket, wsaBufs.data(), (DWORD)wsaBufs.size(), OUT & numOfBytes, 0, &_sendEvent, nullptr))
	{
		const int32 err = ::WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			HandleError(err);
			_sendEvent.owner = nullptr;     // RELEASE_REF
			_sendEvent.sendBuffers.clear(); // RELEASE_REF
			_sendRegistered.store(false);   // 다시 시도 가능
		}
	}
}


```

---

### 2) Handle\_C\_LOGIN\_REQ (로그인 처리 + 세션 바인딩 with Metrics)

**Source**: `ChatServer/ClientPacketHandler.cpp`
**Role**: 로그인 요청 처리, DB 검증, 세션-플레이어 바인딩, 성능 메트릭 기록.

* `GMetrics` 카운터로 패킷/DB/커넥션 풀 상태 추적
* DB PreparedStatement로 안전하게 playerId 조회
* 성공 시 Player 객체 생성 후 ChatSession에 바인딩, `S_LOGIN_RES` 송신
* 실패 시 즉시 응답 패킷으로 사유 전달

```cpp
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

```

---

