
# Kwak Portfolio
 <img src="assets/kwakkyungin.jpg" width="180" align="left" alt="Kwak Kyung-In" />

&emsp;&emsp;***🎓 명지대학교 정보통신공학과 · 졸업예정***<br>
&emsp;&emsp;***💻 희망 포지션: 게임 서버 프로그래머***<br>
<br>
&emsp;&emsp;게임 서버 개발자를 꿈꾸며, 네트워크와 시스템 기본기에 충실한 성장을 추구합니다.<br>
&emsp;&emsp;복잡한 기술보다 안정적인 구조와 원리를 이해하고자 하며,<br>
&emsp;&emsp;꾸준한 학습과 실험으로 탄탄한 기반을 다지고 있습니다.<br>
<br>
&emsp;&emsp;🛠 주요 스택: C++ · C# · Win32 IOCP · Redis · MS SQL · Protocol Buffers · Unity · Prometheus/Grafana<br>
&emsp;&emsp;✨ “기본에 충실한 코드, 데이터로 증명하는 성능”을 지향합니다.
<br clear="left"/>

---

<br><br>
# 🚀 Representative Projects<br>

> ## 🔸 mmo server project
> 멀티 서버 구조로 설계한 **C++ 기반 MMO 서버 프로젝트**.<br>
> `LoginServer · GameServer · DBAgent`를 분리하고, Redis 토큰 인증과 write-back persistence, RoomActor/JobQueue 기반 동시성 제어를 적용해 **로그인 → 월드 입장 → 이동·전투 → 파티·던전·거래 → 저장**까지 이어지는 핵심 플레이 루프를 구현했습니다.
>
> <details>
> <summary><b>🔍 상세 기술 경험 및 프로젝트 링크 (클릭)</b></summary>
>
> ### 📊 Server Highlight
> - **멀티 서버 구조**: LoginServer / GameServer / DBAgent 분리, Protobuf 기반 C2S·S2S 프로토콜 설계, Redis 토큰 인증과 MSSQL 연동
> - **실시간 월드 처리**: AOI/SpatialGrid 기반 가시성 관리, 이동 검증, 몬스터 AI, 스킬·투사체 전투 로직 구현
> - **MMO 시스템 구현**: 인벤토리·장비·퀵슬롯, 파티/파티 채팅, 인스턴스 던전, 거래 및 골드 저장 흐름 구현
> - **검증 및 운영 경험**: Unity 클라이언트로 기능을 검증하고, Prometheus/Grafana 기반 혼합 부하 테스트로 **3채널 900 CCU를 60분간 안정적으로 유지**한 결과를 확보
>
> ### 🔗 [프로젝트 링크](./MmoProject/README.md)
> </details>

---
> ## 🔸 IocpChatServer
> 고성능 비동기 IOCP 기반 채팅 서버.<br>
> 로그인, 방 생성·참여·채팅 등 기본 기능과 성능 최적화 구조를 구현했습니다.
>
> <details>
> <summary><b>📊 성능 지표 및 프로젝트 링크 (클릭)</b></summary>
>
> ### 📊 Performance Highlight
> - 500 Clients · 10 Rooms 환경에서 안정적 처리 검증
> - Broadcast 처리량: **초당 약 25만 메시지**
> - 평균 지연 시간: **~0.4ms (p50 0.34ms, p99 2ms)**
>
> ### 🔗 [프로젝트 링크](./IocpChatServer/README.md)
> </details>

---
> ## 🔸 CapstoneDesign project : Unity 2D Farm RPG
> 타일 기반 **농사·채집·대화 프로토타입**.<br>
> ScriptableObject 기반 데이터 주도 설계로, 인벤토리·툴바·시간 시스템까지 연결된 **완전한 플레이 루프**를 구현했습니다.
>
> <details>
> <summary><b>🎮 구현 기능 및 프로젝트 링크 (클릭)</b></summary>
>
> ### 🎮 Gameplay Highlight
> - **플레이 루프**: 이동 → 상호작용 → 농사/채집 → 아이템 수집 → 인벤토리/툴 사용 → 성장/대화 → 지역 이동
> - **모듈화 설계**: 툴 액션, 자원 노드, 타일맵, 시간 이벤트를 분리하여 **확장성 확보**
> - **UX 디테일**: 하이라이트, 자동 아이템 흡수, 툴바 휠 전환, 드래그&드랍 인벤토리
> - **씬 전환 안정화**: Additive Scene + Cinemachine OnTargetObjectWarped 활용
>
> ### 🔗 [프로젝트 링크](./CapstoneDesign1_Unity2DFarmRPG/README.md)
> </details>

---

> ## 🔸 CapstoneDesign project - Lakrmir : Unity 2D Multiplayer Action Prototype
>
> Photon PUN2 기반 **멀티플레이 액션 프로토타입**.<br>
> 로비/방 생성·참여·Ready → 멀티 씬 진입 → **플레이어 이동·점프·대쉬·공격 동기화**와 **마스터 클라이언트 몬스터/보스 제어**까지 이어지는 멀티플레이 루프를 구현했습니다.
>
> <details>
> <summary><b>🎮 멀티플레이 핵심 요소 및 프로젝트 링크 (클릭)</b></summary>
>
> ### 🎮 Multiplayer Highlight
> - **멀티 시작 플로우**: 닉네임 입력 → 모드 선택 → 방 생성·참여 → Ready 동기화 → 씬 진입
> - **플레이어 네트워크 동기화**: 이동/점프/대쉬/근·원거리/실드 공격을 SerializeView + RPC로 처리, 보간(LERP) 적용
> - **권한 모델 설계**: 플레이어는 Owner 입력 주도, 몬스터/보스는 MasterClient 단일 권한으로 AI·패턴·투사체 관리
> - **투사체 복제 전략**: 플레이어 화살 = 로컬 발사 + RPC 복제(반응성), 몬스터 화살 = 네트워크 Instantiate(일관성)
> - **UI & UX 연결**: MainMenu/MultiSelect에서 로비 UI → PhotonNetwork 씬 전환까지 자연스럽게 연동
>
> ### 🔗 [프로젝트 링크](./CapstoneDesign2_Lakemir/README.md)
> </details>

---
> 각 프로젝트의 상세 내용과 실행 방법은 프로젝트 폴더의 README를 참고하세요.
---
<br><br>
## 📖 Study Repositories

💡 **프로젝트 경험뿐만 아니라, 서버 개발에 필요한 기반 지식을 체계적으로 다지기 위해 별도의 학습 저장소를 운영하고 있습니다.** - 운영체제, 네트워크, C++ 문법 및 심화 주제를 정리하며, 단순한 이론 습득을 넘어 실제 프로젝트에 어떻게 적용할 수 있는지까지 기록했습니다.

🔗 [공부 저장소 바로가기](https://github.com/KwakKyungIn/Server-Job-Prep)  


---

## 🛠 Tech Stack
- **Language**: C++, C#
- **Server / Concurrency**: Win32 IOCP, 멀티스레딩, 비동기 I/O, JobQueue / Actor Model
- **Data / Infra**: MSSQL (ODBC), Redis, Protocol Buffers
- **Client / Tools**: Unity, Visual Studio, Prometheus/Grafana, Git/GitHub

---

## 📬 Contact
- **Email**: rhkrruddls19999@gmail.com  
- **GitHub**: [kwakkyungin](https://github.com/kwakkyungin)  

---
