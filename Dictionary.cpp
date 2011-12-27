#include "stdafx.h"
#include "Hashtable.h"

#include <uXray/fxlog_console.h>

#include "time_ops.h"

#include "DictStructures.h"
#include "DictOperations.h"
#include "DictReader.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			Dictionary.cpp
// Author		intelfx
// Description	Dictionary with HTML output, as per hometask assignment #4.b.
// -----------------------------------------------------------------------------

/*
 * Hasher representation for std::wstring support.
 */

template<>
void ByteStreamRepresentation<std::wstring>::operator() (const std::wstring* object,
														 const void** pointer,
														 size_t* size)
{
	*pointer = object ->c_str();
	*size = object ->size();
}

const char* option_string = "w:i:o:ntvms:d:";
FILE *input = 0, *output = 0;
const size_t translate_min_length = 3;

void parse_and_prepare (int argc, char** argv) /* non-LOCALE */
{
	int getopt_result;
	const char* input_file;
	size_t count = 0;

	while ( (getopt_result = getopt (argc, argv, option_string)) != -1)
	{
		__sverify (getopt_result != '?', "Invalid option provided: '%c'", optopt);
		__sverify (getopt_result != ':', "No argument provided to option '%c'", optopt);

		switch (getopt_result)
		{
		case 'w':
			smsg (E_INFO, E_VERBOSE, "Setting dictionary to \"%s\"", optarg);
			dictionary = fopen (optarg, "rt");
			__sverify (dictionary, "Dictionary open failed: %s", strerror (errno));
			break;

		case 'i':
			smsg (E_INFO, E_VERBOSE, "Setting input file to \"%s\"", optarg);
			input = fopen (optarg, "rt");
			__sverify (input, "Input file open failed: %s", strerror (errno));

			input_file = optarg;
			break;

		case 'o':
			smsg (E_INFO, E_VERBOSE, "Setting output file to \"%s\"", optarg);
			output = fopen (optarg, "wt");
			__sverify (output, "Output file open failed: %s", strerror (errno));
			break;

		case 'n':
			smsg (E_WARNING, E_VERBOSE, "Disabling word normalisation");
			cfg.use_normalisation = 0;
			break;

		case 't':
			smsg (E_WARNING, E_VERBOSE, "Dictionary file format errors are tolerated");
			cfg.dictionary_error_fatal = 0;
			break;

		case 'v':
			smsg (E_WARNING, E_VERBOSE, "Switching on internal O(n) verification");
			cfg.skip_verify = 0;
			break;

		case 'm':
			smsg (E_WARNING, E_VERBOSE, "Disabling key uniquity check");
			cfg.check_for_multi_insertion = 0;
			break;

		case 's':
			cfg.src = decode_language (optarg);
			smsg (E_INFO, E_VERBOSE, "Source language set to \"%s\"", langdesc[cfg.src]);
			break;

		case 'd':
			cfg.dest = decode_language (optarg);
			smsg (E_INFO, E_VERBOSE, "Destination language set to \"%s\"", langdesc[cfg.dest]);
			break;

		default:
			__sasshole ("Invalid option: '%c' or switch error", getopt_result);
			break;
		}

		++count;
	} // while()

	__sverify (input, "No input file provided");
	__sverify (dictionary, "No dictionary file provided");
	if (!output)
	{
		char *outname = 0, *input_prefix = 0;

		sscanf (input_file, "%a[^.]", &input_prefix);
		__sassert (input_prefix, "Failed to read input file prefix");
		fx_asprintf (&outname, "%s_translation.htm", input_prefix);
		__sassert (outname, "Failed to generate output file name");

		smsg (E_WARNING, E_VERBOSE, "Output file is not specified. Using fallback \"%s\"", outname);
		output = fopen (outname, "wt");
		__sverify (output, "Output file open failed: %s", strerror (errno));

		free (input_prefix);
		free (outname);
	}
}

