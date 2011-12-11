#include "stdafx.h"
#include "Hashtable.h"

#include <uXray/fxlog_console.h>
#include <uXray/fxhash_functions.h>

#include <time.h>

// -----------------------------------------------------------------------------
// Library      Homework
// File         Hashtest.cpp
// Author       intelfx
// Description  Hash table demonstration and unit-tests.
// -----------------------------------------------------------------------------


timespec clock_resolution, start_clock;

static const size_t line_max = 0x100;
static const unsigned long nano_divisor = 1000000000;

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

	DictEntry& operator= (DictEntry && that)
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
		_1 (reinterpret_cast<char*> (malloc (line_max))),
		_2 (reinterpret_cast<char*> (malloc (line_max)))
	{
		strncpy (_1, s1, line_max);
		strncpy (_2, s2, line_max);
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
	timespec init, seek, dump;

	StatEntry (const char* name_, size_t total_, size_t used_, size_t* counts_,
			   timespec init_, timespec seek_, timespec dump_) :
	name (name_),
	total (total_),
	used (used_),
	counts (counts_),
	init (init_),
	seek (seek_),
	dump (dump_)
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
		dump = that.dump;

		that.counts = 0;

		return *this;
	}

	StatEntry (StatEntry&& that) :
	name (that.name),
	total (that.total),
	used (that.used),
	counts (that.counts),
	init (that.init),
	seek (that.seek),
	dump (that.dump)
	{
		that.counts = 0;
	}

	StatEntry (const StatEntry&) = delete;
	StatEntry& operator= (const StatEntry&) = delete;
};

void calibrate_and_setup_clock()
{
	timespec calibrate_source;

	int value = clock_getres (CLOCK_THREAD_CPUTIME_ID, &calibrate_source);
	__sassert (!value, "Clock resolution get failed: %s", strerror (errno));

	clock_resolution.tv_sec  = calibrate_source.tv_nsec / nano_divisor;
	clock_resolution.tv_nsec = calibrate_source.tv_nsec % nano_divisor;
	clock_resolution.tv_sec += calibrate_source.tv_sec;

	if (clock_resolution.tv_sec)
		smsg (E_WARNING, E_VERBOSE, "Clock resolution too big: %lu.%09lu",
			  clock_resolution.tv_sec, clock_resolution.tv_nsec);

	else
		smsg (E_INFO, E_VERBOSE, "Clock resolution: .%09lu",
			  clock_resolution.tv_nsec);
}

inline void clock_measure_start (const char* message)
{
	timespec time_source;

	smsg (E_INFO, E_VERBOSE, "Starting measure on \"%s\"", message);

	int value = clock_gettime (CLOCK_THREAD_CPUTIME_ID, &time_source);
	__sassert (!value, "Clock get failed: %s", strerror (errno));

	long nsec_error = clock_resolution.tv_nsec ? time_source.tv_nsec % clock_resolution.tv_nsec : 0;
	long sec_error  = clock_resolution.tv_sec  ? time_source.tv_sec  % clock_resolution.tv_sec  : 0;

	start_clock.tv_nsec = time_source.tv_nsec - nsec_error;
	start_clock.tv_sec  = time_source.tv_sec - sec_error;

	if (nsec_error || sec_error)
		smsg (E_WARNING, E_VERBOSE, "Clock reading does not obey resolution: %lu.%09lu -> errors [%lu.%09lu]",
			  time_source.tv_sec, time_source.tv_nsec, sec_error, nsec_error);
}

inline double timespec_to_fp (const timespec& time)
{
	double result;
	result  = time.tv_nsec;
	result /= nano_divisor;
	result += time.tv_sec;
	return result;
}

inline timespec clock_measure_end()
{
	timespec time_source;

	int value = clock_gettime (CLOCK_THREAD_CPUTIME_ID, &time_source);
	__sassert (!value, "Clock get failed: %s", strerror (errno));

	long nsec_error = clock_resolution.tv_nsec ? time_source.tv_nsec % clock_resolution.tv_nsec : 0;
	long sec_error  = clock_resolution.tv_sec  ? time_source.tv_sec  % clock_resolution.tv_sec  : 0;

	time_source.tv_nsec -= nsec_error;
	time_source.tv_sec  -= sec_error;

	time_source.tv_nsec -= start_clock.tv_nsec;
	time_source.tv_sec  -= start_clock.tv_sec;

	if (nsec_error || sec_error)
		smsg (E_WARNING, E_VERBOSE, "Clock reading does not obey resolution: %lu.%09lu -> errors [%lu.%09lu]",
			  time_source.tv_sec, time_source.tv_nsec, sec_error, nsec_error);

	smsg (E_INFO, E_VERBOSE, "Action has taken %lu.%09lu seconds",
		  time_source.tv_sec, time_source.tv_nsec);

	return time_source;
}

