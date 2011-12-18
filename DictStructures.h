#ifndef _DICT_STRUCTURES_H
#define _DICT_STRUCTURES_H

#include "build.h"
#include "Hashtable.h"

#include "DictOperations.h"

// -----------------------------------------------------------------------------
// Library      Homework
// File         DictStructures.h
// Author       intelfx
// Description  Supplementary data structures for the dictionary (assignment #4.b).
// -----------------------------------------------------------------------------

// Languages

enum Language
{
	L_ENGLISH = 0,
	L_RUSSIAN,
	LCOUNT
};

const char* langs[LCOUNT] =
{
	"eng",
	"rus"
};

const char* langdesc[LCOUNT] =
{
	"English",
	"Russian"
};

Language decode_language (const char* str)
{
	for (unsigned i = 0; i < LCOUNT; ++i)
		if (!strcmp (langs[i], str))
			return static_cast<Language> (i);

	__sverify (0, "Invalid language string: \"%s\"", str);
	return LCOUNT;
}

// Configuration

struct Configs
{
	bool dictionary_error_fatal;
	bool skip_verify;
	bool use_normalisation;
	bool check_for_multi_insertion;
	Language src, dest;

	Configs() :
	dictionary_error_fatal (1),
	skip_verify (1),
		use_normalisation (1),
	check_for_multi_insertion (1),
	src (L_ENGLISH),
	dest (L_RUSSIAN)
	{
	}
} cfg;

// Data entry

struct Entry /* LOCALE */
{
	wchar_t* word;
	const wchar_t* tran;
	size_t flags;

	explicit Entry (wchar_t* src) :
	word (src),
	tran (0),
	flags (0)
	{
		smsg (E_INFO, E_DEBUG, "Word parsed: \"%ls\"", src);
	}

	void AttemptNormalisation (NmOperation s_id)
	{
		smsg (E_INFO, E_DEBUG, "Attempting normalisation \"%s\" on \"%ls\"", tr_op_names[s_id], word);

		if ( (operations[s_id]) (word))
		{
			smsg (E_INFO, E_DEBUG, "Accomplished - result is \"%ls\"", word);
			flags |= MASK (s_id);
		}
	}
};

typedef std::vector< std::vector< Entry > > InputData;
typedef Hashtable< std::wstring, std::wstring > Dictionary;

#endif // _DICT_STRUCTURES_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
