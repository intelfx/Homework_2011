#ifndef _LIST_H
#define _LIST_H

#include "build.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			List.h
// Author		intelfx
// Description	Linked list implementation, as per hometask assignment #3.a.
// -----------------------------------------------------------------------------

DeclareDescriptor (LinkedList);
DeclareDescriptor (LinkedListIterator);

template <typename T>
class LinkedList : LogBase (LinkedList)
{
protected:
	T*		storage_;
	size_t*	next_;

	size_t free_;
	size_t first_;

	size_t count_, capacity_;

	static const size_t end_marker_ = static_cast<size_t> (-1);

	virtual void LLRealloc (size_t new_cap)
	{
		T* new_stor = new T[new_cap];
		__assert (new_stor, "Unable to reallocate main storage");

		for (size_t i = 0; i < capacity_; ++i)
			new_stor[i] = std::move (storage_[i]);

		delete[] storage_; storage_ = new_stor;

		size_t* new_next = reinterpret_cast<size_t*> (realloc (next_, sizeof (size_t) * new_cap));
		__assert (new_next, "Unable to reallocate supplementary 'next' storage");
		next_ = new_next;

		capacity_ = new_cap;
	}

	virtual void LLLink (size_t elem1, size_t elem2)
	{
		if (elem1 == end_marker_)
			first_ = elem2;

		else
			next_[elem1] = elem2;
	}

	virtual void LLSet (size_t elem, T&& data)
	{
		storage_[elem] = std::move (data);
	}

	virtual size_t LLAlloc()
	{
		__assert (free_ != end_marker_, "Free chain is empty");
		size_t allocd = free_;
		free_ = next_[free_];

		++count_;
		return allocd;
	}

	virtual void LLReclaim (size_t element)
	{
		next_[element] = free_;
		free_ = element;

		--count_;
	}

	void LLResetChains (size_t first_new_element, size_t last_new_element)
	{
		// Initialize free chain body
		for (unsigned i = first_new_element; i < last_new_element - 1; ++i)
			next_[i] = i + 1;

		// Link to existing free chain
		next_[last_new_element - 1] = free_;
		free_ = first_new_element;
	}

	virtual void LLInitFields()
	{
		first_	= -1;
		free_	= -1;
		count_	= 0;
	}

	void Reallocate (size_t new_cap)
	{
		verify_method;

		__assert (new_cap, "Cannot reallocate to zero capacity");
		__assert (new_cap > capacity_, "Cannot reallocate to reduce capacity");

		msg (E_INFO, E_DEBUG, "Adding capacity %zu -> %zu", capacity_, new_cap);

		size_t old_cap = capacity_;
		LLRealloc (new_cap);
		LLResetChains (old_cap, new_cap);

		msg (E_INFO, E_DEBUG, "Reallocation completed");

		verify_method;
	}

protected:
	virtual bool _Verify() const
	{
		size_t tmp_count, index;

		verify_statement (count_ <= capacity_, "Count is greater than capacity");

		if (!capacity_)
		{
			verify_statement (!storage_, "Invalid uninitialized state: main storage is %p", storage_);
			verify_statement (!next_, "Invalid uninitialized state: next storage is %p", next_);

			verify_statement (!count_, "Invalid uninitialized state: count is %zu", count_);
			verify_statement (free_ == end_marker_, "Invalid uninitialized state: free chain head is %zd", free_);
			verify_statement (first_ == end_marker_, "Invalid uninitialized state: main chain head is %zd", first_);

			return 1; // not initialized
		}

		verify_statement (storage_, "NULL main storage");
		verify_statement (next_, "NULL next storage");

		/* bounds check */
		for (tmp_count = 0, index = 0; index < capacity_; ++index)
			verify_statement (next_[index] == end_marker_ ||
							  next_[index] < capacity_,
							  "NEXT [%zd] -> %zd points out of bounds (cap %zu)", index, next_[index], capacity_);

		/* allocated chain verification */
		for (index = first_; index != end_marker_; index = next_[index])
			++tmp_count;

		verify_statement (tmp_count == count_, "Chain inconsistency: allocated chain length %zu (accounted %zu)",
						  tmp_count, count_);

		/* free chain verification */
		for (tmp_count = 0, index = free_; index != end_marker_; index = next_[index])
			++tmp_count;

		verify_statement (tmp_count == capacity_ - count_, "Chain inconsistency: free chain length %zu (accounted %zu)",
						  tmp_count, capacity_ - count_);

		return 1;
	}

public:
public:
	class Iterator : LogBase (LinkedListIterator)
	{
	protected:
		LinkedList* parent_;
		size_t prev_index_;

		size_t GetNext_() const
		{
			verify_method;

			return (prev_index_ == end_marker_) ? parent_ ->first_
												: parent_ ->next_[prev_index_];
		}

	protected:
		virtual bool _Verify() const
		{
			verify_statement (parent_, "Orphan iterator");
			verify_statement (parent_ ->CheckObject(), "Owner check failed");

			if (prev_index_ != end_marker_)
				verify_statement (prev_index_ < parent_ ->capacity_, "Iterator points beyond the container actual capacity");

			return 1;
		}

