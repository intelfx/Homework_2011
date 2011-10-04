#include "stdafx.h"
#include "Stack.h"
#include "Processor.h"
#include <uXray/fxlog_console.h>

void Interactive (Processor& proc)
{
	smsg (E_INFO, E_USER, "Running in interactive mode");
	smsg (E_INFO, E_USER, "Command set version is %d:%d", proc.GetMajorVersion(), proc.GetMinorVersion());
	smsg (E_INFO, E_USER, "Dumping command set:");
	proc.DumpCommandSet();
	smsg (E_INFO, E_USER, "");
	proc.AllocContext();
	proc.SetExecuteInPlace (1);
	proc.Decode (stdin);
}

int main (int argc, char** argv)
{
	Debug::System::Instance().SetTargetProperties (Debug::CreateTarget ("stderr", EVERYTHING, EVERYTHING & ~MASK (Debug::E_DEBUGLIB)),
												   &FXConLog::Instance());

	srand (time (0));
	int a1 = rand(), a2 = rand();
	smsg (E_INFO, E_USER, "Stack self-tests");
	Stack<int> stack;
	stack.Push (a1);
	stack.Push (a2);

	__sassert (stack.Top() == a2, "Top() test failed");
	smsg (E_INFO, E_USER, "Top is %d", stack.Top());

	stack.Push (stack.Pop() - stack.Pop());
	__sassert (stack.Pop() == a2 - a1, "Pop() order test failed");

	smsg (E_INFO, E_USER, "Starting processor");
	Processor proc;

	if (argc == 1)
	{
		Interactive (proc);
	}

	else if (argc == 2)
	{
		if (!strcasecmp (argv[1], "help"))
		{
			fprintf (stderr, "Usage: %s compile|execute|decompile|help <filename>\n", argv[0]);
			fprintf (stderr, "\tprefix: Used in interactive mode for buffer management commands\n");
			fprintf (stderr, "\tcompile: Compiles given <filename> into bytecode\n");
			fprintf (stderr, "\texecute: Executes given <filename> as bytecode\n");
			fprintf (stderr, "\tdecompile: Disassembles given <filename>\n");
			fprintf (stderr, "\tnothing: Enter interactive mode, <filename> is prefix for inline buffer management\n");
		}

		else
		{
			proc.SetFilenamePrefix (argv[1]);
			Interactive (proc);
		}
	}

	else if (argc == 3)
	{
		if (!strcasecmp (argv[1], "execute"))
		{
			proc.AllocContext();
			proc.SetExecuteInPlace (0);

			smsg (E_INFO, E_USER, "Executing bytecode from file \"%s\"", argv[2]);
			FILE* rfile = fopen (argv[2], "rb");
			proc.Load (rfile);
			fclose (rfile);

			proc.ExecuteBuffer();
		}

		else if (!strcasecmp (argv[1], "compile"))
		{
			proc.AllocContext();
			proc.SetExecuteInPlace (0);
			size_t used_buffer = proc.GetCurrentBuffer();

			smsg (E_INFO, E_USER, "Loading ASM from file \"%s\"", argv[2]);
			FILE* rfile = fopen (argv[2], "r");
			proc.Decode (rfile);
			fclose (rfile);

			char* prefix = reinterpret_cast<char*> (malloc (line_length));
			char* output = reinterpret_cast<char*> (malloc (line_length));

			sscanf (argv[2], "%s.%*s", prefix);
			snprintf (output, line_length, "%s.dbin", prefix);
			free (prefix);

			smsg (E_INFO, E_USER, "Using file \"%s\" to write bytecode", output);
			FILE* wfile = fopen (output, "wb");
			free (output);

			proc.DumpBC (wfile, 0, used_buffer);
			fclose (wfile);
		}

		else if (!strcasecmp(argv[1], "decompile"))
		{
			proc.AllocContext();
			proc.SetExecuteInPlace (0);
			size_t used_buffer = proc.GetCurrentBuffer();

			smsg (E_INFO, E_USER, "Loading bytecode from file \"%s\"", argv[2]);
			FILE* rfile = fopen (argv[2], "rb");
			proc.Load (rfile);
			fclose (rfile);

			char* prefix = reinterpret_cast<char*> (malloc (line_length));
			char* output = reinterpret_cast<char*> (malloc (line_length));

			sscanf (argv[2], "%s.%*s", prefix);
			snprintf (output, line_length, "%s.dasm", prefix);
			free (prefix);

			smsg (E_INFO, E_USER, "Using file \"%s\" to write ASM", output);
			FILE* wfile = fopen (output, "w");
			free (output);

			proc.DumpAsm (wfile, used_buffer);
			fclose (wfile);
		}

		else
			__sasshole ("Unknown command: %s", argv[1]);
	}

	else
		__sasshole ("Wrong number of parameters: %d", argc - 1);

	Debug::System::Instance.ForceDelete();
}