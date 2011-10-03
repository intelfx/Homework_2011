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

	smsg (E_INFO, E_USER, "She's alive, testing stack");
	Stack<int> stack;
	stack.Push (1);
	smsg (E_INFO, E_USER, "Popped value is %d", stack.Pop());

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
			fprintf (stderr, "\tcompile: Compiles given <filename> into bytecode");
			fprintf (stderr, "\texecute: Executes given <filename> as bytecode");
			fprintf (stderr, "\texecute: Disassembles given <filename>");
			fprintf (stderr, "\tnothing: Enter interactive mode, <filename> is prefix for inline buffer management");
		}

		else
		{
			proc.SetFilenamePrefix (argv[1]);
			Interactive (proc);
		}
	}

	Debug::System::Instance.ForceDelete();
}