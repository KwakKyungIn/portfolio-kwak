using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

[CreateAssetMenu(menuName = "Data/Item")] // ScriptableObject 생성 메뉴에 추가
public class Item : ScriptableObject
{
    public string Name;            // 아이템 이름
    public bool stackable;         // 여러 개 쌓일 수 있는지 여부
    public Sprite icon;            // 아이템 아이콘
    public ToolAction onAction;    // 월드에서 사용 시 동작
    public ToolAction onTileMapAction; // 타일에서 사용 시 동작
    public ToolAction onItemUsed;  // 사용 후 호출되는 동작(내구도 소모 등)
    public Crop crop;              // 연결된 작물 데이터(씨앗일 경우)
}
