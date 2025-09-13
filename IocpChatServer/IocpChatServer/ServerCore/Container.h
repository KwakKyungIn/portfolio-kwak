#pragma once
#include "Types.h"
#include "Allocator.h"
#include <array>
#include <vector>
#include <list>
#include <queue>
#include <stack>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
using namespace std;

// --------------------------------------------
// STL 컨테이너에 커스텀 Allocator(StlAllocator) 적용
// - 서버 환경에서 메모리 단편화 줄이기 위함
// - 메모리 풀 기반 할당으로 성능 최적화
// - 기본 STL과 동일한 사용법 유지
// --------------------------------------------

// 배열 (고정 크기)
template<typename Type, uint32 Size>
using Array = array<Type, Size>;

// 벡터 (동적 배열, 풀 할당자 적용)
template<typename Type>
using Vector = vector<Type, StlAllocator<Type>>;

// 리스트 (양방향 연결 리스트)
template<typename Type>
using List = list<Type, StlAllocator<Type>>;

// 맵 (정렬된 key-value 쌍)
template<typename Key, typename Type, typename Pred = less<Key>>
using Map = map<Key, Type, Pred, StlAllocator<pair<const Key, Type>>>;

// 셋 (정렬된 unique key 집합)
template<typename Key, typename Pred = less<Key>>
using Set = set<Key, Pred, StlAllocator<Key>>;

// 덱 (양쪽 입출력 가능 큐)
template<typename Type>
using Deque = deque<Type, StlAllocator<Type>>;

// 큐 (FIFO, 덱 기반)
template<typename Type, typename Container = Deque<Type>>
using Queue = queue<Type, Container>;

// 스택 (LIFO, 덱 기반)
template<typename Type, typename Container = Deque<Type>>
using Stack = stack<Type, Container>;

// 우선순위 큐 (힙 구조)
template<typename Type, typename Container = Vector<Type>, typename Pred = less<typename Container::value_type>>
using PriorityQueue = priority_queue<Type, Container, Pred>;

// 문자열 (풀 할당자 적용)
using String = basic_string<char, char_traits<char>, StlAllocator<char>>;

// 와이드 문자열
using WString = basic_string<wchar_t, char_traits<wchar_t>, StlAllocator<wchar_t>>;

// 해시맵 (unordered_map)
template<typename Key, typename Type, typename Hasher = hash<Key>, typename KeyEq = equal_to<Key>>
using HashMap = unordered_map<Key, Type, Hasher, KeyEq, StlAllocator<pair<const Key, Type>>>;

// 해시셋 (unordered_set)
template<typename Key, typename Hasher = hash<Key>, typename KeyEq = equal_to<Key>>
using HashSet = unordered_set<Key, Hasher, KeyEq, StlAllocator<Key>>;
