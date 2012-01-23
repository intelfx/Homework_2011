#include "stdafx.h"
#include "Huffman.h"

#include <uXray/fxlog_console.h>

// -----------------------------------------------------------------------------
// Library		Homework
// File			Huffman.h
// Author		intelfx
// Description	Huffman compression implementation (as per hometask assignment #5.a)
// -----------------------------------------------------------------------------

void PrintHelp()
{
	fprintf (stderr, "Usage: %s [-p threads] [-dm] input... output\n", program_invocation_name);
	fprintf (stderr, "\t -p threads         use specified count of threads\n"
					 "\t -d                 enable debug non-binary output\n"
					 "\t -m					use memory-mapped I/O\n");

	exit (1);
}

void ReadSettingsInitialise (int argc, char** argv)
{
	static const char* getopt_spec = "+p:dh";

	char next_option = -1;

	while ((next_option = getopt (argc, argv, getopt_spec)) != -1)
	{
		case 'p':
			settings.threads_num = atoi (optarg);
			__sverify (settings.threads_num < 1024, "Thread number negative or insane: %hd",
					   settings.threads_num);

			break;

		case 'd':
			smsg (E_WARNING, E_VERBOSE, "Using text-mode debug output!");
			settings.flags |= MASK (FL_DO_DEBUG_WRITE);
			break;

		case 'h':
			PrintHelp();
			break;

		case '?':
			__sasshole ("Invalid option detected: '%c'. Bailing out", optopt);
			break;

		default:
			__sasshole ("Invalid character returned by getopt(): '%c'. Bailing out", next_option);
			break;
	}

	const char* previous_file = 0;

	for (int argument = optind; argument < argc; ++argument)
	{
		if (previous_file) // open non-last file as input
		{
			msg (E_INFO, E_VERBOSE, "Opening file \"%s\" as input", previous_file);
			settings.input_files.push_back (FileRecord (previous_file));
		}

		previous_file = argv[argument];
	}

	msg (E_INFO, E_VERBOSE, "Opening file \"%s\" as output", previous_file);
	settings.output_file = fopen (previous_file, "wb");
	__sassert (settings.output_file, "Output file \"%s\": unable to open: fopen() says %s", strerror (errno));
}

int main (int argc, char** argv)
{
	Debug::System::SetTargetProperties (Debug::CreateTarget ("stderr", EVERYTHING, EVERYTHING),
										&FXConLog::Instance());

	calibrate_and_setup_clock();

	clock_measure_start ("Parsing command-line arguments and opening files");
	ReadSettingsInitialise();
	clock_measure_end();


}