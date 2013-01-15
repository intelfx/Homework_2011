#ifndef INTERPRETER_INTERFACES_H
#define INTERPRETER_INTERFACES_H

#include "build.h"

#include "Utility.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			Interfaces.h
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Plugin interface declarations.
// -------------------------------------------------------------------------------------

DeclareDescriptor( IModuleBase, INTERPRETER_API, INTERPRETER_TE );
DeclareDescriptor( ICommandSet, INTERPRETER_API, INTERPRETER_TE );
DeclareDescriptor( IReader, INTERPRETER_API, INTERPRETER_TE );
DeclareDescriptor( IWriter, INTERPRETER_API, INTERPRETER_TE );
DeclareDescriptor( IMMU, INTERPRETER_API, INTERPRETER_TE );
DeclareDescriptor( IExecutor, INTERPRETER_API, INTERPRETER_TE );
DeclareDescriptor( ILinker, INTERPRETER_API, INTERPRETER_TE );
DeclareDescriptor( ProcessorAPI, INTERPRETER_API, INTERPRETER_TE );
DeclareDescriptor( IBackend, INTERPRETER_API, INTERPRETER_TE );
DeclareDescriptor( ILogic, INTERPRETER_API, INTERPRETER_TE );

namespace Processor
{

class INTERPRETER_API ProcessorAPI : LogBase( ProcessorAPI )
{
	IReader*		reader_;
	IWriter*		writer_;
	IMMU*			mmu_;
	IExecutor*		executors_[Value::V_MAX + 1];
	IBackend*		backend_;
	ILinker*		linker_;
	ICommandSet*	cset_;
	ILogic*			internal_logic_;

	/*
	 * Shadow fields are used on module detach.
	 *
	 * Since detach function is being called from IModuleBase destructor, at which
	 * time the module itself is already destroyed, and so is vptr,
	 * comparison of pointer to the destroyed module with pointer to IModuleBase
	 * will NOT succeed.
	 * So we keep two pointers to each module:
	 * - normal (for fast access without dynamic_cast);
	 * - base (for comparison at detach time).
	 */
	IModuleBase*	shadow_reader_;
	IModuleBase*	shadow_writer_;
	IModuleBase*	shadow_mmu_;
	IModuleBase*	shadow_executors_[Value::V_MAX + 1];
	IModuleBase*	shadow_backend_;
	IModuleBase*	shadow_linker_;
	IModuleBase*	shadow_cset_;
	IModuleBase*	shadow_logic_;

	bool initialise_completed;

	NativeExecutionManager nem_;

	void Attach_( IReader* reader );
	void Attach_( IWriter* writer );
	void Attach_( IMMU* mmu );
	void Attach_( IExecutor* executor );
	void Attach_( ILinker* linker );
	void Attach_( ICommandSet* cset );
	void Attach_( IBackend* backend );
	void Attach_( ILogic* logic );

	Context current_execution_context_;

protected:
	virtual bool _Verify() const;

	template <typename T>
	T* CheckReturnModule( T* module, const char* modname ) {
		if( initialise_completed ) {
			verify_method;
			return module;
		} else {
			cassert( module, "Module \"%s\" is not available on pre-init state. Reorder module initialisation.", modname );

			// TODO in case of assertion fail, we will have (initialise_completed == false) and verification error.
			return module;
		}
	}

public:
	ProcessorAPI();

	void Attach( IModuleBase* module );
	void Detach( const IModuleBase* module );
	void Initialise( bool value = 1 );

	IReader*	Reader()					{ return CheckReturnModule( reader_, "reader" ); }
	IWriter*	Writer()					{ return CheckReturnModule( writer_, "writer" ); }
	IMMU*		MMU()						{ return CheckReturnModule( mmu_, "MMU" ); }
	IBackend*	Backend()					{ return CheckReturnModule( backend_, "backend" ); }
	IExecutor*	Executor( Value::Type type ){ return CheckReturnModule( executors_[type], "executor" ); }
	ILinker*	Linker()					{ return CheckReturnModule( linker_, "linker" ); }
	ICommandSet* CommandSet()				{ return CheckReturnModule( cset_, "command set" ); }
	ILogic*		LogicProvider()				{ return CheckReturnModule( internal_logic_, "logic provider" ); }

