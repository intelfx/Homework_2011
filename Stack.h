#ifndef _STACK_H
#define _STACK_H

#include "build.h"
#include "Verifier.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			Stack.h
// Author		intelfx
// Description	Stack, as per hometask assignment #1.a.
// -----------------------------------------------------------------------------

static const size_t STACK_SIZE = 100;

DeclareDescriptor(Stack);

template <typename T>
class Stack : LogBase(Stack), protected MallocAllocator<T>
{
	size_t stack_top;

protected:
	virtual bool Verify_() const
	{
		if (stack_top > MallocAllocator<T>::Capacity())
			return 0;

		return 1;
	}

public:
	Stack() :
	stack_top (0)
	{
	}

	virtual ~Stack();

	bool Empty()
	{
		return (stack_top == 0);
	}

	void Push (const T& object)
	{
		msg (E_INFO, E_DEBUG, "Pushing object on [%ld] (capacity %ld)", stack_top, MallocAllocator<T>::Capacity());

		if (stack_top == MallocAllocator<T>::Capacity())
		{
			msg (E_INFO, E_DEBUG, "Trying to reallocate");
			MallocAllocator<T>::Realloc (MallocAllocator<T>::Capacity() * 2);
			msg (E_INFO, E_DEBUG, "New capacity is %ld", MallocAllocator<T>::Capacity());
		}

		MallocAllocator<T>::Access (stack_top++) = std::move (object);
	}

	T Pop()
	{
		__assert (stack_top > 0, "Stack underrun trying to pop an object");
		msg (E_INFO, E_DEBUG, "Popping object from [%ld] (capacity %ld)", --stack_top, MallocAllocator<T>::Capacity());

		return std::move (MallocAllocator<T>::Access (stack_top));
	}

	T& Top()
	{
		__assert (stack_top > 0, "Stack underrun trying to access stack top");
		msg (E_INFO, E_DEBUG, "Accessing stack top on [%ld] (R/W)", stack_top - 1);

		return MallocAllocator<T>::Access (stack_top - 1);
	}

	const T& Top() const
	{
		__assert (stack_top > 0, "Stack underrun trying to access stack top at %d", stack_top - 1);
		msg (E_INFO, E_DEBUG, "Accessing stack top on [%ld] (R/O)", stack_top - 1);

		return MallocAllocator<T>::Access (stack_top - 1);
	}

	T& Introspect (size_t offset)
	{
		__assert (stack_top - offset > 0, "Stack underrun trying to introspect stack at %ld",
				  stack_top - offset - 1);
		msg (E_INFO, E_DEBUG, "Introspecting stack at [%ld] (R/W)", stack_top - offset - 1);

		return MallocAllocator<T>::Access (stack_top - offset - 1);
	}

	const T& Introspect (size_t offset) const
	{
		__assert (stack_top - offset > 0, "Stack underrun trying to introspect stack at %ld",
				  stack_top - offset - 1);
		msg (E_INFO, E_DEBUG, "Introspecting stack at [%ld] (R/O)", stack_top - offset - 1);

		return MallocAllocator<T>::Access (stack_top - offset - 1);
	}

	T& Absolute (size_t offset)
	{
		__assert (offset <= stack_top, "Absolute offset too big at %ld", offset);
		msg (E_INFO, E_DEBUG, "Absolute accessing stack at [%ld] (R/W)", offset);

		return MallocAllocator<T>::Access (offset);
	}

	const T& Absolute (size_t offset) const
	{
		__assert (offset <= stack_top, "Absolute offset too big at %ld", offset);
		msg (E_INFO, E_DEBUG, "Absolute accessing stack at [%ld] (R/O)", offset);

		return MallocAllocator<T>::Access (offset);
	}

	size_t CurrentTopIndex() const
	{
		return stack_top - 1;
	}

	void Reset()
	{
		msg (E_INFO, E_DEBUG, "Resetting stack");
		stack_top = 0;
	}
};

template <typename T>
Stack<T>::~Stack() = default;

#endif // _STACK_H