#ifndef _DLLIST_H
#define _DLLIST_H

#include "build.h"

#include "LinkedList.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			DoubleLinkedList.h
// Author		intelfx
// Description	Double linked list implementation, as per hometask assignment #3.b.
// -----------------------------------------------------------------------------

DeclareDescriptor(DoubleLinkedList);
DeclareDescriptor(DoubleLinkedListIterator);

template <typename T>
class DoubleLinkedList : LogBase(DoubleLinkedList), public LinkedList<T>
{
	size_t last_;
	size_t* prev_;

	void ReallocPrev_ (size_t new_cap)
	{
		size_t* new_prev = reinterpret_cast<size_t*> (realloc (prev_, sizeof (size_t) * new_cap));
		__assert (new_prev, "Unable to reallocate supplementary 'prev' storage");
		prev_ = new_prev;
	}

	virtual void LLRealloc (size_t new_cap)
	{
		LinkedList<T>::LLRealloc (new_cap);
		ReallocPrev_ (new_cap);
	}

	virtual void LLLink (size_t elem1, size_t elem2)
	{
		LinkedList<T>::LLLink (elem1, elem2);

		if (elem2 == LinkedList<T>::end_marker_)
			last_ = elem1;

		else
			prev_[elem2] = elem1;
	}

	virtual void LLInitFields()
	{
		LinkedList<T>::LLInitFields();
		last_ = -1;
	}

protected:
	virtual bool _Verify() const
	{
        /*
         * Base class verification
		 */

		if (!LinkedList<T>::_Verify())
			return 0;

		/*
		 * Incremental verification
		 */

		if (!LinkedList<T>::capacity_)
		{
			verify_statement (!prev_, "Invalid uninitialized state: prev storage is %p", prev_);
			verify_statement (last_ == LinkedList<T>::end_marker_,
							  "Invalid uninitialized state: main chain tail is %zd", last_);
		}

		verify_statement (prev_, "NULL prev storage");

		/* prev consistency check */
		if (LinkedList<T>::count_)
			for (size_t idx = LinkedList<T>::end_marker_;;)
			{
				size_t next = (idx  == LinkedList<T>::end_marker_) ? LinkedList<T>::first_
																   : LinkedList<T>::next_[idx];
				size_t prev = (next == LinkedList<T>::end_marker_) ? last_
																   : prev_[next];

				verify_statement (idx == prev,
								"%zd -> [%zd] <- %zd next-prev inconsistency", idx, next, prev);

				if (next == LinkedList<T>::end_marker_)
					break;

				idx = next;
			}

		return 1;
	}

public:
	class Iterator : public LinkedList<T>::Iterator
	{
		DoubleLinkedList* parent_;

	public:
		Iterator (DoubleLinkedList* parent, size_t prev) :
		LinkedList<T>::Iterator (parent, prev),
		parent_ (parent)
		{
		}

		Iterator& operator-- ()
		{
			verify_method;

			__assert (LinkedList<T>::Iterator::prev_index_ != LinkedList<T>::end_marker_,
					  "Decrementing iterator pointing at begin");
			LinkedList<T>::Iterator::prev_index_ = parent_ ->prev_ [LinkedList<T>::Iterator::prev_index_];
			return *this;
		}
	};

	DoubleLinkedList (size_t initial_capacity = 10) :
	LinkedList<T> (initial_capacity),
	last_ (-1),
	prev_ (0)
	{
		ReallocPrev_ (initial_capacity);
	}

	Iterator PushBack (T&& object)
	{
		verify_method;

		msg (E_INFO, E_DEBUG, "Back insertion: count %zu capacity %zu",
			 LinkedList<T>::count_, LinkedList<T>::capacity_);

		while (LinkedList<T>::count_ >= LinkedList<T>::capacity_)
			LinkedList<T>::Reallocate (LinkedList<T>::capacity_ * 2);

		size_t allocated = LinkedList<T>::LLAlloc();
		size_t old_last = last_;

		LinkedList<T>::LLSet (allocated, std::move (object));
		LLLink (last_, allocated);
		LLLink (allocated, LinkedList<T>::end_marker_);

		verify_method;
		msg (E_INFO, E_DEBUG, "Back insertion completed");

		return Iterator (this, old_last);
	}

	T PopBack()
	{
		verify_method;
		__verify (LinkedList<T>::count_, "Cannot remove from empty list");

		msg (E_INFO, E_DEBUG, "Back removal: count %zu capacity %zu",
			 LinkedList<T>::count_, LinkedList<T>::capacity_);

		size_t removed = last_;

		LLLink (prev_[removed], LinkedList<T>::end_marker_);
		LinkedList<T>::LLReclaim (removed);

		verify_method;
		msg (E_INFO, E_DEBUG, "Back removal completed");

		return std::move (LinkedList<T>::storage_[removed]);
	}

	Iterator End()
	{
		verify_method;

		return Iterator (this, last_);
	}
};

#endif // _DLLIST_H