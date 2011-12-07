#ifndef _QUEUE_H
#define _QUEUE_H

#include "build.h"
#include "Verifier.h"

// -----------------------------------------------------------------------------
// Library		Antided
// File			Queue.h
// Author		intelfx
// Description	Queue, as per hometask assignment #2.
// -----------------------------------------------------------------------------

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
		verify_statement (count_ < storage_.Capacity(), "Queue overflow: [cap %zu] %zu:%zu (%zu)",
						  storage_.Capacity(), tail_, head_, count_);

		verify_statement (head_ < storage_.Capacity(), "HEAD pointer overflow: [cap %zu] %zu:%zu (%zu)",
						  storage_.Capacity(), tail_, head_, count_);

		verify_statement (tail_ < storage_.Capacity(), "TAIL pointer overflow: [cap %zu] %zu:%zu (%zu)",
						  storage_.Capacity(), tail_, head_, count_);

		verify_statement (((head_ - tail_ + storage_.Capacity()) % (storage_.Capacity())) == count_,
						  "Internal storage inconsistency : [cap %zu] %zu:%zu != %zu",
						  storage_.Capacity(), tail_, head_, count_);

		return 1;
	}

public:
	Queue() :
	head_ (0),
	tail_ (0),
	count_ (0),
	storage_()
	{
	}

	Queue (Queue&& that) : move_ctor,
	head_ (that.head_),
	tail_ (that.tail_),
	count_ (that.count_),
	storage_ (std::move (that.storage_))
	{
		that.count_ = 0;
		that.head_ = 0;
		that.tail_ = 0;
	}

	Queue& operator= (Queue&& that)
	{
		if (this == &that)
			return *this;

		move_op;

		head_ = that.head_;
		tail_ = that.tail_;
		count_ = that.count_;

		storage_ = std::move (that.storage_);

		that.head_ = 0;
		that.tail_ = 0;
		that.count_ = 0;

		return *this;
	}

	bool Empty()
	{
		verify_method;
		return !count_;
	}

	bool Full()
	{
		verify_method;
		return count_ == (storage_.Capacity() - 1);
	}

	void Add (T&& object)
	{
		verify_method;

		msg (E_INFO, E_DEBUG, "Queue [cap %lu] %lu:%lu (%lu) : object add",
			 storage_.Capacity(), tail_, head_, count_);

		if (count_)
			__assert ((tail_ != head_) && (count_ < storage_.Capacity()), "Queue overflow");

		storage_.Access (head_++) = std::move (object);
		++count_;

		if (head_ >= storage_.Capacity())
		{
			head_ %= storage_.Capacity();
			msg (E_INFO, E_DEBUG, "Queue wraparound, HEAD now %lu", head_);
		}


		verify_method;
	}

	T Remove()
	{
		verify_method;

		msg (E_INFO, E_DEBUG, "Queue [cap %lu] %lu:%lu (%lu) : object remove",
			 storage_.Capacity(), tail_, head_, count_);

		__assert (count_ && (tail_ != head_), "Queue underflow");

		T& obj_to_remove = storage_.Access (tail_++);
		--count_;

		if (tail_ >= storage_.Capacity())
		{
			tail_ %= storage_.Capacity();
			msg (E_INFO, E_DEBUG, "Queue wraparound, TAIL now %lu", head_);
		}


		verify_method;

		return std::move (obj_to_remove);
	}

	T& Begin()
	{
		verify_method;

		msg (E_INFO, E_DEBUG, "Queue [cal %lu] %lu:%lu (%lu) : accessing head",
			 storage_.Capacity(), tail_, head_, count_);

		__assert (count_, "Queue underflow");
		return storage_.Access (head_);
	}

	T& End()
	{
		verify_method;

		msg (E_INFO, E_DEBUG, "Queue [cal %lu] %lu:%lu (%lu) : accessing tail",
			 storage_.Capacity(), tail_, head_, count_);

		__assert (count_, "Queue underflow");
		return storage_.Access (tail_);
	}
};

#endif // _QUEUE_H