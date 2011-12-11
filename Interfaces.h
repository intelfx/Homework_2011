#ifndef _INTERFACES_H
#define _INTERFACES_H

// -----------------------------------------------------------------------------
// Library		Homework
// File			Interfaces.h
// Author		intelfx
// Description	Interface declarations.
// -----------------------------------------------------------------------------

#include "build.h"
#include "Utility.h"

DeclareDescriptor (IReader);
DeclareDescriptor (IWriter);
DeclareDescriptor (IMMU);
DeclareDescriptor (IExecutor);
DeclareDescriptor (ILinker);
DeclareDescriptor (IProcessor);

namespace Processor
{

class IProcessor : LogBase (IProcessor)
{
	IReader* reader_;
	IWriter* writer_;
	IMMU* mmu_;
	IExecutor* executor_;
	ILinker* linker_;

protected:
	virtual ~IProcessor();

public:
	virtual void AttachReader (IReader* reader) = 0;
	virtual void AttachWriter (IWriter* writer) = 0;
	virtual void AttachMMU (IMMU* mmu) = 0;
	virtual void AttachExecutor (IExecutor* executor) = 0;
	virtual void AttachLinker (ILinker* linker) = 0;

	IReader* Reader() { return reader_; }
	IWriter* Writer() { return writer_; }
	IMMU* MMU() { return mmu_; }
	IExecutor* Executor() { return executor_; }
	ILinker* Linker() { return linker_; }

	virtual void DumpContext (size_t ctx) = 0;

	virtual Register DecodeRegister (const char* reg) = 0;
	virtual const char* EncodeRegister (Register reg) = 0;

	virtual void	Jump (size_t ip) = 0; // Use CODE reference to jump
	virtual void	Analyze (calc_t value) = 0; // Analyze an arbitrary value

	virtual calc_t	Read (Reference& ref) = 0; // Use DATA reference to read
	virtual void	Write (Reference& ref, calc_t value) = 0; // Use DATA reference to write

	virtual calc_t	StackTop() = 0; // Calculation stack "top" operation
	virtual calc_t	StackPop() = 0; // Calculation stack "pop" operation
	virtual void	StackPush (calc_t value) = 0; // Calculation stack "push" operation

	virtual void	Syscall (size_t index) = 0; // Execute the processor syscall
};

class IModuleBase
{
protected:
	IProcessor* proc_;

public:
	virtual void AttachProcessor (IProcessor* proc);
	virtual void DetachProcessor();
};

class IReader : LogBase (IReader), public IModuleBase
{
protected:
	virtual ~IReader();

public:
	virtual void AttachProcessor (IProcessor* proc);
	virtual void DetachProcessor();

	virtual void Setup (FILE* file) = 0;
	virtual void Reset() = 0;
	virtual DecodedSet Read (size_t ctx_id) = 0;
};

class IWriter : LogBase (IWriter), public IModuleBase
{
protected:
	virtual ~IWriter();

public:
	virtual void AttachProcessor (IProcessor* proc);
	virtual void DetachProcessor();

	virtual void Setup (FILE* file) = 0;
	virtual void Reset() = 0;
	virtual void Write (size_t ctx_id) = 0;
};

class IMMU : LogBase (IMMU), public IModuleBase
{
protected:
	virtual ~IMMU();

public:
	virtual void AttachProcessor (IProcessor* proc);
	virtual void DetachProcessor();

	virtual Context&		GetContext	() = 0; // Get current context data

	virtual calc_t&			AStackFrame	(int offset) = 0; // Access calculation stack relative to context's stack frame pointer
	virtual calc_t&			AStackTop	(size_t offset) = 0; // Access calculation stack relative to its top
	virtual calc_t&			ARegister	(Register reg_id) = 0; // Access register
	virtual DecodedCommand&	ACommand	(size_t ip) = 0; // Access CODE section
	DecodedCommand&			ACommand	() { return ACommand (GetContext().ip); }
	virtual calc_t&			AData		(size_t addr) = 0;  // Access DATA section
	virtual symbol_type&	ASymbol		(size_t hash) = 0;  // Access symbol buffer

	virtual void			ReadStack	(calc_t* image, size_t size) = 0; // Read stack data (image end is top)
	virtual void			ReadData	(calc_t* image, size_t size) = 0; // Read data buffer
	virtual void			ReadText	(DecodedCommand* image, size_t size) = 0; // Read code buffer
	virtual void			ReadSyms	(symbol_map* image) = 0; // Read symbol buffer

	virtual size_t			GetTextSize	() = 0;
	virtual size_t			GetDataSize	() = 0;
	virtual size_t			GetStackTop () = 0;

	virtual void ResetBuffers (size_t ctx_id) = 0; // Reset specified context buffer
	virtual void ResetEverything() = 0; // Reset MMU to its initial state, clearing all stacks.

	virtual void SaveContext() = 0; // Push context on call stack
	virtual void ClearContext() = 0; // Clear all context data but context ID
	virtual void RestoreContext() = 0; // Load context from call stack

	virtual void NextContextBuffer() = 0; // Push context on call stack; clear all and increment context ID
	virtual void AllocContextBuffer() = 0; // Switch to next context buffer; reset the buffer

};

class IExecutor : LogBase (IExecutor), public IModuleBase
{
protected:
	virtual ~IExecutor();

public:
	virtual void AttachProcessor (IProcessor* proc);
	virtual void DetachProcessor();

	virtual void Execute (void* handle, DecodedCommand::Argument& argument) = 0;
};

class ILinker : LogBase (ILinker), public IModuleBase
{
protected:
	virtual ~ILinker();

public:
	static const size_t symbol_auto_placement_addr = static_cast<size_t> (-1);

	virtual void AttachProcessor (IProcessor* proc);
	virtual void DetachProcessor();

	// Clear internal buffer symbol tables.
	virtual void InitLinkSession() = 0;

	// Commit internal buffers to the MMU.
	// Function expects MMU to be set up to desired context and everything else except symbols
	// already committed and interpreted by MMU.
	virtual void Finalize() = 0;

	// Link symbols created by partial decoding.
	// Function expects MMU to be set up to desired context
	// and last instruction not yet stored onto the TEXT top.
	// Function expects symbols to be auto-addressed (labels) to have address equal to
	// "symbol_auto_placement_addr".
	// If there is a full symbol image (read from binary file), push it to MMU directly.
	virtual void LinkSymbols (DecodedSet& input) = 0;

	// Retrieve a direct reference for given arbitary reference.
	virtual Reference::Direct& Resolve (Reference& reference) = 0; // or get an unresolved symbol error
};

}

#endif // _INTERFACES_H
// kate: indent-mode cstyle; replace-tabs off; tab-width 4;
