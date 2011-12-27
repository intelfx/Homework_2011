#ifndef _DICTREADER_H
#define _DICTREADER_H

#include "build.h"
#include "DictStructures.h"

#include <uXray/fxhash_functions.h>

// -----------------------------------------------------------------------------
// Library		Homework
// File			DictReader.h
// Author		intelfx
// Description	File readers for the dictionary (assignment #4.b).
// -----------------------------------------------------------------------------

FILE* dictionary;

void helper_tokenize_string (wchar_t* string, std::vector<wchar_t*>& dest, const wchar_t* delim)
{
	wchar_t* tmp_ptr;
	dest.clear();

	while (wchar_t* token = wcstok (string, delim, &tmp_ptr))
	{
		dest.push_back (token);
		string = 0;
	}
}

Dictionary read_dictionary() /* LOCALE */
{
	__sassert (dictionary, "Error in dictionary file");
	rewind (dictionary);

	size_t line_num = 0;
	wchar_t input_buf[STATIC_LENGTH];

	Dictionary result (&hasher_stl, 0x100001);

	const wchar_t* delim_stage_1 = L"\t";
	const wchar_t* delim_stage_2 = L";";
	wchar_t* tmp_ptr;

	std::vector<wchar_t*> tokens_src, tokens_tran;

	while (fgetws (input_buf, STATIC_LENGTH, dictionary))
	{
		++line_num;
		wstr_tolower (input_buf);

		if (wchar_t* cmt = wcschr (input_buf, L'#'))
			*cmt = L'\0';

		wchar_t* sec_src = wcstok (input_buf, delim_stage_1, &tmp_ptr);

		if (!sec_src)
			continue; /* empty line */

		wchar_t* sec_tran = wcstok (0, delim_stage_1, &tmp_ptr);
		wchar_t* sec_part = wcstok (0, delim_stage_1, &tmp_ptr);

		bool parse_ok = sec_src && sec_tran && *sec_src && *sec_tran;

		__sverify (!cfg.dictionary_error_fatal || parse_ok, "Malformed dictionary line %zu: \"%ls\" - \"%ls\" - \"%ls\"",
				   line_num, sec_src, sec_tran, sec_part);

		if (!parse_ok)
			smsg (E_WARNING, E_VERBOSE, "Malformed dictionary line %zu: \"%ls\" - \"%s\" - \"%s\"",
				  line_num, sec_src, sec_tran, sec_part);

		helper_tokenize_string (sec_src, tokens_src, delim_stage_2);
		helper_tokenize_string (sec_tran, tokens_tran, delim_stage_2);

		for (auto srci = tokens_src.begin(); srci != tokens_src.end(); ++srci)
		{
			if (wcschr (*srci, L' ')) continue; // we do not work with multiword sources

			for (auto trni = tokens_tran.begin(); trni != tokens_tran.end(); ++trni)
			{
				smsg (E_INFO, E_DEBUG, "Adding \"%ls\" -> \"%ls\"", *srci, *trni);
				result.Add (*srci, *trni, !cfg.check_for_multi_insertion);
			}
		}
	}

	return std::move (result);
}

#endif // _DICTREADER_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
