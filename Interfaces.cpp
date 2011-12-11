#include "stdafx.h"
#include "Interfaces.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			Interfaces.cpp
// Author		intelfx
// Description	Interfaces supplementary function implementation.
// -----------------------------------------------------------------------------

namespace Processor
{

IProcessor::~IProcessor() = default;
IReader::~IReader() = default;
IWriter::~IWriter() = default;
IMMU::~IMMU() = default;
IExecutor::~IExecutor() = default;
ILinker::~ILinker() = default;

void IExecutor::AttachProcessor (IProcessor* proc)
{
	Processor::IModuleBase::AttachProcessor (proc);
	proc_ ->AttachExecutor (this);
}

void IExecutor::DetachProcessor()
{
	proc_ ->AttachExecutor (0);
	Processor::IModuleBase::DetachProcessor();
}

void ILinker::AttachProcessor (IProcessor* proc)
{
	Processor::IModuleBase::AttachProcessor (proc);
	proc_ ->AttachLinker (this);
}

void ILinker::DetachProcessor()
{
	proc_ ->AttachLinker (0);
	Processor::IModuleBase::DetachProcessor();
}

void IMMU::AttachProcessor (IProcessor* proc)
{
	Processor::IModuleBase::AttachProcessor (proc);
	proc_ ->AttachMMU (this);
}

void IMMU::DetachProcessor()
{
	proc_ ->AttachMMU (0);
	Processor::IModuleBase::DetachProcessor();
}

void IReader::AttachProcessor (IProcessor* proc)
{
	Processor::IModuleBase::AttachProcessor (proc);
	proc_ ->AttachReader (this);
}

void IReader::DetachProcessor()
{
	proc_ ->AttachReader (0);
	Processor::IModuleBase::DetachProcessor();
}

void IWriter::AttachProcessor (IProcessor* proc)
{
	Processor::IModuleBase::AttachProcessor (proc);
	proc_ ->AttachWriter (this);
}

void IWriter::DetachProcessor()
{
	proc_ ->AttachWriter (0);
	Processor::IModuleBase::DetachProcessor();
}


void IModuleBase::AttachProcessor (IProcessor* proc)
{
	__sassert (proc, "Attaching to NULL processor");
	proc_ = proc;
}

void IModuleBase::DetachProcessor()
{
	proc_ = 0;
}

} // namespace Processor

ImplementDescriptor(IReader, "reader module", MOD_APPMODULE, Processor::IReader);
ImplementDescriptor(IWriter, "writer module", MOD_APPMODULE, Processor::IWriter);
ImplementDescriptor(ILinker, "linker module", MOD_APPMODULE, Processor::ILinker);
ImplementDescriptor(IMMU, "memory management unit", MOD_APPMODULE, Processor::IMMU);
ImplementDescriptor(IExecutor, "executor module", MOD_APPMODULE, Processor::IExecutor);
ImplementDescriptor(IProcessor, "processor logic", MOD_APPMODULE, Processor::IProcessor);
