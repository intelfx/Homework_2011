#include "stdafx.h"
#include "Hashtable.h"

#include <uXray/fxlog_console.h>
#include <uXray/fxhash_functions.h>

#include "time_ops.h"

// -----------------------------------------------------------------------------
// Library      Homework
// File         Hashtest.cpp
// Author       intelfx
// Description  Hash table demonstration, as per hometask assignment #4.a.
// -----------------------------------------------------------------------------

/*
 * Specialisation of representation function for hash table to correctly handle std::string.
 */

template<>
void ByteStreamRepresentation<std::string>::operator() (const std::string* object,
														const void** pointer,
														size_t* size)
{
	*pointer = object ->c_str();
	*size = object ->size();
}

struct DictEntry
{
	char *_1, *_2;

	DictEntry (DictEntry && that) :
		_1 (that._1),
		_2 (that._2)
	{
		that._1 = 0;
		that._2 = 0;
	}

	DictEntry& operator= (DictEntry&& that)
	{
		if (this == &that)
			return *this;

		_1 = that._1;
		_2 = that._2;

		that._1 = 0;
		that._2 = 0;

		return *this;
	}

	DictEntry (const char* s1, const char* s2) :
		_1 (reinterpret_cast<char*> (malloc (STATIC_LENGTH))),
		_2 (reinterpret_cast<char*> (malloc (STATIC_LENGTH)))
	{
		strncpy (_1, s1, STATIC_LENGTH);
		strncpy (_2, s2, STATIC_LENGTH);
	}

	~DictEntry()
	{
		free (_1);
		free (_2);
	}
};

struct StatEntry
{
	const char* name;
	size_t total, used, *counts;
	timespec init, seek;

	StatEntry (const char* name_, size_t total_, size_t used_, size_t* counts_,
			   timespec init_, timespec seek_) :
	name (name_),
	total (total_),
	used (used_),
	counts (counts_),
	init (init_),
	seek (seek_)
	{
	}

	~StatEntry()
	{
		free (counts);
	}


	StatEntry& operator= (StatEntry&& that)
	{
		if (this == &that)
			return *this;

		name = that.name;
		total = that.total;
		used = that.used;
		counts = that.counts;
		init = that.init;
		seek = that.seek;

		that.counts = 0;

		return *this;
	}

	StatEntry (StatEntry&& that) :
	name (that.name),
	total (that.total),
	used (that.used),
	counts (that.counts),
	init (that.init),
	seek (that.seek)
	{
		that.counts = 0;
	}

	StatEntry (const StatEntry&) = delete;
	StatEntry& operator= (const StatEntry&) = delete;
};

void write_stats (FILE* statfile, StatEntry* entries, size_t count)
{
	__sassert (statfile && !ferror (statfile), "Invalid stream");

	for (unsigned i = 0; i < count; ++i)
		fprintf (statfile, ";%s", entries[i].name);

	fprintf (statfile, "\nInitialise time");
	for (unsigned i = 0; i < count; ++i)
		fprintf (statfile, ";%f", timespec_to_fp (entries[i].init));

	fprintf (statfile, "\nSeek time");
	for (unsigned i = 0; i < count; ++i)
		fprintf (statfile, ";%f", timespec_to_fp (entries[i].seek));

	fprintf (statfile, "\n;;;\n");
	for (unsigned i = 0; i < count; ++i)
		fprintf (statfile, "%s;", entries[i].name);

	size_t cellcount = entries[0].total;
	for (size_t i = 1; i < count; ++i)
		__sassert (entries[i].total == cellcount, "Cell count mismatch in statistics");

	for (size_t i = 0; i < cellcount; ++i)
	{
		fprintf (statfile, "\n");

		for (size_t j = 0; j < count; ++j)
			fprintf (statfile, "%zu;", entries[j].counts[i]);
	}
}

