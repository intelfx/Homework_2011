#ifndef _FXCWRP_H_
#define _FXCWRP_H_

#include "build.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxcwrp.h
// Author		intelfx
// Description	Template STL containers wrappers.
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Wrappers are used to make storing pointers to objects as easy as it gets.
// Using pointers is faster in case of storing objects which do some
// allocations in ctor / copying ctor / dtor (e. g. non-empty std::vector),
// or which just do not have copying ctor (my classes do not have it too).
// Default:
// testcontainer.push_back (ObjectWithVeryLongCtorAndCopyingCtorAndDtor (.........))
// This creates temporary object (ctor call),
// then container allocates memory for the object (alloc),
// then container stores a copy of the temp object (copying ctor call),
// then a temporary object is deleted (dtor call)
// With wrapper:
// testwrapper.push_back (new ObjectWithVeryLongCtorAndCopyingCtorAndDtor (.........))
// This allocates memory for the new object (alloc),
// then creates object in heap (ctor call),
// then container allocates memory for the pointer (alloc),
// then container receives the pointer (DWORD) and stores it (1-2 asm commands)
// So, overhead is 2 allocs vs 1 alloc but 1 ctor call vs 2 ctor and 1 dtor call.
// Wrapper just replaces the deleting and clearing methods -
// they free memory at pointer, then invoke native methods.
// Also I've implemented some useful methods to "take away" a pointer -
// they will return pointer to program and remove it from the underlying
// container without freeing memory.
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// ---------------------------------Headers-------------------------------------
// -----------------------------------------------------------------------------

// ---------------------------class-pvect-<typename>----------------------------
// --------------------A wrapper to std::vector<typename*>----------------------
// -----------------------------------------------------------------------------

template <typename el_t>
class pvect: public std::vector<el_t*>
{
	pvect (const pvect<el_t>&);
	pvect<el_t>& operator= (const pvect<el_t>&);

public:
	pvect();
	~pvect();

	void	clear();
	void	pop_back();

	el_t*	get_back();

	typedef typename std::vector<el_t*>::iterator iterator;
	typedef typename std::vector<el_t*>::const_iterator const_iterator;
	typedef typename std::vector<el_t*>::reverse_iterator reverse_iterator;
	typedef typename std::vector<el_t*>::const_reverse_iterator const_reverse_iterator;

	iterator erase (const_iterator _Where)
	{
		delete *_Where;
		return std::vector<el_t*>::erase (_Where);
	}

	iterator erase (const_iterator _First_arg, const_iterator _Last_arg)
	{
		for (iterator i = _First_arg; i != _Last_arg; ++i)
			delete *i;

		return std::vector<el_t*>::erase (_First_arg, _Last_arg);
	}
};

// -----------------------------------------------------------------------------
// --------------------------class plist<typename>------------------------------
// ---------------------A wrapper to std::list<typename*>-----------------------
// -----------------------------------------------------------------------------

template <typename el_t>
class plist : public std::list<el_t*>
{
	plist (const plist<el_t>&);
	plist<el_t>& operator= (const plist<el_t>&);

public:
	plist();
	~plist();

	void	clear();
	void	pop_back();
	void	pop_front();

	el_t*	get_back();
	el_t*	get_front();

	typedef typename std::list<el_t*>::iterator iterator;
	typedef typename std::list<el_t*>::const_iterator const_iterator;
	typedef typename std::list<el_t*>::reverse_iterator reverse_iterator;
	typedef typename std::list<el_t*>::const_reverse_iterator const_reverse_iterator;

	iterator erase (iterator _First, iterator _Last)
	{
		for (iterator i = _First; i != _Last; ++i)
			delete *i;

		return std::list<el_t*>::erase (_First, _Last);
	}

	inline iterator erase_nodel (iterator _Where)
	{
		return std::list<el_t*>::erase (_Where);
	}

	inline iterator erase (iterator _Where)
	{
		delete *_Where;

		return std::list<el_t*>::erase (_Where);
	}
};

// -----------------------------------------------------------------------------
// ------------------------------Implementation---------------------------------
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// -------------------------------pvect<el_t>-----------------------------------
// -----------------------------------------------------------------------------

template <typename el_t>
pvect<el_t>::pvect()
{
}

template <typename el_t>
pvect<el_t>::~pvect()
{
	for (iterator i = std::vector<el_t*>::begin();
		 i != std::vector<el_t*>::end();
		 ++i)
		delete *i; // Destroy vector's contents

	// no need in vector's clear(), it'll be destroyed anyway
}

template <typename el_t>
void pvect<el_t>::clear()
{
	for (iterator i = std::vector<el_t*>::begin();
		 i != std::vector<el_t*>::end();
		 ++i)
		delete *i; // Destroy vector's contents

	std::vector<el_t*>::clear(); // Delete vector's elements
}

template <typename el_t>
void pvect<el_t>::pop_back()
{
	delete std::vector<el_t*>::back();
	std::vector<el_t*>::pop_back();
}

template <typename el_t>
el_t* pvect<el_t>::get_back()
{
	el_t* element_pointer = std::vector<el_t*>::back();
	std::vector<el_t*>::pop_back();
	return element_pointer;
}

// -----------------------------------------------------------------------------
// --------------------------class plist<el_t>----------------------------------
// -----------------------------------------------------------------------------

template <typename el_t>
plist<el_t>::plist()
{
}

template <typename el_t>
plist<el_t>::~plist()
{
	for (iterator i = std::list<el_t*>::begin();
		 i != std::list<el_t*>::end();
		 ++i)
		delete *i; // Destroy list's contents

	// no need in list's clear(), it'll be destroyed anyway
}

template <typename el_t>
void plist<el_t>::clear()
{
	for (iterator i = std::list<el_t*>::begin();
		 i != std::list<el_t*>::end();
		 ++i)
		delete *i; // Destroy list's contents

	std::list<el_t*>::clear(); // Delete list's elements
}

template <typename el_t>
void plist<el_t>::pop_back()
{
	delete std::list<el_t*>::back();
	std::list<el_t*>::pop_back();
}

template <typename el_t>
void plist<el_t>::pop_front()
{
	delete std::list<el_t*>::front();

	// FUCK MY BRAIN!!! @ 19.08.2010 17:49:35 DST+0300
	std::list<el_t*>:: /* pop_back */pop_front();
	// That bug has lived for 6 months unfixed
	// and had copy-pasting as its source.
}

template <typename el_t>
el_t* plist<el_t>::get_back()
{
	el_t* element_pointer = std::list<el_t*>::back();
	std::list<el_t*>::pop_back();
	return element_pointer;
}

template <typename el_t>
el_t* plist<el_t>::get_front()
{
	el_t* element_pointer = std::list<el_t*>::front();
	std::list<el_t*>::pop_front();
	return element_pointer;
}

#endif //_FXCWRP_H_
