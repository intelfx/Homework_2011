#ifndef _QUEUE_H
#define _QUEUE_H

#include "build.h"
#include "Verifier.h"

DeclareDescriptor(Queue);

static const size_t QUEUE_SIZE = 100;

template <typename T>
class Queue : LogBase(Queue)
{
	size_t head_, tail_, count_;
	MallocAllocator<T> storage_;

protected:
	virtual bool Verify_() const
	{
		verify_statement (count_ < storage_.Capacity(), "Queue overflow");

		verify_statement (head_ < storage_.Capacity(), "HEAD pointer overflow");
		verify_statement (tail_ < storage_.Capacity(), "TAIL pointer overflow");

		verify_statement (((head_ - tail_) % (storage_.Capacity())) == count_,
						  "Internal storage inconsistency");

		return 1;
	}

public:
	void Add (T&& object)
	{
		verify_method;

		msg (E_INFO, E_DEBUGLIB, "Queue [cap %lu] %lu:%lu (%lu) : object add",
			 storage_.Capacity(), tail_, head_, count_);

		__assert (!count_ || (tail_ != head_), "Queue overflow");
		storage_.Access (head_++) = std::move (object);

		if (head_ >= storage_.Capacity())
		{
			head_ %= storage_.Capacity();
			msg (E_INFO, E_DEBUGLIB, "Queue wraparound, HEAD now %lu", head_);
		}

		verify_method;
	}

	T Remove()
	{
		verify_method;

		msg (E_INFO, E_DEBUGLIB, "Queue [cap %lu] %lu:%lu (%lu) : object remove",
			 storage_.Capacity(), tail_, head_, count_);

		__assert (count_, "Queue underflow");

		T& obj_to_remove = storage_.Access (tail_++);

		if (tail_ >= storage_.Capacity())
		{
			tail_ %= storage_.Capacity();
			msg (E_INFO, E_DEBUGLIB, "Queue wraparound, TAIL now %lu", head_);
		}

		verify_method;

		return std::move (obj_to_remove);
	}

	T& Begin()
	{
		verify_method;

		msg (E_INFO, E_DEBUGLIB, "Queue [cal %lu] %lu:%lu (%lu) : accessing head",
			 storage_.Capacity(), tail_, head_, count_);

		__assert (count_, "Queue underflow");
		return storage_.Access (head_);
	}

	T& End()
	{
		verify_method;

		msg (E_INFO, E_DEBUGLIB, "Queue [cal %lu] %lu:%lu (%lu) : accessing tail",
			 storage_.Capacity(), tail_, head_, count_);

		__assert (count_, "Queue underflow");
		return storage_.Access (tail_);
	}
};

#endif // _QUEUE_H