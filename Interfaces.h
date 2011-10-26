#ifndef _INTERFACES_H
#define _INTERFACES_H

// -----------------------------------------------------------------------------
// Library		Antided
// File			Interfaces.h
// Author		intelfx
// Description	Interface declarations.
// -----------------------------------------------------------------------------

#include "build.h"
#include "Utility.h"

namespace OLD_Processor
{

DeclareDescriptor (IReader);
DeclareDescriptor (IWriter);
DeclareDescriptor (IMMU);
DeclareDescriptor (IExecutor);
DeclareDescriptor (ILinker);
DeclareDescriptor (IProcessor);

class IProcessor
{
	IReader* reader_;
	IWriter* writer_;
	IMMU* mmu_;
	IExecutor executor_;
	ILinker* linker_;

public:
	virtual void AttachReader (IReader* reader) = 0;
	virtual void AttachWriter (IWriter* writer) = 0;
	virtual void AttachMMU (IMMU* mmu) = 0;
	virtual void AttachExecutor (IExecutor* executor) = 0;
	virtual void AttachLinker (ILinker* linker) = 0;

	virtual void Jump (size_t ip) = 0;
	virtual void Analyze (calc_t value) = 0;

	virtual void DumpContext (size_t ctx) = 0;
};

class IModuleBase
{
	IProcessor* proc_;

public:
	virtual void AttachProcessor (IProcessor* proc);
	virtual void DetachProcessor();

	virtual Register DecodeRegister (const char* reg) = 0;
	virtual const char* EncodeRegister (Register reg) = 0;
};

class IReader : LogBase (IReader), public IModuleBase
{

public:
	virtual void AttachProcessor (IProcessor* proc);
	virtual void DetachProcessor();

	virtual void Setup (FILE* file) = 0;
	virtual void Reset() = 0;
	virtual DecodedSet Read (size_t ctx_id) = 0;
};

class IWriter : LogBase (IWriter), public IModuleBase
{

public:
	virtual void AttachProcessor (IProcessor* proc);
	virtual void DetachProcessor();

	virtual void Setup (FILE* file) = 0;
	virtual void Reset() = 0;
	virtual void Write (size_t ctx_id) = 0;
};

class IMMU : LogBase (IMMU), public IModuleBase
{

public:
	virtual void AttachProcessor (IProcessor* proc);
	virtual void DetachProcessor();

	virtual calc_t&			AStackFrame	(int offset) = 0; // Access calculation stack relative to context's stack frame pointer
	virtual calc_t&			AStackTop	(int offset) = 0; // Access calculation stack relative to its top
	virtual calc_t&			ARegister	(Register reg_id) = 0; // Access register
	virtual DecodedCommand&	ACommand	(size_t ip) = 0; // Access CODE section
	virtual calc_t&			AData		(size_t addr) = 0; // Access DATA section
	virtual symbol_type&	ASymbol		(size_t hash) = 0; // Access symbol buffer

	virtual void			StackPush (calc_t value) = 0; // Calculation stack "push" operation
	virtual calc_t			StackPop() = 0; // Calculation stack "pop" operation

	virtual void			ReadStack	(calc_t* image, size_t size); // Read stack data (image end is top)
	virtual void			ReadData	(calc_t* image, size_t size); // Read data buffer
	virtual void			ReadText	(DecodedCommand* image, size_t size); // Read code buffer
	virtual void			ReadSyms	(symbol_map* image); // Read symbol buffer

	virtual size_t			GetTextTop	();
	virtual size_t			GetDataTop	();

	virtual ProcessorState& Context() = 0; // Get current context data

	virtual void ResetBuffers (size_t ctx_id) = 0; // Reset specified context buffer
	virtual void ResetEverything() = 0; // Reset MMU to its initial state, clearing all stacks.

	virtual void SaveContext() = 0; // Push context on call stack
	virtual void NewContext() = 0; // Clear all context data but context ID
	virtual void RestoreContext() = 0; // Load context from call stack

	virtual void NextContextBuffer() = 0; // Push context on call stack; clear all and increment context ID
	virtual void AllocContextBuffer() = 0; // Switch to next context buffer; reset the buffer
};

class IExecutor : LogBase (IExecutor), public IModuleBase
{

public:
	virtual void AttachProcessor (IProcessor* proc);
	virtual void DetachProcessor();

	virtual void Execute (void* handle, DecodedCommand::Argument& argument) = 0;
};

class ILinker : LogBase (ILinker), public IModuleBase
{

public:
	virtual void AttachProcessor (IProcessor* proc);
	virtual void DetachProcessor();

	virtual void InitLinkSession() = 0; // Clear internal buffer symbol tables
	virtual void Finalize() = 0; // Commit internal buffers to the MMU

	// Link symbols created by partial decoding.
	// If there is a full symbol image (read from binary file), call MMU directly.
	virtual void LinkSymbols (DecodedSet& input) = 0;

	// Retrieve a direct reference for given arbitary reference.
	virtual Reference::Direct& Resolve (Reference& reference) = 0; // or get an unresolved symbol error
};

}

#endif // _INTERFACES_H
// kate: indent-mode cstyle; replace-tabs off; tab-width 4;
