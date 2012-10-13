#include "stdafx.h"
#include "Interfaces.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			APIImplementation.cpp
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Core and main API implementation.
// -------------------------------------------------------------------------------------

namespace Processor
{

void ProcessorAPI::Flush()
{
	verify_method;

	MMU()->ResetEverything();
	CommandSet()->ResetCommandSet();

	for( unsigned i = 0; i <= Value::V_MAX; ++i ) {
		Executor( static_cast<Value::Type>( i ) )->ResetImplementations();
	}
}

void ProcessorAPI::Reset()
{
	verify_method;

	MMU()->ClearContext();
}

void ProcessorAPI::Clear()
{
	verify_method;

	IMMU* mmu = MMU();
	mmu->ResetBuffers( mmu->GetContext().buffer );
}

void ProcessorAPI::Delete()
{
	verify_method;

	MMU()->RestoreContext();
}

void ProcessorAPI::Load( FILE* file )
{
	verify_method;

	IMMU* mmu = MMU();

	mmu->AllocContextBuffer();
	msg( E_INFO, E_VERBOSE, "Loading from stream-> context %zu", mmu->GetContext().buffer );

	IReader* reader = Reader();
	cassert( reader, "Loader module is not attached" );

	FileType file_type = reader->RdSetup( file );
	switch( file_type ) {
	case FT_BINARY: {
		MemorySectionType sec_type;
		size_t elements_count, buffer_size;

		void* image_section_buffer = 0;
		size_t image_section_buffer_size = 0;

		msg( E_INFO, E_DEBUG, "Loading binary file absolute image" );

		while( reader->NextSection( &sec_type, &elements_count, &buffer_size ) ) {
			if( sec_type == SEC_SYMBOL_MAP ) {
				msg( E_INFO, E_DEBUG, "Reading symbols section: %zu records", elements_count );
				symbol_map external_symbols;

				reader->ReadSymbols( external_symbols );

				cassert( external_symbols.size() == elements_count, "Invalid symbol map size: %zu",
				         external_symbols.size() );

				mmu->ReadSymbolImage( std::move( external_symbols ) );
			}

			else {
				msg( E_INFO, E_DEBUG, "Reading section type \"%s\": %zu records (%zu bytes)",
				     ProcDebug::FileSectionType_ids[sec_type], elements_count, buffer_size );

				if( buffer_size > image_section_buffer_size ) {
					image_section_buffer = realloc( image_section_buffer, buffer_size );
					cassert( image_section_buffer, "Could not allocate section image buffer: \"%s\"",
					         strerror( errno ) );

					image_section_buffer_size = buffer_size;
				}

				reader->ReadSectionImage( image_section_buffer );
				mmu->ReadSection( sec_type, image_section_buffer, elements_count );
			}
		} // while (next section)

		msg( E_INFO, E_DEBUG, "Binary file read completed" );
		free( image_section_buffer );

		break;
	} // binary file

	case FT_STREAM: {

		msg( E_INFO, E_DEBUG, "Stream file decode start" );
		ILinker* linker = Linker();
		linker->DirectLink_Init();

		while( DecodeResult* result = reader->ReadStream() ) {

			if( !result->mentioned_symbols.empty() ) {
				size_t mmu_limits[SEC_MAX];
				mmu->QueryLimits( mmu_limits );

				msg( E_INFO, E_DEBUG, "Adding symbols" );
				linker->DirectLink_Add( result->mentioned_symbols, mmu_limits );
			}

			if( !result->commands.empty() ) {
				msg( E_INFO, E_DEBUG, "Adding commands: %zu", result->commands.size() );
				mmu->ReadSection( SEC_CODE_IMAGE, result->commands.data(), result->commands.size() );
			}

			if( !result->data.empty() ) {
				msg( E_INFO, E_DEBUG, "Adding data: %zu", result->data.size() );
				mmu->ReadSection( SEC_DATA_IMAGE, result->data.data(), result->data.size() );
			}

			if( !result->bytepool.empty() ) {
				msg( E_INFO, E_DEBUG, "Adding bytepool data: %zu bytes", result->bytepool.size() );
				mmu->ReadSection( SEC_BYTEPOOL_IMAGE, result->bytepool.data(), result->bytepool.size() );
			}

		}

		msg( E_INFO, E_DEBUG, "Stream decode completed - committing symbols" );
		linker->DirectLink_Commit();

		break;

	} // stream file

	case FT_NON_UNIFORM:
		casshole( "Not implemented" );
		break;

	default:
		casshole( "Switch error" );
		break;
	} // switch (file_type)

	msg( E_INFO, E_VERBOSE, "Loading completed" );
	reader->RdReset();
}

void ProcessorAPI::Dump( FILE* file )
{
	verify_method;

	IMMU* mmu = MMU();
	msg( E_INFO, E_VERBOSE, "Writing context to stream (ctx %zu)", mmu->GetContext().buffer );

	IWriter* writer = Writer();
	cassert( writer, "Writer module is not attached" );
	writer->WrSetup( file );
	writer->Write( mmu->GetContext().buffer );
	writer->WrReset();
}

void ProcessorAPI::MergeWithContext( size_t source_ctx )
{
	verify_method;

	IMMU* mmu = MMU();
	ILinker* linker = Linker();
	msg( E_INFO, E_VERBOSE, "Merging context buffers: %zu -> %zu", mmu->GetContext().buffer, source_ctx );

	mmu->SetContext( source_ctx );

	size_t source_limits[SEC_MAX];
	mmu->QueryLimits( source_limits );

	msg( E_INFO, E_DEBUG, "Source context buffer limits:" );
	for( unsigned i = 0; i < SEC_MAX; ++i ) {
		msg( E_INFO, E_DEBUG, "Section %s: %zu", ProcDebug::FileSectionType_ids[i], source_limits[i] );
	}

	msg( E_INFO, E_DEBUG, "Reading source context symbol map" );
	symbol_map src_symbols;
	mmu->WriteSymbolImage( src_symbols );

	mmu->RestoreContext();

	// Relocate dest section to free space for pasting
	mmu->ShiftImages( source_limits );
	linker->Relocate( source_limits );

	mmu->PasteFromContext( source_ctx );

	linker->DirectLink_Init();
	linker->MergeLink_Add( src_symbols );
	linker->DirectLink_Commit();
}

void ProcessorAPI::Compile()
{
	verify_method;

	try {
		IMMU* mmu = MMU();
		IBackend* backend = Backend();
		ILogic* logic = LogicProvider();

		msg( E_INFO, E_VERBOSE, "Attempting to compile context %zu", mmu->GetContext().buffer );
		cassert( backend, "Backend is not attached" );

		size_t chk = logic->ChecksumState();

		backend->CompileBuffer( chk, &InterpreterCallbackFunction );
		cassert( backend->ImageIsOK( chk ), "Backend reported compile error" );

		msg( E_INFO, E_VERBOSE, "Compilation OK: checksum assigned %zx", chk );
	}

	catch( std::exception& e ) {
		msg( E_CRITICAL, E_USER, "Compilation FAILED: %s", e.what() );
	}
}

calc_t ProcessorAPI::Exec()
{
	verify_method;

	IMMU* mmu = MMU();
	ILogic* logic = LogicProvider();
	IBackend* backend = Backend();

	Context& interpreter_context = mmu->GetContext();
	size_t chk = logic->ChecksumState();

	msg( E_INFO, E_VERBOSE, "Starting execution of context %zu (system checksum %zx)",
	     interpreter_context.buffer, chk );

	// Try to use backend if image was compiled
	if( backend && backend->ImageIsOK( chk ) ) {
		msg( E_INFO, E_VERBOSE, "Backend reports image is OK. Using precompiled image" );

		try {
			SetCallbackProcAPI( this );

			void* address = backend->GetImage( chk );
			calc_t result = ExecuteGate( address );

			SetCallbackProcAPI( 0 );

			msg( E_INFO, E_VERBOSE, "Native code execution COMPLETED." );
			return result;
		}

		catch( std::exception& e ) {
			msg( E_CRITICAL, E_USER, "Execution FAILED: Error = \"%s\". Reverting to interpreter", e.what() );
		}
	}

	// Else fall back to the interpreter.
	msg( E_INFO, E_VERBOSE, "Using interpreter" );

	while( !( interpreter_context.flags & MASK( F_EXIT ) ) ) {
		Command& command = mmu->ACommand( interpreter_context.ip );

		try {
			logic->ExecuteSingleCommand( command );
		}

		catch( std::exception& e ) {
			msg( E_WARNING, E_USER, "Last executed command: %s", logic->DumpCommand( command ) );

			const char* ctx_dump, *reg_dump, *stack_dump;
			mmu->DumpContext( &ctx_dump, &reg_dump, &stack_dump );

			msg( E_WARNING, E_USER, "MMU context dump:" );
			msg( E_WARNING, E_USER, "%s", ctx_dump );
			msg( E_WARNING, E_USER, "%s", reg_dump );
			msg( E_WARNING, E_USER, "%s", stack_dump );

			throw;
		}
	} // interpreter loop

	calc_t result;

	// Interpreter return value is in stack of last command.
	size_t last_command_stack_top = 0;
	mmu->QueryActiveStack( 0, &last_command_stack_top );

	if( last_command_stack_top )
		result = mmu->AStackTop( 0 );

	msg( E_INFO, E_VERBOSE, "Interpreter COMPLETED." );
	return result;
}

void ProcessorAPI::SetCallbackProcAPI( ProcessorAPI* procapi )
{
	callback_procapi = procapi;
}

abiret_t ProcessorAPI::InterpreterCallbackFunction( Command* cmd )
{
	// TODO, FIXME: this does not account for arguments.
	// While it is clear how to pass integer arguments (just use varargs to steal them from the native stack),
	// passing fp ones is unclear...
	// For now just manifest it in the trampoline's limitations.
	ILogic* logic = callback_procapi->LogicProvider();
	logic->ExecuteSingleCommand( *cmd );

	// stack is still set to the last type
	calc_t temporary_result = logic->StackPop();
	return temporary_result.GetABI();
}

} // namespace Processor
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
