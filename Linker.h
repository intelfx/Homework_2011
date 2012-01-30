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

	protected:
//		virtual bool _Verify() const; // nothing to verify yet.. internal map is verified in Finalize()

	public:
		UATLinker();
		virtual ~UATLinker();

		virtual void InitLinkSession();
		virtual void Finalize();

		virtual void LinkSymbols (DecodeResult& input);

		Reference::Direct Resolve (Reference& reference);
	};
}

#endif // _LINKER_H