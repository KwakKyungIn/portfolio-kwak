# ✨ 주요 기능 (Features)

> 각 기능은 **플레이 관점 → 기술 관점(클래스/흐름) → 확장 포인트/엣지 케이스** 순으로 정리.

## 1) 캐릭터 이동 & 상호작용

**플레이**: 이동하면서 근처 상호작용 대상에 하이라이트가 켜지고, 마우스/키 조작으로 대화/채집/상자열기 등을 실행.

**기술**

* 이동/애니메이션: `CharacterController2D`

  * `Input.GetAxisRaw()`로 벡터 입력 → `Rigidbody2D.velocity` 반영
  * `lastMotionVector` 저장 → 정지 상태에서도 **마지막 바라보기 방향** 유지(대화/툴 사용 시 UX ↑)
* 상호작용: `CharacterInteractController`

  * **전방 오프셋 + 원형 탐지(OverlapCircleAll)** → `Interactable` 검색
  * `HighlightController`로 타겟 표시, 우클릭으로 `Interactable.Interact(character)` 호출

**확장/엣지**

* 상호작용 **우선순위(거리/각도/태그)** 옵션화
* 인터랙션 범위/레이어 **프로파일**로 분리 (전투/농사 모드 전환 등)

---

## 2) 농사(Plow→Seed→Grow→Harvest)

**플레이**: 괭이로 밭을 갈고, 씨앗을 심으면 시간이 지날수록 스프라이트가 성장 단계별로 바뀌며, 수확 타이밍을 제공.

**기술**

* 데이터: `Crop`(성장 총 시간, 단계별 시간·스프라이트, 수확 산출물/개수)
* 타일 액션: `PlowTile`, `SeedTile` (둘 다 `ToolAction` 상속)

  * `PlowTile`: 허용 타일만(`canPlow`) 밭갈기 → `CropsManager.Plow()`로 **CropTile 생성 + 타일맵 변경**
  * `SeedTile`: `CropsManager.Check()`로 밭 여부 확인 후 **씨앗 심기**, `OnItemUsed()`로 씨앗 소모
* 시간 이벤트: `CropsManager : TimeAgent`

  * `onTimeTick`에서 `growTimer/growStage` 진행, 단계별 스프라이트 적용
  * **완전 성장** 시 `crop = null`로 다음 상태(수확/제거) 전환 준비

**확장/엣지**

* **계절/수분/비료/병충해** 파라미터 추가 (성장 가중치)
* 수확 액션(예: 낫) 별도로 분리, 루팅 보상 `Item` 다변화
* **동시 타일 갱신** 시 타일맵 드로우 콜/오브젝트 풀 고려

---

## 3) 자원 채집(Tree/Ore 등)

**플레이**: 도끼/픽 등 맞는 도구로 노드를 타격하면 아이템이 뿌려지고, 노드는 파괴됨.

**기술**

* 노드: `ResourceNode : UsingTool`

  * `nodeType(Tree/Ore)`에 따라 `GatherResourceNode.canHitNodesOfType`과 매칭
  * `Hit()` → **랜덤 스프레드**로 드랍 산포 → `ItemSpawnManager.SpawnItem()`
* 툴 액션: `GatherResourceNode : ToolAction`

  * 전방 원형 탐지 → `UsingTool` 가져와 `CanBeHit()` 체크 → `Hit()` 실행

**확장/엣지**

* **노드 내구도/상태 머신**(깨짐 단계), 리스폰 타이머
* 도구 **등급(목/석/철)**에 따른 효율/가능 여부
* 드랍 테이블 **확률 가중치**/희귀 드랍

---

## 4) 아이템·인벤토리·툴바

**플레이**: 아이템이 바닥에 떨어지고, 근처면 자동으로 빨려 들어와 인벤토리에 쌓인다. `I`로 인벤토리 토글, 휠로 툴바 스왑, 슬롯 드래그/드랍으로 정리하거나 바닥에 버릴 수 있다.

**기술**