void test_dictionary (Hashtable<std::string, std::string>::hash_function hasher,
					  const std::vector<DictEntry>& input,
					  std::vector<StatEntry>& stat,
					  const char* name)
{
	Hashtable<std::string, std::string> dictionary (hasher);
	timespec stat_init_time, stat_seek_time;
	size_t total, used, *counts;

	clock_measure_start ("Dictionary initialize");

	size_t added_count = 0;
	for (auto i = input.cbegin(); i != input.cend(); ++i, ++added_count)
	{
		dictionary.Add (i ->_1, i ->_2);
		if (!(added_count % 100))
			smsg (E_INFO, E_DEBUG, "Added %zu-th element", added_count);
	}

	stat_init_time = clock_measure_end();

	clock_measure_start ("Dictionary verify (sequential find)");

	for (auto i = input.cbegin(); i != input.cend(); ++i)
	{
		auto it = dictionary.Find (i ->_1);
		__sassert (!it.End(), "Not found in dictionary: \"%s\"", i ->_1);
	}

	stat_seek_time = clock_measure_end();

	dictionary.ReadStats (&total, &used, &counts);
	stat.push_back (StatEntry (name, total, used, counts, stat_init_time, stat_seek_time));
}

int main (int argc, char** argv)
{
	Debug::System::Instance().SetTargetProperties (Debug::CreateTarget ("stderr", EVERYTHING, EVERYTHING),
												   &FXConLog::Instance());


	Debug::API::SetDefaultVerbosity (Debug::E_USER);
	Debug::API::SetStaticTypeVerbosity< void > (Debug::E_VERBOSE);
 	Debug::API::ClrStaticTypeFlag< LinkedList<int> > (Debug::OF_USEVERIFY);

	const char* filename = "dictionary.txt";
	const char* outfile = "stats.csv";

	char sstr[STATIC_LENGTH], str1[STATIC_LENGTH], str2[STATIC_LENGTH];
	size_t dict_line = 0;

	FILE* dict;
	std::vector<DictEntry> dict_data;
	std::vector<StatEntry> stat_data;

	if (argc == 2)
		filename = argv[1];

	dict = fopen (filename, "rt");
	__sverify (dict, "Could not open file \"%s\": %s", filename, strerror (errno));

	while (!feof (dict))
	{
		if (!fgets (sstr, STATIC_LENGTH, dict))
			continue;

		++dict_line;

		int result = sscanf (sstr, "%s %s", str1, str2);

		if (result != 2)
			continue;

		smsg (E_INFO, E_DEBUG, "Read dictionary: \"%s\" -> \"%s\"", str1, str2);
		dict_data.push_back (DictEntry (str1, str2));
	}

	fclose (dict);

	smsg (E_INFO, E_USER, "Read completed: %zu lines read", dict_line);

  	smsg (E_INFO, E_USER, "Testing with 'constant' hasher");
  	test_dictionary (&hasher_zero, dict_data, stat_data, "Constant");

	smsg (E_INFO, E_USER, "Testing with 'length' hasher");
	test_dictionary (&hasher_length, dict_data, stat_data, "Length");

	smsg (E_INFO, E_USER, "Testing with 'sum' hasher");
	test_dictionary (&hasher_sum, dict_data, stat_data, "Byte sum");

	smsg (E_INFO, E_USER, "Testing with 'xor/roll' hasher");
	test_dictionary (&hasher_xroll_plain, dict_data, stat_data, "xor/roll hash");

	smsg (E_INFO, E_USER, "Testing with STL hasher");
	test_dictionary (&hasher_stl, dict_data, stat_data, "STL hash");

	smsg (E_INFO, E_USER, "Writing statistics");

	FILE* statfile = fopen (outfile, "wt");
	__sverify (statfile, "Unable to open \"%s\": %s", outfile, strerror (errno));
	write_stats (statfile, stat_data.data(), stat_data.size());

	fclose (statfile);

	smsg (E_INFO, E_USER, "Finished writing statistics");

	Debug::System::Instance.ForceDelete();
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