	      Context& CurrentContext()       { return current_execution_context_; }
	const Context& CurrentContext() const { return current_execution_context_; }

	NativeExecutionManager& ExecutionManager() { return nem_; }

	void	Flush(); // Completely reset and reinitialise the system
	void	Reset(); // Reset current execution context
	void	Clear(); // Clear current execution buffers (implies Reset())
	ctx_t	Load( FILE* file ); // Load into a new context/buffer
	void	Dump( ctx_t id, FILE* file ); // Dump current context/buffer
	ctx_t	MergeContexts( const std::vector<ctx_t>& contexts ); // Merge all contexts into a new one
	void	SelectContext( ctx_t id ); // Switch to a context
	void	DeleteContext( ctx_t id ); // Delete (deallocate) the given context
	void	DeleteCurrentContext(); // Return to previous context buffer, exiting from all frames in the current one
	void	Compile(); // Invoke backend to compile the bytecode
	calc_t	Exec(); // Execute current system state whatever it is now

	void DumpExecutionContext( std::string* ctx_dump );
};

class INTERPRETER_API IModuleBase : LogBase( IModuleBase )
{
protected:
	ProcessorAPI* proc_;

	IModuleBase() : proc_( nullptr ) {}
	virtual ~IModuleBase();

	virtual void OnAttach() {}
	virtual void OnDetach() {}

public:
	ProcessorAPI* GetProcessor() {
		return proc_;
	}

	void SetProcessor( ProcessorAPI* proc ) {
		s_cassert( proc, "Attaching to NULL processor" );
		proc_ = proc;
		OnAttach();
	}

	void DetachSelf() {
		if( proc_ ) {
			proc_->Detach( this );
			OnDetach();
			proc_ = nullptr;
		}
	}

};

class INTERPRETER_API ILogic : LogBase( ILogic ), public IModuleBase
{
public:
	virtual Register	DecodeRegister( const char* reg ) = 0;
	virtual const char*	EncodeRegister( Register reg ) = 0;

	virtual size_t	ChecksumState() = 0; // Returns a "check sum" of context/buffer and main logic module IDs

	virtual void	Syscall( size_t index ) = 0; // Execute the processor syscall
	virtual void	Analyze( calc_t value ) = 0; // Analyze an arbitrary value

	virtual void	Jump( const DirectReference& ref ) = 0; // Use CODE reference to jump
	virtual calc_t	Read( const DirectReference& ref ) = 0; // Use DATA reference to read
	virtual void	Write( const DirectReference& ref, calc_t value ) = 0; // Use DATA reference to write
	virtual void	UpdateType( const DirectReference& ref, Value::Type type ) = 0; // Use DATA reference to rewrite its type

	// Stack management commands operate on the stack corresponding to the last command executed
	// and are designed for usage by the execution unit modules (or external inspection code).
	virtual size_t	StackSize() = 0; // Calculation stack "count of elements" operation
	virtual calc_t	StackTop() = 0; // Calculation stack "top" operation
	virtual calc_t	StackPop() = 0; // Calculation stack "pop" operation
	virtual void	StackPush( calc_t value ) = 0; // Calculation stack "push" operation

	virtual void    ResetCurrentContextState() = 0; // Resets the current execution context state
	virtual void    SetCurrentContextBuffer( ctx_t ctx ) = 0; // Changes the current context buffer.
	virtual void    SaveCurrentContext() = 0; // Saves the current context (state and buffer) onto the call stack
	virtual void    RestoreCurrentContext() = 0; // Restores the current context from the call stack
	virtual void    ClearContextStack() = 0; // Clears the call stack

