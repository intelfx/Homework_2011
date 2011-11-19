#ifndef _LIST_H
#define _LIST_H

#include "build.h"
#include "Verifier.h"

template <typename T>
class LinkedList
{
public:
	struct Entity
	{
		T data;
		Entity* next;
	};

	class Iterator
	{
		Entity* entity_;

	public:
		Iterator (Entity* entity) : entity_ (entity) {}
		~Iterator() {}

		T& operator*()
		{
			return entity_ ->data;
		}

		T* operator->()
		{
			return &entity_ ->data;
		}

		Iterator& operator++()
		{

		}
	};
};

#endif // _LIST_H