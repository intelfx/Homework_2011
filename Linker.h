#ifndef _LINKER_H
#define _LINKER_H

// -----------------------------------------------------------------------------
// Library		Homework
// File			Linker.h
// Author		intelfx
// Description	Linker.
// -----------------------------------------------------------------------------

#include "build.h"
#include "Interfaces.h"

namespace ProcessorImplementation
{
	using namespace Processor;

	class UATLinker: public ILinker
	{
		symbol_map temporary_map;

		const char* Dbg_AddrType (AddrType atype);

	protected:
//		virtual bool Verify_() const; // nothing to verify yet.. internal map is verified in Finalize()

	public:
		virtual void InitLinkSession();
		virtual void Finalize();

		virtual void LinkSymbols (DecodedSet& input);

		virtual Reference::Direct& Resolve (Reference& reference);
	};
}

#endif // _LINKER_H