	virtual void	ExecuteSingleCommand( Command& command ) = 0; // Execute a single command
	virtual std::string	DumpCommand( Command& command ) const = 0; // Decode and log a single command

	void SwitchToContextBuffer( ctx_t id, bool clear_on_switch = false ); // Switch to a different context buffer, remembering last context.
	                                                                      // You shall probably use _this_ function to change contexts.
};

class INTERPRETER_API ICommandSet : LogBase( ICommandSet ), public IModuleBase
{
public:
	// Remove all registered handlers and user commands.
	virtual void ResetCommandSet() = 0;

	// Dynamically registers a command. id field is non-significant; a random id is assigned.
	virtual void AddCommand( CommandTraits && command ) = 0;

	// Dynamically registers an implementation for a specific command.
	// In case of not found command, exception is thrown.
	virtual void AddCommandImplementation( const char* mnemonic, size_t module, void* handle ) = 0;

	// Decode given command by mnemonic or opcode.
	// WARNING can return 0 with meaning "invalid/unregistered command".
	virtual const CommandTraits* DecodeCommand( const char* mnemonic ) const = 0;
	virtual const CommandTraits* DecodeCommand( cid_t id ) const = 0;

	// Returns handle for given command and implementation.
	virtual void* GetExecutionHandle( const CommandTraits& cmd, size_t module ) = 0;
};

// many would want to do a shared R/W handler to share some routines
class INTERPRETER_API IReader : LogBase( IReader ), virtual public IModuleBase
{
public:
	virtual FileType RdSetup( FILE* file ) = 0;
	virtual void RdReset() = 0;

	// Advance file to the next section and return information about the new section.
	// Successive calls to read functions should then read from this section.
	// Returns: pair<section type, section size>
	virtual std::pair<MemorySectionIdentifier, size_t> NextSection() = 0;

	// Reads current section image into "destination".
	virtual llarray ReadSectionImage() = 0;

	// Reads current section symbol map into "destination".
	virtual symbol_map ReadSymbols() = 0;

	// Reads and decodes next element of uniform stream section.
	// Returns pointer to decoded unit (if any).
	virtual DecodeResult* ReadStream() = 0;
};

// many would want to do a shared R/W handler to share some routines
class INTERPRETER_API IWriter : LogBase( IWriter ), virtual public IModuleBase
{
public:
	virtual void WrSetup( FILE* file ) = 0;
	virtual void WrReset() = 0;

	virtual void Write( ctx_t ctx_id ) = 0;
};

class INTERPRETER_API IMMU : LogBase( IMMU ), public IModuleBase
{
public:
	virtual void			DumpContext( std::string* regs,
	                                     std::string* stacks ) const = 0; // Print context data to the string

	virtual ctx_t			CurrentContextBuffer() const = 0; // Get the current context buffer
	virtual ctx_t			AllocateContextBuffer() = 0; // Allocate a new context buffer
	virtual void			SelectContextBuffer( ctx_t id ) = 0; // Select a different context buffer. NOTE: Do not use this function, see ILogic.
	virtual void			ReleaseContextBuffer( ctx_t id ) = 0; // Release a context buffer; deselect if selected
	virtual void			ResetContextBuffer( ctx_t id ) = 0; // Clear a context buffer (was ResetBuffers())

	virtual size_t			QueryStackTop( Value::Type type ) const = 0; // Get absolute value of a stack's top (0 means stack is empty)
	virtual void            SetStackTop( Value::Type type, ssize_t adjust ) = 0; // Set a stack top relative to its current value

	virtual calc_t&			AStackFrame( Value::Type type, ssize_t offset ) = 0; // Access calculation stack relative to context's stack frame pointer
	virtual calc_t&			AStackTop( Value::Type type, size_t offset ) = 0; // Access calculation stack relative to its top
	virtual calc_t&			ARegister( Register reg_id ) = 0; // Access register
	virtual Command&		ACommand( size_t ip ) = 0; // Access CODE section
	virtual calc_t&			AData( size_t addr ) = 0; // Access DATA section
	virtual symbol_type&	ASymbol( size_t hash ) = 0; // Access symbol buffer
	virtual char*			ABytepool( size_t offset ) = 0; // Access byte pool

