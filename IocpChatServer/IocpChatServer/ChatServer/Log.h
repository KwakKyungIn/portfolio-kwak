#pragma once                               // 헤더 중복 포함 방지

#include <cstdio>                          // printf / snprintf
#include <cstdarg>                         // 가변 인자 처리(va_list 등)
#include <chrono>                          // 시간(시:분:초 + ms) 출력용
#include <thread>                          // 스레드 ID 얻기
#include <string>                          // 문자열 해시 등에 필요

#ifdef _WIN32
#include <Windows.h>                       // 윈도우 API 사용
// 현재 스레드 ID를 얻는 함수 (Win32 전용)
inline uint32_t _tid() { return ::GetCurrentThreadId(); }
#else
// 리눅스/기타 환경: std::thread::id를 hash로 uint32 변환
inline uint32_t _tid() {
    auto id = std::this_thread::get_id();
    return (uint32_t)std::hash<std::thread::id>{}(id);
}
#endif

// Session, Player, Room 전방 선언 (충돌 방지 목적)
class Session; class Player; class Room;

// 현재 시각을 "HH:MM:SS.mmm" 문자열로 변환
inline void _now_hms(char out[32]) {
    using namespace std::chrono;
    const auto now = system_clock::now();                  // 현재 시각
    const auto tt = system_clock::to_time_t(now);          // time_t 변환 (초 단위)
    tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &tt);                                // Win32 thread-safe 변환
#else
    localtime_r(&tt, &tmv);                                // POSIX thread-safe 변환
#endif
    const auto ms = duration_cast<milliseconds>(           // 현재 시각의 밀리초만 추출
        now.time_since_epoch()) % 1000;
    std::snprintf(out, 32, "%02d:%02d:%02d.%03d",          // "HH:MM:SS.mmm" 포맷
        tmv.tm_hour, tmv.tm_min, tmv.tm_sec, (int)ms.count());
}

// 세션/플레이어/룸 컨텍스트까지 포함된 로깅 함수
inline void LOG_SNAPSHOT_CTX(const Session* sess,
    uint64_t playerId,
    int roomId,
    const char* fmt, ...)
{
    char ts[32]; _now_hms(ts);                             // 시각 문자열
    uintptr_t s = reinterpret_cast<uintptr_t>(sess);       // 세션 포인터 주소값 (컨텍스트 구분용)

    char msg[1024];                                        // 최종 메시지 버퍼
    va_list ap; va_start(ap, fmt);                         // 가변 인자 처리 시작
    std::vsnprintf(msg, sizeof(msg), fmt, ap);             // 포맷 문자열 조합
    va_end(ap);                                            // 가변 인자 해제

    // 공통 prefix: 시간 + 스레드ID + 세션 주소
    char prefix[128]; prefix[0] = '\0';
    if (sess) std::snprintf(prefix, sizeof(prefix), "[%s][thr:%u][sess:%p]\n", ts, _tid(), (void*)s);
    else      std::snprintf(prefix, sizeof(prefix), "[%s][thr:%u]\n", ts, _tid());

    // 플레이어ID, 룸ID 여부에 따라 다른 포맷으로 출력
    if (playerId && roomId)
        std::printf("%s[player:%llu][room:%d] %s\n", prefix, (unsigned long long)playerId, roomId, msg);
    else if (playerId)
        std::printf("%s[player:%llu] %s\n", prefix, (unsigned long long)playerId, msg);
    else if (roomId)
        std::printf("%s[room:%d] %s\n", prefix, roomId, msg);
    else
        std::printf("%s %s\n", prefix, msg);
}

// 컨텍스트 없이 단순 로그만 찍는 함수
inline void LOG_SNAPSHOT(const char* fmt, ...) {
    char ts[32]; _now_hms(ts);                             // 시각 문자열
    char msg[1024];
    va_list ap; va_start(ap, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, ap);             // 메시지 조합
    va_end(ap);
    std::printf("[%s] %s\n", ts, msg);                     // "HH:MM:SS.mmm msg"
}

// 필요하면 전역 토글 스위치 추가 가능
// (예: 특정 빌드에서 로그 꺼버리기)
