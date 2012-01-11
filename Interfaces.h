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

DeclareDescriptor (IModuleBase);
DeclareDescriptor (ICommandSet);
DeclareDescriptor (IReader);
DeclareDescriptor (IWriter);
DeclareDescriptor (IMMU);
DeclareDescriptor (IExecutor);
DeclareDescriptor (ILinker);
DeclareDescriptor (ProcessorAPI);
DeclareDescriptor (IBackend);
DeclareDescriptor (IInternalLogic);

namespace Processor
{

	class ProcessorAPI : LogBase (ProcessorAPI)
	{
		IReader* reader_;
		IWriter* writer_;
		IMMU* mmu_;
		StaticAllocator<IExecutor*, Value::V_MAX + 1> executors_;
		IBackend* backend_;
		ILinker* linker_;
		ICommandSet* cset_;
		ILogic* internal_logic_;
		bool initialise_completed;

		void Attach_ (IReader* reader);
		void Attach_ (IWriter* writer);
		void Attach_ (IMMU* mmu);
		void Attach_ (IExecutor* executor);
		void Attach_ (ILinker* linker);
		void Attach_ (ICommandSet* cset);
		void Attach_ (IBackend* backend);
		void Attach_ (ILogic* logic);

		void ExecuteCommand (Command& command);

	protected:
		virtual bool _Verify() const;

	public:
		virtual ~ProcessorAPI();
		ProcessorAPI();

		void Attach (IModuleBase* module);
		void Detach (const IModuleBase* module);
		void Initialise();

		IReader*	Reader() { verify_method; return reader_; }
		IWriter*	Writer() { verify_method; return writer_; }
		IMMU*		MMU() { verify_method; return mmu_; }
		IBackend*	Backend() { verify_method; return backend_; }
		IExecutor*	Executor (Value::Type type) { verify_method; return executors_[type]; }
		ILinker*	Linker() { verify_method; return linker_; }
		ICommandSet*CommandSet() { verify_method; return cset_; }
		ILogic*		LogicProvider() { verify_method; return internal_logic_; }


		void	Flush(); // Completely reset and reinitialise the system
		void	Reset(); // Reset current execution context
		void	Clear(); // Clear current execution buffers
		void	Load (FILE* file, bool execute_stream = 0); // Load into a new context/buffer
		void	Dump (FILE* file); // Dump current context/buffer
		void	Delete(); // Return to previous context/buffer
		void	Compile(); // Invoke backend to compile the bytecode
		calc_t	Exec(); // Execute current system state whatever it is now
	};

	class IModuleBase : LogBase (IModuleBase)
	{
	protected:
		ProcessorAPI* proc_;
		virtual ~IModuleBase();

		virtual void OnAttach() {}
		virtual void OnDetach() {}

	public:
		virtual ProcessorAPI* GetProcessor()
		{
			return proc_;
		}

		void SetProcessor (ProcessorAPI* proc)
		{
			__sassert (proc, "Attaching to NULL processor");
			proc_ = proc;
			OnAttach();
		}

		void DetachSelf()
		{
			if (proc_)
			{
				proc_ ->Detach (this);
				OnDetach();
				proc_ = 0;
			}
		}
	};

	class ILogic : LogBase (IInternalLogic), public IModuleBase
	{
	public:
		virtual ~ILogic();

		virtual Register	DecodeRegister (const char* reg) = 0;
		virtual const char*	EncodeRegister (Register reg) = 0;

		virtual size_t	ChecksumState() = 0; // Returns a "check sum" of context/buffer and main logic module IDs

		virtual void	Syscall (size_t index) = 0; // Execute the processor syscall
		virtual void	Analyze (calc_t value) = 0; // Analyze an arbitrary value

		virtual void	Jump (Reference& ref) = 0; // Use CODE reference to jump
		virtual calc_t	Read (Reference& ref) = 0; // Use DATA reference to read
		virtual void	Write (Reference& ref, calc_t value) = 0; // Use DATA reference to write

		virtual calc_t	StackTop() = 0; // Calculation stack "top" operation
		virtual calc_t	StackPop() = 0; // Calculation stack "pop" operation
		virtual void	StackPush (calc_t value) = 0; // Calculation stack "push" operation
	};

	class ICommandSet : LogBase (ICommandSet), public IModuleBase
	{
		static IExecutor* default_exec_;

	public:
		virtual ~ICommandSet();

		static void SetFallbackExecutor (IExecutor* exec);
		abiret_t ExecFallbackCallback (Processor::Command* cmd); // look, what a great name!


		// Remove all registered handlers and user commands.
		virtual void ResetCommandSet() = 0;

		// Dynamically registers a command. id field is non-significant; a random id is assigned.
		virtual void AddCommand (CommandTraits&& command) = 0;

		// Dynamically registers an implementation for a specific command.
		// In case of not found command, exception is thrown.
		virtual void AddCommandImplementation (const char* mnemonic, size_t module, void* handle) = 0;

		virtual const CommandTraits& DecodeCommand (const char* mnemonic) const = 0;
		virtual const CommandTraits& DecodeCommand (cid_t id) const = 0;

		// Returns handle for given command and implementation.
		// WARNING can return 0 with meaning "no registered handler".
		virtual void* GetExecutionHandle (cid_t id, size_t module) = 0;
	};

	// many would want to do a shared R/W handler to share some routines
	class IReader : LogBase (IReader), virtual public IModuleBase
	{
	public:
		virtual ~IReader();

		// Sections in list are in file appearance order.
		virtual FileProperties RdSetup (FILE* file) = 0;
		virtual void RdReset (FileProperties* prop) = 0;