void write_stats (FILE* statfile, StatEntry* entries, size_t count)
{
	__sassert (statfile && !ferror (statfile), "Invalid stream");

	for (unsigned i = 0; i < count; ++i)
		fprintf (statfile, ",%s", entries[i].name);

	fprintf (statfile, "\nInitialise time");
	for (unsigned i = 0; i < count; ++i)
		fprintf (statfile, ",\"%f\"", timespec_to_fp (entries[i].init));

	fprintf (statfile, "\nSeek time");
	for (unsigned i = 0; i < count; ++i)
		fprintf (statfile, ",\"%f\"", timespec_to_fp (entries[i].seek));

	fprintf (statfile, "\nDump time");
	for (unsigned i = 0; i < count; ++i)
		fprintf (statfile, ",\"%f\"", timespec_to_fp (entries[i].dump));

	fprintf (statfile, "\n,,,\n");
	for (unsigned i = 0; i < count; ++i)
		fprintf (statfile, "%s,", entries[i].name);

	size_t cellcount = entries[0].total;
	for (size_t i = 1; i < count; ++i)
		__sassert (entries[i].total == cellcount, "Cell count mismatch in statistics");

	for (size_t i = 0; i < cellcount; ++i)
	{
		fprintf (statfile, "\n");

		for (size_t j = 0; j < count; ++j)
			fprintf (statfile, "\"%zu\",", entries[j].counts[i]);
	}
}

void test_dictionary (Hashtable<std::string, std::string>::hash_function hasher,
					  const std::vector<DictEntry>& input,
					  std::vector<StatEntry>& stat,
					  const char* name)
{
	Hashtable<std::string, std::string> dictionary (hasher);
	timespec stat_init_time, stat_seek_time, stat_dump_time;
	size_t total, used, *counts;

	clock_measure_start ("Dictionary initialize");

	for (auto i = input.cbegin(); i != input.cend(); ++i)
		dictionary.Add (i ->_1, i ->_2);

	stat_init_time = clock_measure_end();

	clock_measure_start ("Dictionary verify (sequential find)");

	for (auto i = input.cbegin(); i != input.cend(); ++i)
	{
		auto it = dictionary.Find (i ->_1);
		__sassert (!it.End(), "Not found in dictionary: \"%s\"", i ->_1);
	}

	stat_seek_time = clock_measure_end();

	clock_measure_start ("Dictionary statistics dump");
	dictionary.ReadStats (&total, &used, &counts);
	stat_dump_time = clock_measure_end();

	stat.push_back (StatEntry (name, total, used, counts, stat_init_time, stat_seek_time, stat_dump_time));
}

int main (int argc, char** argv)
{
	Debug::System::Instance().SetTargetProperties (Debug::CreateTarget ("stderr", EVERYTHING, EVERYTHING),
												   &FXConLog::Instance());

	Debug::API::SetStaticTypeVerbosity< LinkedList<int> > (Debug::E_USER);
	Debug::API::SetStaticTypeVerbosity< LinkedList<int>::Iterator > (Debug::E_USER);
	Debug::API::SetStaticTypeVerbosity< Hashtable<int, int> > (Debug::E_USER);
	Debug::API::SetStaticTypeVerbosity<void> (Debug::E_VERBOSE);
	Debug::API::ClrStaticTypeFlag< LinkedList<int> > (Debug::OF_USEVERIFY);

	const char* filename = "dictionary.txt";

	char sstr[line_max], str1[line_max], str2[line_max];
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
		if (!fgets (sstr, line_max, dict))
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
	test_dictionary (&hasher_xroll, dict_data, stat_data, "xor/roll hash");

	smsg (E_INFO, E_USER, "Testing with STL hasher");
	test_dictionary (&hasher_stl, dict_data, stat_data, "STL hash");

	smsg (E_INFO, E_USER, "Writing statistics");

	FILE* statfile = fopen ("stats.csv", "wt");
	__sverify (statfile, "Unable to open stats.csv: %s", strerror (errno));
	write_stats (statfile, stat_data.data(), stat_data.size());

	fclose (statfile);

	smsg (E_INFO, E_USER, "Finished writing statistics");

	Debug::System::Instance.ForceDelete();
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
