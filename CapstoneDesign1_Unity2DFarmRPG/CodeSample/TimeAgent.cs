using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System;

public class TimeAgent : MonoBehaviour
{
    public Action onTimeTick; // 시간 이벤트가 발생했을 때 실행할 델리게이트

    void Start()
    {
        Init(); // 시작 시 초기화 실행
    }

    public void Init()
    {
        // GameManager의 timeController에 자신을 구독자로 등록
        GameManager.instance.timeController.Subscribe(this);
    }

    public void Invoke()
    {
        // onTimeTick에 연결된 모든 메소드 실행 (null 체크 포함)
        onTimeTick?.Invoke();
    }

    private void OnDestory()
    {
        // 오브젝트가 파괴될 때 구독 해제
        GameManager.instance.timeController.Unsubscribe(this);
    }
}