wchar_t* read_input() /* LOCALE */
{
	__sassert (input, "Error in input file");
	rewind (input);

	size_t length;
	fseek (input, 0, SEEK_END);
	length = ftell (input);
	rewind (input);

	char* data = reinterpret_cast<char*> (malloc (length + 1));
	wchar_t* wdata = reinterpret_cast<wchar_t*> (malloc (sizeof (wchar_t) * (length + 1)));
	__sassert (data, "Input data malloc failed [size %zu]", length + 1);
	__sassert (wdata, "Wide data malloc failed [size %zu]", length + 1);

	// read
	size_t result = fread (data, 1, length, input);
	__sverify (result == length, "I/O error reading input on position %zu: %s", result, strerror (errno));
	data[result] = 0;

	// convert
	size_t conv_r = mbstowcs (wdata, data, length + 1);
	free (data);

	__sverify (conv_r != static_cast<size_t> (-1),
			   "mbstowcs() failed, invalid characters detected in input file");

	// postprocess and return
	wstr_tolower (wdata);
	return wdata;
}

void parse_input (wchar_t* data, InputData& dest, wchar_t** title) /* LOCALE */
{
	__sassert (data, "Invalid data");
	static const wchar_t* newline_delim = L"\n\r";

	wchar_t word_delim[0x100];

	{
		wchar_t* ptr = word_delim;
		for (char ch = 0x0; isascii (ch); ++ch)
			if (isspace (ch) || ispunct (ch))
			{
				size_t cnv_r = mbtowc (ptr, &ch, 1);
				__sassert (cnv_r != static_cast<size_t> (-1),
						   "Internal error, invalid delimiting character '%c'", ch);

				ptr += cnv_r;
			}

		*ptr++ = L'\0';
	}

	smsg (E_INFO, E_DEBUG, "Using \"%ls\" as delimiter string", word_delim);

	wchar_t *saveptr_nl, *saveptr_word;
	*title = wcstok (data, newline_delim, &saveptr_nl);

	dest.clear();
	while (wchar_t* line_token = wcstok (0, newline_delim, &saveptr_nl))
	{
		smsg (E_INFO, E_DEBUG, "Received line: \"%ls\" len %zu", line_token, wcslen (line_token));

		dest.push_back (InputData::value_type());
		while (wchar_t* word_token = wcstok (line_token, word_delim, &saveptr_word))
		{
			line_token = 0;
			dest.back().push_back (Entry (word_token));
		}
	}
}

void debug_print_data (const InputData& src, const wchar_t* title, bool second_pass = 0) /* LOCALE */
{
	printf ("\nDebug data dump. Title is \"%ls\"\n\n", title);

	for (InputData::const_iterator i = src.begin(); i != src.end(); ++i)
	{
		printf ("NEWLINE\n");
		for (std::vector<Entry>::const_iterator j = i ->begin(); j != i ->end(); ++j)
		{
			printf ("* WORD: \"%ls\"", j ->word);

			if (second_pass && j ->tran)
				printf (" -> \"%ls\"", j ->tran);

			else if (second_pass)
				printf (" (no translation)");

			putchar ('\n');
		}
	}

	printf ("Debug print ends\n");
}

const wchar_t* attempt_translate_word (std::wstring& in_str, Dictionary& dict) /* LOCALE */
{
	Dictionary::Iterator it = dict.Find (in_str.c_str());

	return it.End() ? 0 : it ->data.c_str();
}

const char* debug_dump_operations (Entry& e) /* non-LOCALE */
{
	static char buffer [STATIC_LENGTH];

	char* ptr = buffer;
	size_t maxlen = STATIC_LENGTH;
	const char* fmt = "%s";

	for (size_t op = 1; op < S_SCOUNT; ++op)
		if (e.flags & MASK (op))
		{
			size_t len = snprintf (ptr, maxlen, fmt, tr_op_names[op]);

			if (len >= maxlen)
				break;

			maxlen -= len;
			ptr += len;

			fmt = ", %s";
		}

	return buffer;
}