	virtual Offsets			QuerySectionLimits() const = 0; // Query current section sizes (limits)

	virtual llarray			DumpSection( MemorySectionIdentifier section, size_t address,
	                                     size_t count ) = 0;
	virtual void			ModifySection( MemorySectionIdentifier section, size_t address,
	                                       const void* data, size_t count, bool insert = false ) = 0;
	virtual void			AppendSection( MemorySectionIdentifier section,
	                                       const void* data, size_t count ) = 0;

	virtual void			ShiftImages( const Offsets& offsets ) = 0; // Shift forth all sections by specified offset, filling space with empty data.
	virtual void			PasteFromContext( ctx_t id ) = 0; // Paste the specified context over the current one

	virtual void			SetSymbolImage( symbol_map&& symbols ) = 0; //
	virtual symbol_map		DumpSymbolImage() const = 0; // Write symbol map to image

	virtual void			VerifyReference( const DirectReference& ref,
											 Value::Type frame_stack_type ) const = 0; // Check if given reference is valid to access

	virtual void			ResetEverything() = 0; // Reset MMU to its initial state: deallocate all context buffers
};

class INTERPRETER_API IExecutor : LogBase( IExecutor ), public IModuleBase
{
public:
	virtual size_t ID() { return typeid( *this ).hash_code(); }

	// Returns instruction type supported by the executor module
	virtual Value::Type SupportedType() const = 0;

	// Re-registers all supported implementations.
	virtual void ResetImplementations() = 0;

	// Executes a single command.
	virtual void Execute( void* handle, Command& command ) = 0;
};

class INTERPRETER_API IBackend : LogBase( IBackend ), public IModuleBase
{
public:
	virtual void		CompileBuffer( size_t chk ) = 0;
	virtual bool		ImageIsOK( size_t chk ) = 0;
	virtual abi_native_fn_t
						GetImage( size_t chk ) = 0;
};

class INTERPRETER_API ILinker : LogBase( ILinker ), public IModuleBase
{
public:
	// Prepare for symbol linkage; load any existing symbols.
	virtual void DirectLink_Init() = 0;

	// Commit collected symbols to the MMU.
	virtual void DirectLink_Commit( bool UAT = false ) = 0;

	// Collect symbols in decode stage and append them to the temporary image.
	// Use provided offsets for auto-placement.
	virtual void DirectLink_Add( symbol_map&& symbols, const Offsets& limits ) = 0;

	// Handle a plain reference in decode stage.
	// Use provided offsets for auto-placement.
	virtual void DirectLink_HandleReference( Reference& ref, const Offsets& limits ) = 0;

	// Relocate the temporary image:
	// - adjust symbols and (possibly) references
	virtual void Relocate( const Offsets& offsets ) = 0;

	// Collect symbols from another linked source and append them to the temporary image.
	// Do not auto-place.
	virtual void MergeLink_Add( symbol_map&& symbols ) = 0;

	// Retrieve a direct reference for given arbitrary reference.
	// If "partial_resolution" is not null, the reference shall be resolved statically: that is,
	// 1) no indirections and dynamic symbols shall be resolved,
	// 2) in case there are unresolved components, the returned address is invalid
	//    and "partial_resolution" shall be set to false to indicate this.
	// The section of returned direct reference shall always be resolved and valid.
	//
	// NOTE: If the reference is completely resolved in the partial method,
	// "partial_resolution" is untouched (so it shall be initialized to true by the caller).
	virtual DirectReference Resolve( const Reference& reference, bool* partial_resolution = nullptr ) = 0;
};

} // namespace Processor

#endif // INTERPRETER_INTERFACES_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
