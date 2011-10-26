#include "stdafx.h"
#include "Interfaces.h"

// -----------------------------------------------------------------------------
// Library		Antided
// File			Interfaces.cpp
// Author		intelfx
// Description	Interfaces supplementary function implementation.
// -----------------------------------------------------------------------------

namespace OLD_Processor
{

void IExecutor::AttachProcessor (IProcessor* proc)
{
	OLD_Processor::IModuleBase::AttachProcessor (proc);
	proc_ ->AttachExecutor (this);
}

void IExecutor::DetachProcessor()
{
	proc_ ->AttachExecutor (0);
	OLD_Processor::IModuleBase::DetachProcessor();
}

void ILinker::AttachProcessor (IProcessor* proc)
{
	OLD_Processor::IModuleBase::AttachProcessor (proc);
	proc_ ->AttachLinker (this);
}

void ILinker::DetachProcessor()
{
	proc_ ->AttachLinker (0);
	OLD_Processor::IModuleBase::DetachProcessor();
}

void IMMU::AttachProcessor (IProcessor* proc)
{
	OLD_Processor::IModuleBase::AttachProcessor (proc);
	proc_ ->AttachMMU (this);
}

void IMMU::DetachProcessor()
{
	proc_ ->AttachMMU (0);
	OLD_Processor::IModuleBase::DetachProcessor();
}

void IModuleBase::AttachProcessor (IProcessor* proc)
{
	__assert (proc, "Attaching to NULL processor");
	proc_ = proc;
}

void IModuleBase::DetachProcessor()
{
	proc_ = 0;
}

} // namespace Processor