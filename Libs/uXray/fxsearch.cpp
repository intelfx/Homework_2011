#include "stdafx.h"
#include "fxsearch.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxsearch.cpp
// Author		intelfx
// Description	Search engine
// -----------------------------------------------------------------------------

void searchEngine::addstr_ (const char* string_,
							trieNode* node_,
							unsigned strnum_) // Add a specified string
{
	//Sanity check
	if (!node_ || !*string_)
		return;

	//The char we are looking for
	char char_srch = string_[0];

	//Our node
	trieNode* node_new;

	//Look for the next node
	nm_iterator next_it = node_ ->nextn.find (char_srch);

	//Do we have it?
	if (next_it == node_->nextn.end())
	{
		//Need to build this node
		node_new = new trieNode (node_ ->depth + 1);

		//Add it
		node_ ->nextn.insert (nm_element (char_srch, node_new));
	}
	else
		//Take the node
		node_new = next_it ->second;

	//Is it the last char?
	if (!string_[1]) // if (strlen (string_) == 1) // Non-optimized
		//Set as last and set the string #
	{
		node_new ->isfinal = 1;
		node_new ->strnum = strnum_;
	}

	else
		//Run next layer
		addstr_ (string_ + 1, node_new, strnum_);
}

searchEngine::trieNode* searchEngine::srchnode_ (const char* string_, trieNode* node_)
{
	//Sanity check
	if (!node_ || ! (*string_))
		return 0;

	//The char we are looking for
	char char_srch;
	char_srch = string_[0];

	//Look for the next node
	nm_iterator next_it = node_ ->nextn.find (char_srch);

	//Do we have it?
	if (next_it != node_ ->nextn.end())
	{
		//Is it the last char?
		if (!string_[1]) //if (rString.length()==1) // Non-optimized
			//We found our string
			return next_it ->second;

		else
			//Search again
			return srchnode_ (string_ + 1, next_it ->second);
	}

	else
		//Not found
		return 0;
}

void searchEngine::buildti_ (const char* string_, trieNode* node_, unsigned len_)
{
	//Sanity
	if (!node_)
		return;

	//Do we need to process this node?
	if (node_ ->depth > 1)
	{
		//Clear the node first
		node_ ->failn = 0;

		//We need to start and look for suffix/prefix
		for (unsigned i = 1; i < len_; ++i)
		{
			//Build the sub string
			const char* sub_ = string_ + i;

			//And search
			trieNode* node_fallback;
			node_fallback = srchnode_ (sub_ , &root);

			//Did we get it?
			if (node_fallback)
			{
				//Save it
				node_ ->failn = node_fallback;

				//Exit from this loop
				break;
			}
		}
	}

	//Iterate all its children
	for (nm_iterator i = node_ ->nextn.begin(); i != node_->nextn.end(); ++i)
		//Build the index
	{
		char* newstring_ = new char[len_ + 2];
		strcpy (newstring_, string_); // Fast strcat with char
		newstring_[len_] = i ->first;
		newstring_[len_ + 1] = 0;

		buildti_ (newstring_, i ->second, len_ + 1);

		delete[] newstring_;
	}
}

searchEngine::searchEngine (const char* const* strings_ /*= 0*/, int size_ /*= 0*/)
{
	// Root node is reset at creation

	if (strings_)
	{
		__assert (size_, "Invalid array size");

		load (strings_, size_);
	}
}

searchEngine::~searchEngine()
{
}

void searchEngine::load (const char* const* strings_, unsigned size_) // Loads string array
{
	__assert (strings_, "Invalid pointer");
	__assert (size_, "Invalid pointer");

	for (unsigned i = 0; i < size_; ++i)
		addstr_ (strings_[i], &root, i);

	buildti_ ("", &root, 0);
}

void searchEngine::search (const char* string_,
						  const char*& occur_,
						  unsigned& what_,
						  unsigned& len_) // Actual search
{
	// After-final checks
	bool afcheck = 0;

	// Matched string length
	unsigned length = 0;

	// Our node position
	trieNode* node_cur = &root;

	// Iterate the string
	for (unsigned i = 0; string_[i]; ++i)
	{
		// Did we switched node (proceeded in a tree) already
		bool switched = 0;

		// Loop while we got something
		for (;;)
		{
			// Look for the current char in a tree
			nm_iterator nextnode = node_cur ->nextn.find (string_[i]);

			// We have failed to find - current branch is invalid.
			if (nextnode == node_cur ->nextn.end())
			{
				// First check if this is after a final node
				// If yes, then return immediately
				// Correct results are already loaded
				if (afcheck)
					return;

				// Then check if we have a failure node.
				// It is the node to fallback (last common node between this and
				// other "needle" variants).

				if (node_cur ->failn) // We have it
				{
					// What is the depth difference?
					unsigned short depth_diff =
						 (node_cur ->depth) - (node_cur ->failn ->depth - 1);

					// This is how many chars to remove

					length -= depth_diff;

					// Go to the failure node
					node_cur = node_cur ->failn;

					// We have changed the node
					switched = 1;
				}

				else // No common node, start at root again
				{
					node_cur = &root;

					// Reset search string length
					length = 0;

					// If we have changed the node
					if (switched)
						// We need to do this over
						i--;

					// Exit this loop
					break;
				}
			}

			else
			{
				// Add the char
				length++;

				// Save the new node
				node_cur = nextnode ->second;

				// Exit the loop
				break;
			}
		}

		// Is this a final node?
		if (node_cur ->isfinal)
		{
			//We got our data, exit
			occur_ = string_ + i - length + 1;
			what_ = node_cur ->strnum;
			len_ = length;

			// If no next nodes or sting end - nothing to check, just return immediately
			if (node_cur ->nextn.empty() || !string_[i + 1])
				return;

			else
			{
				afcheck = 1; // Else set the after-final checking flag
				continue; // And continue to skip afcheck reset
			}
		}

		// If afcheck has succeeded, it is no longer the check.
		// It is a normal search then.
		afcheck = 0;
	} // for

	//Nothing found, exit
	occur_ = 0;
	what_ = 0;
	len_ = 0;
	return;
}

searchEngine::trieNode::trieNode (unsigned short depth_ /*= 0*/) :
isfinal (0),
failn (0),
depth (depth_),
strnum (0)
{
}

searchEngine::trieNode::~trieNode()
{
	for (nm_iterator i = nextn.begin(); i != nextn.end(); ++i)
		delete i ->second;
}

ImplementDescriptor(searchEngine, "string search engine", MOD_APPMODULE);