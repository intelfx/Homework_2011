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
	static ProcessorAPI* callback_procapi;

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

protected:
	virtual bool _Verify() const;

	static void SetCallbackProcAPI( ProcessorAPI* procapi );
	static abiret_t InterpreterCallbackFunction( Command* cmd );

	template <typename T>
	T* CheckReturnModule( T* module, const char* modname ) {
		if( initialise_completed ) {
			verify_method;
			return module;
		}

		else {
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
	IExecutor*	Executor( Value::Type type )	{ return CheckReturnModule( executors_[type], "executor" ); }
	ILinker*	Linker()					{ return CheckReturnModule( linker_, "linker" ); }
	ICommandSet* CommandSet()				{ return CheckReturnModule( cset_, "command set" ); }
	ILogic*		LogicProvider()				{ return CheckReturnModule( internal_logic_, "logic provider" ); }

	NativeExecutionManager& ExecutionManager() { return nem_; }


	void	Flush(); // Completely reset and reinitialise the system
	void	Reset(); // Reset current execution context
	void	Clear(); // Clear current execution buffers
	void	Load( FILE* file ); // Load into a new context/buffer
	void	Dump( FILE* file ); // Dump current context/buffer
	void	Delete(); // Return to previous context/buffer
	void	Compile(); // Invoke backend to compile the bytecode
	calc_t	Exec(); // Execute current system state whatever it is now
};

class INTERPRETER_API IModuleBase : LogBase( IModuleBase )
{
protected:
	ProcessorAPI* proc_;

	IModuleBase() : proc_( 0 ) {}
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
			proc_ = 0;
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

	virtual calc_t	StackTop() = 0; // Calculation stack "top" operation
	virtual calc_t	StackPop() = 0; // Calculation stack "pop" operation
	virtual void	StackPush( calc_t value ) = 0; // Calculation stack "push" operation

	virtual void	ExecuteSingleCommand( Command& command ) = 0; // Execute a single command
	virtual char* 	DumpCommand( Command& command ) const = 0; // Decode and log a single command
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
	// Returns 1 if call succeeded (0 if there is no sections).
	virtual size_t NextSection( MemorySectionType* type, size_t* count, size_t* bytes ) = 0;

	// Reads current section image into "destination".
	virtual void ReadSectionImage( void* destination ) = 0;

	// Reads current section symbol map into "destination".
	virtual void ReadSymbols( symbol_map& destination ) = 0;

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

	virtual void Write( size_t ctx_id ) = 0;
};

class INTERPRETER_API IMMU : LogBase( IMMU ), public IModuleBase
{
public:
	virtual Context&		GetContext() = 0;  // Get current context data (persistent)
	virtual void			DumpContext( const char** ctx,
	                                     const char** regs,
	                                     const char** stacks ) const = 0; // Print context data to the string


	virtual void			SelectStack( Value::Type type ); // Selects stack for consequent operations
	virtual void			QueryActiveStack( Value::Type* type, size_t* top ) = 0; // Request information about selected stack
	virtual void			AlterStackTop( short offset ) = 0; // Change stack top relatively (-1 is pop).

	virtual calc_t&			AStackFrame( int offset ) = 0; // Access calculation stack relative to context's stack frame pointer
	virtual calc_t&			AStackTop( size_t offset ) = 0; // Access calculation stack relative to its top
	virtual calc_t&			ARegister( Register reg_id ) = 0; // Access register
	virtual Command&		ACommand( size_t ip ) = 0; // Access CODE section
	virtual calc_t&			AData( size_t addr ) = 0; // Access DATA section
	virtual symbol_type&	ASymbol( size_t hash ) = 0; // Access symbol buffer
	virtual char*			ABytepool( size_t offset ) = 0; // Access byte pool

	virtual void			QueryLimits( size_t limits[SEC_MAX] ) const = 0; // Query current section limits (to array)
	virtual void			ReadSection( MemorySectionType section, void* image, size_t count ) = 0; // Read and append section image
	virtual void			WriteSection( MemorySectionType section, void* image ) const = 0; // Write section to image
	virtual void			ReadSymbolImage( symbol_map && symbols ) = 0; // Read symbol map
	virtual void			WriteSymbolImage( symbol_map& symbols ) const = 0; // Write symbol map to image

	virtual void			PasteFromContext( size_t ctx_id ) = 0; // Paste the specified context over the current one

	virtual void VerifyReference( const DirectReference& ref ) const = 0; // Check if given reference is valid to access

	virtual void ResetBuffers( size_t ctx_id ) = 0; // Reset specified context buffer
	virtual void ResetEverything() = 0; // Reset MMU to its initial state, clearing all stacks.

	virtual void SaveContext() = 0; // Push context on call stack (PC must point to call instruction)
	virtual void ClearContext() = 0; // Clear all context data but context ID
	virtual void RestoreContext() = 0; // Load context from call stack

	virtual void NextContextBuffer() = 0; // Push context on call stack; clear all and increment context ID
	virtual void AllocContextBuffer() = 0; // Switch to next context buffer; reset the buffer

	void SetTemporaryContext( size_t ctx_id );
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
	virtual void		CompileBuffer( size_t chk, abi_callback_fn_t callback ) = 0;
	virtual bool		ImageIsOK( size_t chk ) = 0;
	virtual void*		GetImage( size_t chk ) = 0;
};

class INTERPRETER_API ILinker : LogBase( ILinker ), public IModuleBase
{
public:
	// Prepare for direct on-load symbol linkage.
	virtual void DirectLink_Init() = 0;

	// Commit collected buffers to the MMU.
	virtual void DirectLink_Commit( bool UAT = false ) = 0;

	// Collect symbols in decode stage.
	// Use provided offsets for auto-placement.
	virtual void DirectLink_Add( symbol_map& symbols, size_t offsets[SEC_MAX] ) = 0;

	// Retrieve a direct reference for given arbitrary reference.
	virtual DirectReference Resolve( Reference& reference ) = 0; // or get an unresolved symbol error
};

} // namespace Processor

#endif // INTERPRETER_INTERFACES_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