* 데이터: `Item`(stackable, icon, onAction/onTileMapAction/onItemUsed, 연결 작물 `crop`)
* 저장: `ItemContainer` + `ItemSlot`

  * 스택/비스택 **분기**가 구현되어 있음 (슬롯 검색, 카운트 증감/클리어)
* 습득: `PickUpItem`

  * 플레이어와 **거리 임계** 내면 `MoveTowards()`로 흡수, TTL 지나면 소멸
  * `GameManager.inventoryContainer.Add(item,count)`
* UI/조작:

  * `InventoryController`(I키로 Panel/Toolbar 토글)
  * `ItemPannel`/`ItemToolbarPannel`(버튼-슬롯 바인딩, 선택 하이라이트)
  * `ItemDragAndDropController`(마우스 따라다니는 아이콘, **UI 밖 클릭 시 월드 드랍**)
  * `ToolBarController`(휠 입력으로 인덱스 순환, `GetItem` 제공)

**확장/엣지**

* 장비/툴 내구도(`OnItemUsed`) 실제 감소, 스택 분할(Split)
* 아이템 설명 툴팁/퀵슬롯 키 매핑(1~0)
* 인벤토리 **정렬/필터**(소비/재료/퀘스트)

---

## 5) 대화 시스템

**플레이**: NPC에 접근해 상호작용하면 **타이핑 효과**로 대사가 출력되고, **배우 이름/초상화**가 함께 표시된다.

**기술**

* 데이터: `Actor`(이름/초상화), `DialogueContainer`(대사 라인 배열)
* 로직: `DialogueSystem`

  * 클릭으로 **타이핑 스킵 → 다음 라인**
  * `Initialize()`에서 UI 열고 `UpdatePortrait()`로 배우 정보 갱신
* 진입: `TalkInteract : Interactable`가 `GameManager.dialogueSystem.Initialize(dialogue)` 호출

**확장/엣지**

* **분기/선택지**, 변수 치환(플레이어 이름, 인벤토리 체크), 퀘스트 트리거
* 대사 로그/Auto-Advance/스킵

---

## 6) 씬/지역 전환 & 카메라

**플레이**: 특정 구역에 들어가면 워프하거나, 다른 씬으로 자연스럽게 이동.

**기술**

* 트리거: `TransitionArea`(Player 태그 감지 후 상위 `Transition` 호출)
* 전환: `Transition`

  * **Warp**: 자식 Transform 목적지 좌표로 이동 + `CinemachineBrain.ActiveVirtualCamera.OnTargetObjectWarped()` 호출로 카메라 보정
  * **Scene**: `GameSceneManager.SwitchScene()`(Additive Load/Unload) + 목표 좌표 배치, 카메라 보정

**확장/엣지**

* 씬 페이드, 포스트 프로세싱 전환
* **세이브 포인트**/리스폰 지점 관리

---

## 7) 시간 시스템 & 월드 이벤트

**플레이**: 시간이 흐르며 작물이 성장하고, 가끔씩 자원이 떨어진다.

**기술**

* 구독자: `TimeAgent`(임의 오브젝트가 쉽게 `onTimeTick`을 **구독/해지**)
* 발행자: `DayTimeController`(코드 외부) — `GameManager.timeController.Subscribe/Unsubscribe`
* 소비처: `CropsManager`(성장), `ItemSpawner`(확률 스폰)

**확장/엣지**

* **낮/밤/날씨/계절** 파라미터로 시스템 전반 가중치 적용
* 틱 주기/부하 분산(대량 타일/오브젝트)

---

## 8) 상자/오브젝트 상호작용

**플레이**: 상자를 열면 외형이 바뀐다. (루팅 기능은 후속 확장)

**기술**

* `LootContainerInteract : Interactable`

  * 최초 상호작용 시 **닫힘→열림** 상태 전환(프리팹 스왑)

**확장/엣지**

* 인벤토리 연동(상자 컨테이너), 잠금/키 시스템, 드랍 테이블

---