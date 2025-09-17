using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

[Serializable]
public class ItemSlot
{
    public Item item;     // 슬롯에 들어있는 아이템
    public int count;     // 해당 아이템 개수

    public void Copy(ItemSlot slot)
    {
        item = slot.item;   // 아이템 복사
        count = slot.count; // 개수 복사
    }

    public void Set(Item item, int count)
    {
        this.item = item;   // 아이템 설정
        this.count = count; // 개수 설정
    }

    public void Clear()
    {
        item = null;  // 아이템 제거
        count = 0;    // 개수 초기화
    }
}

[CreateAssetMenu(menuName = "Data/Item Container")]
public class ItemContainer : ScriptableObject
{
    public List<ItemSlot> slots; // 슬롯 리스트 (인벤토리)

    public void Add(Item item, int count = 1)
    {
        if (item.stackable == true) // 스택 가능한 경우
        {
            ItemSlot itemSlot = slots.Find(x => x.item == item); // 같은 아이템 찾기
            if (itemSlot != null)
            {
                itemSlot.count += count; // 있으면 개수 추가
            }
            else
            {
                itemSlot = slots.Find(x => x.item == null); // 비어있는 슬롯 찾기
                if (itemSlot != null)
                {
                    itemSlot.item = item;   // 아이템 넣기
                    itemSlot.count = count; // 개수 설정
                }
            }
        }
        else // 스택 불가능한 경우
        {
            ItemSlot itemSlot = slots.Find(x => x.item == null); // 빈 슬롯 찾기
            if (itemSlot != null)
            {
                itemSlot.item = item; // 아이템만 배치 (개수는 항상 1)
            }
        }
    }

    public void Remove(Item itemToRemove, int count = 1)
    {
        if (itemToRemove.stackable) // 스택 가능한 경우
        {
            ItemSlot itemSlot = slots.Find(x => x.item == itemToRemove); // 해당 아이템 찾기
            if (itemSlot == null) return; // 없으면 종료

            itemSlot.count -= count; // 개수 차감

            if (itemSlot.count <= 0) // 0 이하가 되면
            {
                itemSlot.Clear(); // 슬롯 비우기
            }
        }
        else // 스택 불가능한 경우
        {
            while (count > 0) // 제거 개수만큼 반복
            {
                count -= 1;
                ItemSlot itemSlot = slots.Find(x => x.item == itemToRemove); // 해당 아이템 찾기
                if (itemSlot == null) return; // 없으면 종료
                itemSlot.Clear(); // 슬롯 비우기
            }
        }
    }
}
