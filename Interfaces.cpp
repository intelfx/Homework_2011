#include "stdafx.h"
#include "Interfaces.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			Interfaces.cpp
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Plugin interface helper methods.
// -------------------------------------------------------------------------------------

namespace Processor
{

ProcessorAPI* ProcessorAPI::callback_procapi = 0;

void ProcessorAPI::Attach( IModuleBase* module )
{
	bool was_attach = 0;

	if( IBackend* backend = dynamic_cast<IBackend*>( module ) ) {
		Attach_( backend );
		was_attach = 1;
	}

	if( IReader* reader = dynamic_cast<IReader*>( module ) ) {
		Attach_( reader );
		was_attach = 1;
	}

	if( IWriter* writer = dynamic_cast<IWriter*>( module ) ) {
		Attach_( writer );
		was_attach = 1;
	}

	if( IMMU* mmu = dynamic_cast<IMMU*>( module ) ) {
		Attach_( mmu );
		was_attach = 1;
	}

	if( IExecutor* executor = dynamic_cast<IExecutor*>( module ) ) {
		Attach_( executor );
		was_attach = 1;
	}

	if( ICommandSet* cset = dynamic_cast<ICommandSet*>( module ) ) {
		Attach_( cset );
		was_attach = 1;
	}

	if( ILogic* internal_logic = dynamic_cast<ILogic*>( module ) ) {
		Attach_( internal_logic );
		was_attach = 1;
	}

	if( ILinker* linker = dynamic_cast<ILinker*>( module ) ) {
		Attach_( linker );
		was_attach = 1;
	}

	if( !was_attach )
		msg( E_CRITICAL, E_USER, "Could not attach module \"%s\" (%p) - unrecognized module type",
		     Debug::API::GetClassName( module ), module );
}

void ProcessorAPI::Detach( const IModuleBase* module )
{
	bool was_detach = 0;

	if( shadow_backend_ == module ) {
		shadow_backend_ = backend_ = 0;
		was_detach = 1;
	}

	// We can not dynamic_cast<>() to IExecutor* to get executor's type
	// since module itself is already destroyed by this time.
	// See explanations in Interfaces.h near shadow_* declarations.
	for( unsigned i = 0; i <= Value::V_MAX; ++i )
		if( shadow_executors_[i] == module ) {
			shadow_executors_[i] = executors_[i] = 0;
			was_detach = 1;
			break;
		}

	if( shadow_reader_ == module ) {
		shadow_reader_ = reader_ = 0;
		was_detach = 1;
	}

	if( shadow_writer_ == module ) {
		shadow_writer_ = writer_ = 0;
		was_detach = 1;
	}

	if( shadow_cset_ == module ) {
		shadow_cset_ = cset_ = 0;
		was_detach = 1;
	}

	if( shadow_logic_ == module ) {
		shadow_logic_ = internal_logic_ = 0;
		was_detach = 1;
	}

	if( shadow_linker_ == module ) {
		shadow_linker_ = linker_ = 0;
		was_detach = 1;
	}

	if( shadow_mmu_ == module ) {
		shadow_mmu_ = mmu_ = 0;
		was_detach = 1;
	}

	if( !was_detach )
		msg( E_WARNING, E_VERBOSE, "Not detaching \"%s\" (%p) - has not been attached",
		     Debug::API::GetClassName( module ), module );
}

void ProcessorAPI::Attach_( IBackend* backend )
{
	cassert( backend, "NULL backend to be attached" );

	if( backend_ )
		backend_->DetachSelf();

	shadow_backend_ = backend_ = backend;
	backend_->SetProcessor( this );
}

void ProcessorAPI::Attach_( ICommandSet* cset )
{
	cassert( cset, "NULL command set to be attached" );

	if( cset_ )
		cset_->DetachSelf();

	shadow_cset_ = cset_ = cset;
	cset_->SetProcessor( this );
}

void ProcessorAPI::Attach_( IExecutor* executor )
{
	Value::Type supported_type = executor->SupportedType();
	cassert( supported_type <= Value::V_MAX,
	         "Invalid supported type (executor module to be registered: %p \"%s\")",
	         executor, Debug::API::GetClassName( executor ) );

	IExecutor*& executor_ = executors_[supported_type];
	IModuleBase*& shadow_executor_ = shadow_executors_[supported_type];

	if( executor_ )
		executor_->DetachSelf();

	shadow_executor_ = executor_ = executor;
	executor_->SetProcessor( this );
}

void ProcessorAPI::Attach_( ILinker* linker )
{
	if( linker_ )
		linker_->DetachSelf();

	shadow_linker_ = linker_ = linker;
	linker_->SetProcessor( this );
}

void ProcessorAPI::Attach_( ILogic* logic )
{
	if( internal_logic_ )
		internal_logic_->DetachSelf();

	shadow_logic_ = internal_logic_ = logic;
	internal_logic_->SetProcessor( this );
}

void ProcessorAPI::Attach_( IMMU* mmu )
{
	if( mmu_ )
		mmu_->DetachSelf();

	shadow_mmu_ = mmu_ = mmu;
	mmu_->SetProcessor( this );
}

void ProcessorAPI::Attach_( IReader* reader )
{
	if( reader_ )
		reader_->DetachSelf();

	shadow_reader_ = reader_ = reader;
	reader_->SetProcessor( this );
}

void ProcessorAPI::Attach_( IWriter* writer )
{
	if( writer_ )
		writer_->DetachSelf();

	shadow_writer_ = writer_ = writer;
	writer_->SetProcessor( this );
}

IModuleBase::~IModuleBase()
{
	DetachSelf();
}

void ProcessorAPI::Initialise( bool value )
{
	if( value ) {
		initialise_completed = 1;
		msg( E_INFO, E_USER, "API initialised" );
		verify_method;
	}

	else {
		msg( E_WARNING, E_VERBOSE, "API deinitialised" );
		verify_method;
		initialise_completed = 0;
	}
}

bool ProcessorAPI::_Verify() const
{
	verify_statement( initialise_completed, "Not initialised" );

	verify_submodule( internal_logic_, "internal logic" );
	verify_submodule( cset_, "command set" );
	verify_submodule( linker_, "linker" );
	verify_submodule( mmu_, "MMU" );

	for( unsigned i = 0; i <= Value::V_MAX; ++i ) {
		verify_submodule_x( executors_[i], "%s executor", ProcDebug::AddrType_ids[i] );
	}

	if( backend_ )
		verify_submodule( backend_, "backend compiler" );

	if( reader_ )
		verify_submodule( reader_, "read I/O engine" );

	if( writer_ )
		verify_submodule( writer_, "write I/O engine" );


	verify_statement( internal_logic_ == shadow_logic_, "Logic pointers inconsistence" );
	verify_statement( cset_ == shadow_cset_, "Command set pointers inconsistence" );
	verify_statement( linker_ == shadow_linker_, "Linker pointers inconsistence" );
	verify_statement( mmu_ == shadow_mmu_, "MMU pointers inconsistence" );

	for( unsigned i = 0; i <= Value::V_MAX; ++i ) {
		verify_statement( executors_[i] == shadow_executors_[i], "Executor \"%s\" pointer inconsistence", ProcDebug::AddrType_ids[i] );
	}

	verify_statement( backend_ == shadow_backend_, "Backend pointers inconsistence" );
	verify_statement( reader_ == shadow_reader_, "Reader pointers inconsistence" );
	verify_statement( writer_ == shadow_writer_ , "Writer pointers inconsistence" );

	return 1;
}

ProcessorAPI::ProcessorAPI() :
	reader_( 0 ),
	writer_( 0 ),
	mmu_( 0 ),
	executors_(),
	backend_( 0 ),
	linker_( 0 ),
	cset_( 0 ),
	internal_logic_( 0 ),
	shadow_reader_( 0 ),
	shadow_writer_( 0 ),
	shadow_mmu_( 0 ),
	shadow_executors_(),
	shadow_backend_( 0 ),
	initialise_completed( 0 )
{
	memset( executors_, 0, Value::V_MAX );
	memset( shadow_executors_, 0, Value::V_MAX );
}

} // namespace Processor

ImplementDescriptor( IReader, "reader module", MOD_APPMODULE );
ImplementDescriptor( IWriter, "writer module", MOD_APPMODULE );
ImplementDescriptor( ILinker, "linker module", MOD_APPMODULE );
ImplementDescriptor( IMMU, "memory management unit", MOD_APPMODULE );
ImplementDescriptor( IExecutor, "executor module", MOD_APPMODULE );
ImplementDescriptor( ProcessorAPI, "processor API", MOD_APPMODULE );
ImplementDescriptor( ICommandSet, "command set handler", MOD_APPMODULE );
ImplementDescriptor( IBackend, "native compiler", MOD_APPMODULE );
ImplementDescriptor( ILogic, "processor logic", MOD_APPMODULE );
ImplementDescriptor( IModuleBase, "unspecified module", MOD_APPMODULE );

// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
