using System;  
using System.Collections;  
using System.Collections.Generic;  
using UnityEngine;  
using UnityEngine.Scripting.APIUpdating;  

[RequireComponent(typeof(Rigidbody2D))] // Rigidbody2D가 꼭 필요하다고 명시
public class CharacterController2D : MonoBehaviour
{
    Rigidbody2D rigidbody2d;             // 물리 이동을 담당할 Rigidbody2D
    [SerializeField] float speed = 2f;   // 이동 속도 (Inspector에서 조정 가능)
    Vector2 motionVector;                // 현재 입력 방향
    public Vector2 lastMotionVector;     // 마지막 이동 방향 (정지 상태에서 방향 유지)
    Animator animator;                   // 애니메이션 제어용
    public bool moving;                  // 현재 이동 중인지 여부

    void Awake()
    {
        rigidbody2d = GetComponent<Rigidbody2D>(); // Rigidbody2D 컴포넌트 가져오기
        animator = GetComponent<Animator>();       // Animator 컴포넌트 가져오기
    }

    private void Update()
    {
        float horizontal = Input.GetAxisRaw("Horizontal"); // 좌우 입력(-1,0,1)
        float vertical = Input.GetAxisRaw("Vertical");     // 상하 입력(-1,0,1)

        // 입력값을 motionVector에 저장
        motionVector = new Vector2(horizontal, vertical);

        // 애니메이터에 현재 입력 방향 전달
        animator.SetFloat("horizontal", horizontal);
        animator.SetFloat("vertical", vertical);

        // 이동 여부 판정 (하나라도 0이 아니면 true)
        moving = horizontal != 0 || vertical != 0;
        animator.SetBool("moving", moving);

        // 입력이 있으면 마지막 이동 방향 갱신
        if (horizontal != 0 || vertical != 0)
        {
            lastMotionVector = new Vector2(horizontal, vertical).normalized; // 정규화된 벡터

            // 애니메이터에 마지막 방향 저장
            animator.SetFloat("lastHorizontal", horizontal);
            animator.SetFloat("lastVertical", vertical);
        }
    }

    void FixedUpdate()
    {
        Move(); // 물리 이동은 FixedUpdate에서 실행
    }

    private void Move()
    {
        rigidbody2d.velocity = motionVector * speed; // 속도를 입력 방향 * 속도로 설정
    }
}
