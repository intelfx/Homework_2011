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

Dictionary read_dictionary() /* LOCALE */
{
	__sassert (dictionary, "Error in dictionary file");
	rewind (dictionary);

	size_t line_num = 0, successful_num = 0;
	wchar_t input_buf[STATIC_LENGTH], data_buf[2][STATIC_LENGTH];
	wchar_t designator[STATIC_LENGTH];
	Dictionary result (&hasher_xroll_plain, 0x100001);

	swprintf (designator, STATIC_LENGTH, L"== %s-%s", langs[cfg.src], langs[cfg.dest]);

	while (!feof (dictionary))
	{
		++line_num;

		fgetws (input_buf, STATIC_LENGTH, dictionary);
		wstr_tolower (input_buf);

		if (!wcscmp (designator, input_buf)) break; /* advance to section read */
	}

	__sverify (!feof (dictionary), "Could not find \"%s-%s\" section in dictionary file",
			   langs[cfg.src], langs[cfg.dest]);

	smsg (E_INFO, E_VERBOSE, "Reading \"%s-%s\" section from line %zu",
		  langs[cfg.src], langs[cfg.dest], line_num + 1);

	while (!feof (dictionary))
	{
		++line_num;
		++successful_num;

		wchar_t* gets_r = fgetws (input_buf, STATIC_LENGTH, dictionary);
		__sverify (gets_r, "fgetws() failed, invalid characters detected in dictionary file [line %zu]", line_num);
		wstr_tolower (input_buf);

		if (!wcsncmp (input_buf, designator, 2)) break; /* stop section read */

		if (! (line_num % 100))
			smsg (E_INFO, E_DEBUG, "Adding %zu-th line", line_num);

		data_buf[1][0] = L'\0';
		data_buf[0][0] = L'\0'; /* guard */

		int scanf_result = swscanf (input_buf, L"%l[^=]=%l[^=]", data_buf[0], data_buf[1]);

		if (scanf_result == EOF)
		{
			__sverify (!ferror (dictionary), "I/O error on line %zu: %s", line_num, strerror (errno));
			/* empty line, skip it */
		}

		else
		{
			bool parse_ok = (scanf_result == 2) && data_buf[0][0] && data_buf[1][0];

			__sverify (!cfg.dictionary_error_fatal || parse_ok,
					   "Malformed dictionary line %zu: [sscanf returned %d] \"%ls\"",
					   line_num, scanf_result, input_buf);

			if (!parse_ok)
				smsg (E_WARNING, E_VERBOSE, "Malformed dictionary line %zu: [sscanf returned %d] \"%ls\"",
					  line_num, scanf_result, input_buf);

			else
			{
				{
					wchar_t* ptr = data_buf[1];
					while (*ptr++)
						if (*ptr == L'\t')
							*ptr = L'|';
				}

				smsg (E_INFO, E_DEBUG, "Adding pair: \"%ls\" -> \"%ls\"",
					  data_buf[0],
					  data_buf[1]);

				result.Add (data_buf[0], data_buf[1], !cfg.check_for_multi_insertion);
			}
		} /* scanf_result != EOF */
	} /* while (!feof) */

	smsg (E_INFO, E_DEBUG, "Read %zu lines of section \"%s-%s\"",
		  successful_num, langs[cfg.src], langs[cfg.dest]);
	return std::move (result);
}

#endif // _DICTREADER_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
