#pragma once

// 단순 malloc/free 기반 기본 할당자
class BaseAllocator
{
public:
	// 메모리 할당 (malloc)
	static void* Alloc(int32 size);

	// 메모리 해제 (free)
	static void Release(void* ptr);
};

// 페이지 단위 할당으로 오버런 탐지용
class StompAllocator
{
	enum { PAGE_SIZE = 0x1000 }; // 한 페이지 크기 (4KB)

public:
	// VirtualAlloc 사용, 디버깅용
	static void* Alloc(int32 size);

	// VirtualFree 사용, 전체 페이지 반환
	static void Release(void* ptr);
};

// 메모리 풀 기반, 재사용으로 성능 향상
class PoolAllocator
{
public:
	// 메모리 풀에서 꺼내옴
	static void* Alloc(int32 size);

	// 메모리를 다시 풀에 반납
	static void Release(void* ptr);
};

// STL 컨테이너 전용 커스텀 allocator
template<typename T>
class StlAllocator
{
public:
	using value_type = T;

	StlAllocator() {}
	template<typename Other>
	StlAllocator(const StlAllocator<Other>&) {}

	// 요소 개수 × sizeof(T) 만큼 메모리 확보
	T* allocate(size_t count)
	{
		const int32 size = static_cast<int32>(count * sizeof(T));
		return static_cast<T*>(PoolAllocator::Alloc(size));
	}

	// 확보한 메모리 해제
	void deallocate(T* ptr, size_t count)
	{
		PoolAllocator::Release(ptr);
	}
};
