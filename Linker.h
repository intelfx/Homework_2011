#ifndef _LINKER_H
#define _LINKER_H

// -----------------------------------------------------------------------------
// Library		Homework
// File			Linker.h
// Author		intelfx
// Description	Unit-At-a-Time linker implementation
// -----------------------------------------------------------------------------

#include "build.h"
#include "Interfaces.h"

namespace ProcessorImplementation
{
using namespace Processor;

class INTERPRETER_API UATLinker: public ILinker
{
	symbol_map temporary_map;

public:
	virtual void DirectLink_Init();
	virtual void DirectLink_Add (symbol_map& symbols, size_t offsets[SEC_MAX]);
	virtual void DirectLink_Commit();

	DirectReference Resolve (Reference& reference);
};
}

#endif // _LINKER_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
