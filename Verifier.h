#ifndef _VERIFIER_H
#define _VERIFIER_H

#include "build.h"

// -----------------------------------------------------------------------------
// Library		Antided
// File			Verifier.h
// Author		intelfx
// Description	Verifier and allocator classes for containers.
// -----------------------------------------------------------------------------

DeclareDescriptor(AllocBase);
DeclareDescriptor(StaticAllocator);
DeclareDescriptor(MallocAllocator);

template <typename T>
class AllocBase : LogBase(AllocBase)
{
protected:
	virtual T& _Access (size_t subscript) = 0;
	virtual const T& _Access (size_t subscript) const = 0;

	virtual bool Verify_() const;
	virtual void _Realloc (size_t capacity) = 0;

public:
	AllocBase() = default;
	virtual ~AllocBase();

	virtual size_t Capacity() const = 0;

	void Realloc (size_t capacity);

	T& Access (size_t subscript);
	const T& Access (size_t subscript) const;
};

template <typename T>
AllocBase<T>::~AllocBase() = default;


template <typename T>
void AllocBase<T>::Realloc (size_t capacity)
{
	__assert (capacity, "New capacity is zero");
	if (capacity == Capacity())
	{
		msg (E_WARNING, E_VERBOSE, "Reallocating to the same size");
		return;
	}

	_Realloc (capacity);
}

template <typename T>
T& AllocBase<T>::Access (size_t subscript)
{
	msg (E_INFO, E_DEBUGLIB, "Accessing [%d] for RW", subscript);
	__assert (subscript < Capacity(), "Accessing beyond the end");

	return _Access (subscript);
}

template <typename T>
const T& AllocBase<T>::Access (size_t subscript) const
{
	msg (E_INFO, E_DEBUGLIB, "Accessing [%d] for R", subscript);
	__assert (subscript < Capacity(), "Accessing beyond the end");

	return _Access (subscript);
}

template <typename T>
bool AllocBase<T>::Verify_() const
{
	if (!Capacity())
		return 0;

	return 1;
}

template <typename T>
class MallocAllocator : LogBase(MallocAllocator), public AllocBase<T>
{
	static const size_t initial_cap = 10;
	size_t capacity_;
	T* array_;

protected:
	virtual bool Verify_() const
	{
		if (!capacity_)
			return 0;

		if (!array_)
			return 0;

		return AllocBase<T>::Verify_();
	}

	virtual void _Realloc (size_t cap)
	{
		msg (E_INFO, E_VERBOSELIB, "Reallocating dynamic allocator to %d", cap);

		T* old_array = array_;
		array_ = reinterpret_cast<T*> (realloc (old_array, cap));

		if (!array_) free (old_array);
		__assert (array_, "Failed to reallocate");

		capacity_ = cap;
	}

	virtual T& _Access (size_t subscript)
	{
		return array_[subscript];
	}

	virtual const T& _Access (size_t subscript) const
	{
		return array_[subscript];
	}

public:
	MallocAllocator() :
	capacity_ (initial_cap),
	array_ (reinterpret_cast<T*> (malloc (sizeof (T) * capacity_)))
	{
	}

	virtual size_t Capacity() const
	{
		return capacity_;
	}

	T& operator[] (size_t subscript)
	{
		return AllocBase<T>::Access (subscript);
	}

	const T& operator[] (size_t subscript) const
	{
		return AllocBase<T>::Access (subscript);
	}
};

template <typename T, size_t capacity>
class StaticAllocator : LogBase(StaticAllocator), public AllocBase<T>
{
	T array_ [capacity];

protected:
	virtual bool Verify_() const
	{
		return AllocBase<T>::Verify_(); // Nothing to verify
	}

	virtual void _Realloc (size_t)
	{
		msg (E_CRITICAL, E_VERBOSE, "Reallocating static allocator is not supported");
	}

	virtual T& _Access (size_t subscript)
	{
		return array_[subscript];
	}

	virtual const T& _Access (size_t subscript) const
	{
		return array_[subscript];
	}

public:
	StaticAllocator()
	{
		static_assert (capacity, "Static allocator with zero capacity");
	}

	virtual size_t Capacity() const
	{
		return capacity;
	}

	T& operator[] (size_t subscript)
	{
		return AllocBase<T>::Access (subscript);
	}

	const T& operator[] (size_t subscript) const
	{
		return AllocBase<T>::Access (subscript);
	}
};


#endif // _VERIFIER_H