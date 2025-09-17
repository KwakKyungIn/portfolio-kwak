## 🏗 아키텍처 개요

이 프로젝트의 전체 구조는 **플레이어 루프**, **월드 시스템**, **아이템 루프**, **코어 매니저** 네 가지 축을 중심으로 모듈화되어 있다.
아래 컴포넌트 다이어그램은 주요 시스템 간의 관계와 데이터/이벤트 흐름을 한눈에 보여준다.

📊 다이어그램:
![Unity2D\_FarmRPG\_Component](./assets/Unity2D_FarmRPG_Component.svg)
![Unity2D\_FarmRPG\_Component](./assets/Unity2D_FarmRPG_Architecture.svg)
---

### 🎮 플레이어 루프

* **입력 기반 제어**: `CharacterController2D`가 이동과 애니메이션을 담당
* **상호작용**: `CharacterInteractController`가 주변 `Interactable` 오브젝트 탐지 → `TalkInteract`, `LootContainerInteract`, `ResourceNode`와 연결
* **툴 사용**: `ToolsCharacterController`가 툴바(`ToolBarController`)에서 선택된 아이템을 바탕으로

  * 월드 액션(`GatherResourceNode`)
  * 타일 액션(`PlowTile`, `SeedTile`) 실행
* **결과**: 작물 심기/성장, 자원 채집, 대화 시작, 상자 열기 등 플레이 루프가 완결됨

---

### 🌍 월드 시스템

* **농사 시스템**:

  * `CropsManager`가 `TimeAgent`를 통해 시간 이벤트를 구독
  * Plow/Seed 액션 결과를 타일맵(`TileMapReadController`)에 반영
  * `MarkerManager`가 선택 타일을 시각적으로 표시
* **자원 시스템**:

  * `ResourceNode`(나무/광석)가 `GatherResourceNode` 툴 액션에 반응
  * `Hit()` 호출 시 `ItemSpawnManager`를 통해 아이템 드랍

---

### 🎒 아이템 루프

* **드랍**: `ItemSpawner`나 `ResourceNode`에서 `ItemSpawnManager`를 통해 월드에 `PickUpItem` 생성
* **습득**: 플레이어 접근 시 `PickUpItem`이 자동으로 인벤토리(`ItemContainer`)에 아이템을 추가
* **관리**: `InventoryController`로 인벤토리/툴바 UI를 토글, `ItemPannel`과 `ItemToolbarPannel`로 슬롯/버튼 표시
* **조작**: `ItemDragAndDropController`로 드래그·드랍 및 월드 재배치 지원
* **사용/소모**: 툴바 선택 → `ToolAction.OnApply()` 실행 → 필요 시 `OnItemUsed()`로 소모 처리

---

### 🛠 코어 매니저

* **GameManager**: 전역 단일 진입점, Player/Inventory/Dialogue/TimeController를 보관
* **GameSceneManager**: 씬 전환 담당 (Additive 로딩 + 플레이어 위치 갱신)
* **TimeAgent / DayTimeController**: 주기적 Tick 이벤트 발행 및 구독, 작물 성장·아이템 스폰 등 처리
* **ItemSpawnManager**: 전역 아이템 드랍 관리, Prefab 기반 `PickUpItem` 소환



---