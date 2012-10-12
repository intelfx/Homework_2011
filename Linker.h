#ifndef INTERPRETER_LINKER_H
#define INTERPRETER_LINKER_H

#include "build.h"

#include "Interfaces.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			Linker.h
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Default linker plugin implementation.
// -------------------------------------------------------------------------------------

namespace ProcessorImplementation
{
using namespace Processor;

typedef std::multimap<size_t, std::pair<std::string, Symbol> > symbol_tmap;

class INTERPRETER_API UATLinker: public ILinker
{
	symbol_tmap temporary_map;

	void RelocateReference( Reference& ref, size_t offsets[SEC_MAX] );

public:
	virtual void DirectLink_Init();
	virtual void DirectLink_Add( symbol_map& symbols, size_t offsets[SEC_MAX] );
	virtual void DirectLink_Commit( bool UAT );

	virtual void Relocate( size_t offsets[SEC_MAX] );

	DirectReference Resolve( Reference& reference );
};

} // namespace ProcessorImplementation

#endif // INTERPRETER_LINKER_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