void translate_data (InputData& src, Dictionary& dict) /* LOCALE */
{
	size_t total = 0, translated = 0, skipped = 0, failed = 0;

	for (InputData::iterator i = src.begin(); i != src.end(); ++i)
	{
		for (std::vector<Entry>::iterator j = i ->begin(); j != i ->end(); ++j)
		{
			++total;
			Entry& tr_entry = *j;

			if (wcslen (tr_entry.word) < translate_min_length)
			{
				++skipped;

				smsg (E_INFO, E_DEBUG, "Skipped word \"%ls\"", tr_entry.word);
				continue;
			}

			std::wstring working_string (tr_entry.word);
			smsg (E_INFO, E_DEBUG, "Translating word \"%ls\"", working_string.c_str());

			// If we do not use word operations, we set current_op to the end manually.
			size_t current_op = (cfg.use_normalisation) ? 0 : S_SCOUNT;

			const wchar_t* translation = 0;
			while (!(translation = attempt_translate_word (working_string, dict)) && (++current_op < S_SCOUNT))
				tr_entry.AttemptNormalisation (static_cast<NmOperation> (current_op), working_string);

			if (!translation)
			{
				++failed;

				smsg (E_WARNING, E_VERBOSE, "Translation failure on \"%ls\" (normalised to \"%ls\")",
					  tr_entry.word, working_string.c_str());
			}

			else
			{
				++translated;

				smsg (E_INFO, E_DEBUG, "OK: \"%ls\" -> \"%ls\" (transformations used: %s)",
					  tr_entry.word, translation, debug_dump_operations (tr_entry));
				tr_entry.tran = translation;
			}
		} // inner for()
	} // outer for()

	smsg (E_INFO, E_VERBOSE, "Translation completed");
	smsg (E_INFO, E_VERBOSE, "Stats: %zu words, from which %zu translated, %zu too short and %zu failed",
		  total, translated, skipped, failed);
}

int main (int argc, char** argv) /* LOCALE */
{
	Debug::System::Instance().SetTargetProperties (Debug::CreateTarget ("stderr", EVERYTHING, EVERYTHING),
												   &FXConLog::Instance());

	setlocale (LC_ALL, "");
	calibrate_and_setup_clock();

	try
	{
		clock_measure_start ("Parsing command arguments");
		parse_and_prepare (argc, argv);
		clock_measure_end();
	}

	catch (std::exception& e)
	{
		smsg (E_CRITICAL, E_USER, "Options parsing failed: %s", e.what());
		fprintf (stderr,
				 "Invalid call: %s\n"
				 "Usage: %s -w dictionary -i input [ -o output ] [ -s source_lang ] [ -d dest_lang ] [ -ntvm ]\n",
				 e.what(), argv[0]);

		Debug::System::Instance.ForceDelete();
		return 1;
	}

	Debug::API::SetDefaultVerbosity (Debug::E_USER);
	Debug::API::SetTypewideVerbosity (0, Debug::E_DEBUG);

	if (cfg.skip_verify)
	{
		Debug::API::ClrTypewideFlag ("Hashtable", Debug::OF_USEVERIFY);
		Debug::API::ClrTypewideFlag ("LinkedList", Debug::OF_USEVERIFY);
		Debug::API::ClrTypewideFlag ("LinkedListIterator", Debug::OF_USEVERIFY);
	}

	InputData input_data;

	clock_measure_start ("Reading dictionary");
	Dictionary dict_data (read_dictionary());
	clock_measure_end();

	clock_measure_start ("Reading input file");
	wchar_t *file_data = read_input(), *working_file_data = wcsdup (file_data), *title;
	parse_input (file_data, input_data, &title);
	clock_measure_end();

	clock_measure_start ("Translating words");
	translate_data (input_data, dict_data);
	clock_measure_end();

	debug_print_data (input_data, title, 1);

	input_data.clear();

	/* every input block became invalid by this point */
	free (file_data);
	free (working_file_data);

	fclose (input);
	fclose (output);
	fclose (dictionary);

	Debug::System::Instance.ForceDelete();
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
