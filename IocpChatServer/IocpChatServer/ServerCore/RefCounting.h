#pragma once

// 참조 카운팅 기반 객체 (스마트포인터 흉내)
// - 기본 refCount=1
// - AddRef() / ReleaseRef()로 참조 관리
// - 0 되면 delete this
class RefCountable
{
public:
	RefCountable() : _refCount(1) {}
	virtual ~RefCountable() {}

	// 현재 참조 개수 확인
	int32 GetRefCount() { return _refCount; }

	// 참조 증가
	int32 AddRef() { return ++_refCount; }

	// 참조 감소 (0이면 자동 해제)
	int32 ReleaseRef()
	{
		int32 refCount = --_refCount;
		if (refCount == 0)
		{
			delete this;
		}
		return refCount;
	}

protected:
	atomic<int32> _refCount;
};

// 직접 만든 SharedPtr
// - AddRef/ReleaseRef 자동 처리
// - 포인터 소유권 공유 가능
template<typename T>
class TSharedPtr
{
public:
	TSharedPtr() {}
	TSharedPtr(T* ptr) { Set(ptr); }

	// 복사 생성 (AddRef)
	TSharedPtr(const TSharedPtr& rhs) { Set(rhs._ptr); }

	// 이동 생성 (소유권 이전)
	TSharedPtr(TSharedPtr&& rhs) { _ptr = rhs._ptr; rhs._ptr = nullptr; }

	// 다른 타입 → 다운캐스팅 허용
	template<typename U>
	TSharedPtr(const TSharedPtr<U>& rhs) { Set(static_cast<T*>(rhs._ptr)); }

	~TSharedPtr() { Release(); }

public:
	// 복사 대입
	TSharedPtr& operator=(const TSharedPtr& rhs)
	{
		if (_ptr != rhs._ptr)
		{
			Release();
			Set(rhs._ptr);
		}
		return *this;
	}

	// 이동 대입
	TSharedPtr& operator=(TSharedPtr&& rhs)
	{
		Release();
		_ptr = rhs._ptr;
		rhs._ptr = nullptr;
		return *this;
	}

	// 비교 연산자들
	bool operator==(const TSharedPtr& rhs) const { return _ptr == rhs._ptr; }
	bool operator==(T* ptr) const { return _ptr == ptr; }
	bool operator!=(const TSharedPtr& rhs) const { return _ptr != rhs._ptr; }
	bool operator!=(T* ptr) const { return _ptr != ptr; }
	bool operator<(const TSharedPtr& rhs) const { return _ptr < rhs._ptr; }

	// 포인터 연산자 오버로딩
	T* operator*() { return _ptr; }
	const T* operator*() const { return _ptr; }
	operator T* () const { return _ptr; }
	T* operator->() { return _ptr; }
	const T* operator->() const { return _ptr; }

	// 널 체크
	bool IsNull() { return _ptr == nullptr; }

private:
	// 내부 포인터 설정 (+ AddRef)
	inline void Set(T* ptr)
	{
		_ptr = ptr;
		if (ptr)
			ptr->AddRef();
	}

	// 참조 해제
	inline void Release()
	{
		if (_ptr != nullptr)
		{
			_ptr->ReleaseRef();
			_ptr = nullptr;
		}
	}

private:
	T* _ptr = nullptr; // 실제 객체 포인터
};