		size_t Index() const
		{
			verify_method;

			size_t idx = GetNext_();
			__assert (idx != end_marker_, "Iterator points to end");
			__assert (idx < parent_ ->capacity_, "Iterator points beyond the container actual capacity");

			return idx;
		}

		size_t Prev()
		{
			verify_method;
			return prev_index_;
		}

	public:
		Iterator (const Iterator&) = default;
		Iterator& operator= (const Iterator&) = default;

		Iterator (LinkedList* parent, size_t prev) :
		parent_ (parent),
		prev_index_ (prev)
		{
		}

		Iterator& operator++()
		{
			prev_index_ = Index();
			return *this;
		}

		bool End() const
		{
			verify_method;

			return GetNext_() == end_marker_;
		}

		bool operator== (const Iterator& that) const
		{
			verify_method;
			that.Check();

			__assert (parent_ == that.parent_, "Comparison of iterators of different containers");
			return prev_index_ == that.prev_index_;
		}

		bool operator!= (const Iterator& that) const
		{
			verify_method;
			that.Check();

			__assert (parent_ == that.parent_, "Comparison of iterators of different containers");
			return prev_index_ != that.prev_index_;
		}

		bool operator< (const Iterator& that) const
		{
			verify_method;
			that.Check();

			__assert (parent_ == that.parent_, "Comparison of iterators of different containers");
			return prev_index_ < that.prev_index_;
		}

		bool operator> (const Iterator& that) const
		{
			verify_method;
			that.Check();

			__assert (parent_ == that.parent_, "Comparison of iterators of different containers");
			return prev_index_ > that.prev_index_;
		}

		T& operator*()
		{
			verify_method;

			size_t idx = Index();
			return parent_ ->storage_[idx];
		}

		const T& operator*() const
		{
			verify_method;

			size_t idx = Index();
			return parent_ ->storage_[idx];
		}

		T* operator->()
		{
			return &operator*();
		}

		const T* operator->() const
		{
			return &operator*();
		}
	}; // class Iterator

	LinkedList (size_t initial_capacity = 10) :
	storage_ (0),
	next_ (0),
	free_ (-1),
	first_ (-1),
	count_ (0),
	capacity_ (0)
	{
		Reallocate (initial_capacity);
	}

	virtual ~LinkedList()
	{
		delete[] storage_;
		free (next_);
	}

	size_t Count() const
	{
		verify_method;

		return count_;
	}

	void Clear()
	{
		verify_method;
		LLInitFields();
		LLResetChains (0, capacity_);
		verify_method;
	}

	Iterator PushFront (T&& object)
	{
		verify_method;

		msg (E_INFO, E_DEBUG, "Front insertion: count %zu capacity %zu", count_, capacity_);

		while (count_ >= capacity_)
			Reallocate (capacity_ * 2);

		size_t allocated = LLAlloc();

		LLSet (allocated, std::move (object));
		LLLink (allocated, first_);
		LLLink (end_marker_, allocated);

		verify_method;
		msg (E_INFO, E_DEBUG, "Front insertion completed");

		return Iterator (this, end_marker_);
	}

	T PopFront()
	{
		verify_method;
		__verify (count_, "Cannot remove from empty list");

		msg (E_INFO, E_DEBUG, "Front removal: count %zu capacity %zu", count_, capacity_);

		size_t removed = first_;

		LLLink (end_marker_, next_[removed]);
		LLReclaim (removed);

		verify_method;
		msg (E_INFO, E_DEBUG, "Front removal completed");

		return std::move (storage_[removed]);
	}

	Iterator Insert (Iterator where, T&& object)
	{
		verify_method;

		msg (E_INFO, E_DEBUG, "Iterator insertion: count %zu capacity %zu", count_, capacity_);

		while (count_ >= capacity_)
			Reallocate (capacity_ * 2);

		size_t allocated = LLAlloc();
		size_t place = where.Index();

		LLLink (allocated, next_[place]);
		LLLink (place, allocated);

		verify_method;
		msg (E_INFO, E_DEBUG, "Insertion %zu -> [%zu] -> %zu completed",
			 place, allocated, next_[allocated]);

		return Iterator (this, place);
	}

	Iterator Erase (Iterator where)
	{
		verify_method;

		size_t index = where.Index();
		size_t prev = where.Prev();

		msg (E_INFO, E_DEBUG, "Iterator removal: count %zu capacity %zu", count_, capacity_);

		__assert (next_[prev] == index, "Chain malformed");

		LLLink (prev, next_[index]);
		LLReclaim (index);

		verify_method;
		msg (E_INFO, E_DEBUG, "Removal %zu ( --> [%zu] ) --> %zu completed",
			 prev, index, next_[prev]);

		return Iterator (this, prev);
	}

	Iterator Begin()
	{
		verify_method;

		return Iterator (this, end_marker_);
	}
};

#endif // _LIST_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