		// Advance file to the next section and return information about the new section.
		// Successive calls to read functions should then read from this section.
		// Returns if the section switch has been performed (0 in case of last section).
		virtual size_t NextSection (FileProperties* prop,
									FileSectionType* type, size_t* count, size_t* bytes) = 0;

		// Reads next element from current section into "destination", writing no more
		// than "max" bytes.
		// Returns count of successfully read elements (0 or 1).
		virtual size_t ReadNextElement (FileProperties* prop, void* destination, size_t max) = 0;

		// Reads current section image (that is, which is described by section list front element)
		// into "destination", writing no more than "max" bytes.
		// Returns count of successfully read elements.
		virtual size_t ReadSectionImage (FileProperties* prop, void* destination, size_t max) = 0;

		// Reads and decodes next element of non-uniform stream section.
		// Returns count of successfully read stream units (0 or 1).
		virtual size_t ReadStream (FileProperties* prop, DecodeResult* destination) = 0;
	};

	// many would want to do a shared R/W handler to share some routines
	class IWriter : LogBase (IWriter), virtual public IModuleBase
	{
	public:
		virtual ~IWriter();

		virtual void WrSetup (FILE* file) = 0;
		virtual void WrReset() = 0;

		virtual void Write (size_t ctx_id) = 0;
	};

	class IMMU : LogBase (IMMU), public IModuleBase
	{
	public:
		virtual ~IMMU();

		virtual Context&		GetContext	() = 0; // Get current context data
		virtual void			DumpContext	() const = 0; // Print context data to the log

		virtual void			SelectStack	(Value::Type type); // In implementations with multiple stacks, selects stack for consequent operations.

		virtual calc_t&			AStackFrame	(int offset) = 0; // Access calculation stack relative to context's stack frame pointer
		virtual calc_t&			AStackTop	(size_t offset) = 0; // Access calculation stack relative to its top
		virtual calc_t&			ARegister	(Register reg_id) = 0; // Access register
		virtual Command&		ACommand	(size_t ip) = 0; // Access CODE section
		Command&				ACommand	() { return ACommand (GetContext().ip); }
		virtual calc_t&			AData		(size_t addr) = 0;   // Access DATA section
		virtual symbol_type&	ASymbol		(size_t hash) = 0;   // Access symbol buffer

		virtual void			ReadStack	(calc_t* image, size_t size, bool selected_only) = 0; // Read stack(s) data (closer to image end is closer to top).
		virtual void			ReadData	(calc_t* image, size_t size) = 0; // Read data buffer
		virtual void			ReadText	(Command* image, size_t size) = 0; // Read code buffer
		virtual void			ReadSyms	(void* image, size_t size) = 0; // read symbol buffer (deserialize)

		virtual void			InsertData	(calc_t value) = 0; // add a data entry to the buffer end
		virtual void			InsertText	(const Command& command) = 0; // add a code entry to the buffer end
		virtual void			InsertSyms	(symbol_map&& syms) = 0; // Read prepared symbol buffer

		virtual void			WriteStack	(calc_t* image, bool selected_only) const = 0; // write stack(s) data (closer to image end is closer to top).
		virtual void			WriteData	(calc_t* image) const = 0; // write data buffer
		virtual void			WriteText	(Command* image) const = 0; // write code buffer
		virtual void			WriteSyms	(void** image, size_t* bytes, size_t* count) const = 0; // serialize symbol map

		virtual size_t			GetTextSize	() const = 0;
		virtual size_t			GetDataSize	() const = 0;
		virtual size_t			GetStackTop () const = 0;

		virtual void			AlterStackTop (short offset) = 0; // Change stack top relatively (-1 is pop).

		virtual void VerifyReference (const Reference::Direct& ref) const = 0;

		virtual void ResetBuffers (size_t ctx_id) = 0; // Reset specified context buffer
		virtual void ResetEverything() = 0; // Reset MMU to its initial state, clearing all stacks.

		virtual void SaveContext() = 0; // Push context on call stack
		virtual void ClearContext() = 0; // Clear all context data but context ID
		virtual void RestoreContext() = 0; // Load context from call stack

		virtual void NextContextBuffer() = 0; // Push context on call stack; clear all and increment context ID
		virtual void AllocContextBuffer() = 0; // Switch to next context buffer; reset the buffer

        void SetTemporaryContext (size_t ctx_id);
	};

	class IExecutor : LogBase (IExecutor), public IModuleBase
	{
	public:
		virtual ~IExecutor();

		virtual size_t ID() { return typeid (*this).hash_code(); }

		// Returns instruction type supported by the executor module
		virtual Value::Type SupportedType() const = 0;

		// Re-registers all supported implementations.
		virtual void ResetImplementations() = 0;

		// Executes a single command.
		virtual void Execute (void* handle, Command::Argument& argument) = 0;
	};

	class IBackend : LogBase (IBackend), public IModuleBase
	{
	public:
		virtual ~IBackend();

		virtual void		CompileBuffer (size_t chk) = 0;
		virtual bool		ImageIsOK (size_t chk) = 0;
		virtual abiret_t	ExecuteImage (size_t chk) = 0;
	};

	class ILinker : LogBase (ILinker), public IModuleBase
	{
	public:
		virtual ~ILinker();

		static const size_t symbol_auto_placement_addr = static_cast<size_t> (-1);

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
		virtual void LinkSymbols (DecodeResult& input) = 0;

		// Retrieve a direct reference for given arbitary reference.
		virtual Reference::Direct Resolve (Reference& reference) = 0; // or get an unresolved symbol error
	};

}

#endif // _INTERFACES_H
// kate: indent-mode cstyle; replace-tabs off; indent-width 4; tab-width 4;
