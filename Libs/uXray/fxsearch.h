#ifndef _FXSEARCH_H_
#define _FXSEARCH_H_

#include "build.h"

#include "fxassert.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxsearch.h
// Author		intelfx
// Description	Search engine
// -----------------------------------------------------------------------------

DeclareDescriptor(searchEngine);

class FXLIB_API searchEngine : LogBase(searchEngine)
{
	searchEngine (const searchEngine&)				= delete;
	searchEngine& operator= (const searchEngine&)	= delete;

	// Constants
	static const int MAX_DICT = 64;

	// Internal data types
	class FXLIB_API trieNode;

	typedef std::map<char, trieNode*> nodeMap;

	class FXLIB_API trieNode // Main structure
	{
		trieNode (const trieNode&)				= delete;
		trieNode& operator= (const trieNode&)	= delete;

	public:
		bool			isfinal; //Do we have a word here
		nodeMap			nextn; // Next nodes map
		trieNode*		failn;	//Where we go in case of failure
		unsigned short	depth;	//Depth of this level
		unsigned		strnum; // What string it belongs to

		inline trieNode (unsigned short depth_ = 0);
		~trieNode();
	};

	typedef nodeMap::value_type nm_element;
	typedef nodeMap::iterator nm_iterator;

	// Internal data
	trieNode root; // The root node

	// Internal functions
	void addstr_ (const char* string_, trieNode* node_, unsigned strnum_);
	trieNode* srchnode_ (const char* string_, trieNode* node_);
	void buildti_ (const char* string_, trieNode* node_, unsigned len_);

public:
	// ctor / dtor
	searchEngine (const char* const* strings_ = 0, int size_ = 0);
	~searchEngine();

	// Methods
	void load (const char* const* strings_, unsigned size_); // Loads string array
	void search (const char* string_,
				const char*& occur_,
				unsigned& what_,
				unsigned& len_); // Actual search
};

#endif // _FXSEARCH_H_
