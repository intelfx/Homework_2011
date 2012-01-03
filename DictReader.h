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

	const wchar_t* delimiter_sections = L"\t";
	const wchar_t* delimiter_multiple = L";";
	wchar_t* tmp_ptr;

	std::vector<wchar_t*> tokens_source, tokens_translation;

	while (fgetws (input_buf, STATIC_LENGTH, dictionary))
	{
		++line_num;
		wstr_tolower (input_buf);

		if (wchar_t* sec_id = wcsstr (input_buf, L"###"))
		{
			smsg (E_INFO, E_DEBUG, "Section identifier \"%ls\" on line %zu", input_buf, line_num);

			char src_language[25], dst_language[25];
			int scan_result = swscanf (sec_id, L"### %[^-]-%[^-]", src_language, dst_language);
			__sverify (scan_result == 2, "Invalid section identifier \"%ls\": scanf() says %d", sec_id, scan_result);

			if ((cfg.src == decode_language (src_language)) &&
				(cfg.dest == decode_language (dst_language)))
			break;
		}
	}

	__sassert (!feof (dictionary), "Section \"%s -> %s\" not found in dictionary",
			   langdesc[cfg.src], langdesc[cfg.dest]);

	smsg (E_INFO, E_VERBOSE, "Reading section \"%s -> %s\" from line %zu",
		  langdesc[cfg.src], langdesc[cfg.dest], line_num);

	while (fgetws (input_buf, STATIC_LENGTH, dictionary))
	{
		++line_num;
		wstr_tolower (input_buf);

		if (wcsstr (input_buf, L"###"))
			break;

		if (wchar_t* cmt = wcschr (input_buf, L'#'))
			*cmt = L'\0';

		wchar_t* sec_src = wcstok (input_buf, delimiter_sections, &tmp_ptr);

		if (!sec_src)
			continue; /* empty line */

		wchar_t* sec_tran = wcstok (0, delimiter_sections, &tmp_ptr);
		wchar_t* sec_part = wcstok (0, delimiter_sections, &tmp_ptr);

		bool parse_ok = sec_src && sec_tran && *sec_src && *sec_tran;

		__sverify (!cfg.dictionary_error_fatal || parse_ok, "Malformed dictionary line %zu: \"%ls\" - \"%ls\" - \"%ls\"",
				   line_num, sec_src, sec_tran, sec_part);

		if (!parse_ok)
			smsg (E_WARNING, E_VERBOSE, "Malformed dictionary line %zu: \"%ls\" - \"%s\" - \"%s\"",
				  line_num, sec_src, sec_tran, sec_part);

		helper_tokenize_string (sec_src, tokens_source, delimiter_multiple);
		helper_tokenize_string (sec_tran, tokens_translation, delimiter_multiple);

		for (auto srci = tokens_source.begin(); srci != tokens_source.end(); ++srci)
		{
			if (wcschr (*srci, L' ')) continue; // we do not work with multiword sources

			for (auto trni = tokens_translation.begin(); trni != tokens_translation.end(); ++trni)
			{
				smsg (E_INFO, E_DEBUG, "Adding \"%ls\" -> \"%ls\"", *srci, *trni);
				result.Add (*srci, *trni, !cfg.check_for_multi_insertion);
			}
		}
	}

	smsg (E_INFO, E_VERBOSE, "Section read completed at line %zu", line_num);

	return std::move (result);
}

#endif // _DICTREADER_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
