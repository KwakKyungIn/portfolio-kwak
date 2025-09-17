## 🛠 대표 코드 스니펫

### 1) 작물 성장 관리 (`CropsManager.cs`)

```csharp
private void Tick()
{
    foreach (CropTile cropTile in crops.Values)
    {
        if(cropTile.crop == null) continue;

        cropTile.growTimer += 1;

        if(cropTile.growTimer >= cropTile.crop.growthStageTime[cropTile.growStage])
        {
            cropTile.renderer.gameObject.SetActive(true);
            cropTile.renderer.sprite = cropTile.crop.sprites[cropTile.growStage];
            cropTile.growStage += 1;
        }
        if(cropTile.growTimer >= cropTile.crop.timeToGrow)
        {
            Debug.Log("자랄 준비 끝.");
            cropTile.crop = null;
        }
    }
}
````

✔ `TimeAgent`의 tick 이벤트마다 호출 → 작물 성장 단계 진행.

---

### 2) 아이템 획득 (`PickUpItem.cs`)

```csharp
private void Update()
{
    ttl -= Time.deltaTime;
    if (ttl <0) Destroy(gameObject);

    float distance = Vector3.Distance(transform.position, player.position);
    if (distance > PickUpDistance) return;

    transform.position = Vector3.MoveTowards(
        transform.position,
        player.position,
        speed * Time.deltaTime
    );

    if (distance < 0.1f)
    {
        GameManager.instance.inventoryContainer.Add(item, count);
        Destroy(gameObject);
    }
}
```

✔ 플레이어 근접 → 아이템 자동 습득, 인벤토리에 추가.

---

### 3) NPC 대화 (`DialogueSystem.cs`)

```csharp
public void Initialize(DialogueContainer dialogueContainer)
{
    Show(true);
    currentDialogue = dialogueContainer;
    currentTextLine = 0;
    CycleLine();
    UpdatePortrait();
}
```

✔ 대화 시작 시 UI 표시, 대사 라인 순차 출력, 초상화 갱신.

---

### 4) 툴 사용 (`ToolsCharacterController.cs`)

```csharp
private bool UseToolWorld()
{
    Vector2 position = rgbd2d.position + character.lastMotionVector * offsetDistance;
    Item item = toolbarController.GetItem;
    if(item == null || item.onAction == null) return false;

    bool complete = item.onAction.OnApply(position);
    if (complete && item.onItemUsed != null)
    {
        item.onItemUsed.OnItemUsed(item, GameManager.instance.inventoryContainer);
    }
    return complete;
}
```

✔ 툴바에서 선택된 아이템을 사용 → 월드/타일에 툴 액션 적용.

---