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
		if (count_ >= storage_.Capacity())
			return 0;
	}
};

#endif // _QUEUE_H