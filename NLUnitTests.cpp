#include "stdafx.h"

#include <uXray/fxlog_console.h>
#include "MMU.h"
#include "AssemblyIO.h"
#include "CommandSet_original.h"
#include "Linker.h"
#include "Logic.h"

int main (int argc, char** argv)
{
	Debug::System::Instance().SetTargetProperties (Debug::CreateTarget ("stderr",
																		EVERYTHING,
																		EVERYTHING),
												   &FXConLog::Instance());

// 	Debug::API::SetStaticTypeVerbosity< StaticAllocator<int, 1> > (Debug::E_USER);


	Processor::ProcessorAPI api;

	ProcessorImplementation::MMU mmu;
	ProcessorImplementation::UATLinker linker;
	ProcessorImplementation::AsmHandler asm_rw;
	ProcessorImplementation::Logic logic;
	ProcessorImplementation::CommandSet_mkI cset;

	api.Attach (&mmu);
	api.Attach (&linker);
	api.Attach (&logic);
	api.Attach (&cset);
	api.Flush();

	api.Attach (&asm_rw);
	FILE* read = fopen ("input.dasm", "rt");
	api.Load (read, 0);
	api.Delete();

	Debug::System::Instance().CloseTargets();
}