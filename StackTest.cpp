#include "stdafx.h"
#include "Stack.h"
#include "Processor.h"
#include <uXray/fxlog_console.h>

void Interactive (OLD_Processor& proc)
{
	smsg (E_INFO, E_USER, "Running in interactive mode");
	smsg (E_INFO, E_USER, "Command set version is %d:%d", proc.GetMajorVersion(), proc.GetMinorVersion());
	smsg (E_INFO, E_USER, "Dumping command set:");
	proc.DumpCommandSet();

	while (true)
	{
		proc.InitContexts();
		proc.AllocContextBuffer();
		proc.SetExecuteInPlace (1);
		bool result = proc.Decode (stdin);

		smsg (E_INFO, E_USER, "%s. Type q<Enter> to quit", result ? "Succeeded." : "Failed.");
		char response = getchar();
		if (response == 'q' || response == 'Q')
			break;
	}
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

	stack.Push (stack.Pop() - stack.Pop());
	__sassert (stack.Pop() == a2 - a1, "Pop() order test failed");

	smsg (E_INFO, E_USER, "Starting processor");
	OLD_Processor proc;

	if (argc == 1)
	{
		Interactive (proc);
	}

	else if (argc == 2)
	{
		if (strstr (argv[0], "processor"))
		{
			proc.AllocContextBuffer();
			proc.SetExecuteInPlace (0);

			smsg (E_INFO, E_USER, "Executing bytecode from file \"%s\"", argv[2]);
			FILE* rfile = fopen (argv[1], "rb");
			bool result = proc.Load (rfile);
			fclose (rfile);

			if (!result)
			{
				Debug::System::Instance().CloseTargets();
				return 1;
			}

			proc.NextContextBuffer();

			for (unsigned i = 0; i < proc.GetRegistersNum(); ++i)
			{
				smsg (E_INFO, E_USER, "Enter register #%d", i);
				calc_t value;
				scanf ("%lg", &value);
				proc.AccessRegister (i, value);
			}

			proc.ExecuteBuffer();
		}

		else if (strstr (argv[0], "compiler"))
		{
			proc.AllocContextBuffer();
			proc.SetExecuteInPlace (0);
			size_t used_buffer = proc.GetCurrentBuffer();

			smsg (E_INFO, E_USER, "Loading ASM from file \"%s\"", argv[2]);
			FILE* rfile = fopen (argv[1], "r");
			bool result = proc.Decode (rfile);
			fclose (rfile);

			if (!result)
			{
				Debug::System::Instance().CloseTargets();
				return 1;
			}

			char* prefix = reinterpret_cast<char*> (malloc (line_length));
			char* output = reinterpret_cast<char*> (malloc (line_length));

			sscanf (argv[1], "%s.%*s", prefix);
			snprintf (output, line_length, "%s.dbin", prefix);
			free (prefix);

			smsg (E_INFO, E_USER, "Using file \"%s\" to write bytecode", output);
			FILE* wfile = fopen (output, "wb");
			free (output);

			proc.DumpBC (wfile, 0, used_buffer);
			fclose (wfile);
		}

		else if (strstr (argv[0], "decompiler"))
		{
			proc.AllocContextBuffer();
			proc.SetExecuteInPlace (0);
			size_t used_buffer = proc.GetCurrentBuffer();

			smsg (E_INFO, E_USER, "Loading bytecode from file \"%s\"", argv[2]);
			FILE* rfile = fopen (argv[1], "rb");
			proc.Load (rfile);
			fclose (rfile);

			char* prefix = reinterpret_cast<char*> (malloc (line_length));
			char* output = reinterpret_cast<char*> (malloc (line_length));

			sscanf (argv[1], "%s.%*s", prefix);
			snprintf (output, line_length, "%s.dasm", prefix);
			free (prefix);

			smsg (E_INFO, E_USER, "Using file \"%s\" to write ASM", output);
			FILE* wfile = fopen (output, "w");
			free (output);

			proc.DumpAsm (wfile, used_buffer);
			fclose (wfile);
		}

		else
		{
			proc.SetFilenamePrefix (argv[1]);
			Interactive (proc);
		}
	}

	else
		__sasshole ("Wrong number of parameters: %d", argc - 1);

	Debug::System::Instance().CloseTargets();
}