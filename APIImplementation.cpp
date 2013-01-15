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

	LogicProvider()->ClearContextStack();
	LogicProvider()->ResetCurrentContextState();
	CommandSet()->ResetCommandSet();
	MMU()->ResetEverything();

	for( unsigned i = 0; i <= Value::V_MAX; ++i ) {
		Executor( static_cast<Value::Type>( i ) )->ResetImplementations();
	}
}

void ProcessorAPI::Reset()
{
	verify_method;

	LogicProvider()->ResetCurrentContextState();
}

void ProcessorAPI::Clear()
{
	verify_method;

	Reset();
	MMU()->ResetContextBuffer( CurrentContext().buffer );
}

void ProcessorAPI::SelectContext( ctx_t id )
{
	verify_method;

	LogicProvider()->SwitchToContextBuffer( id );
}

void ProcessorAPI::DeleteContext( ctx_t id )
{
	verify_method;

	cassert( CurrentContext().buffer != id, "Attempt to deallocate currently selected context buffer %zu", id );
	MMU()->ReleaseContextBuffer( id );
}

void ProcessorAPI::DeleteCurrentContext()
{
	verify_method;

	ctx_t current_context_buffer = CurrentContext().buffer,
	      new_context_buffer = current_context_buffer;
	size_t frames_erased = 0;
	cassert( current_context_buffer, "Trying to delete current context buffer when nothing is allocated" );

	msg( E_INFO, E_VERBOSE, "Deleting context buffer %zu", current_context_buffer );

	do {
		LogicProvider()->RestoreCurrentContext();
		new_context_buffer = CurrentContext().buffer;
		++frames_erased;
	} while( new_context_buffer == current_context_buffer );

	MMU()->ReleaseContextBuffer( current_context_buffer );
	msg( E_INFO, E_DEBUG, "Context buffer %zu deleted (new is %zu). %zu frames removed.",
		 current_context_buffer, new_context_buffer, frames_erased );
}

ctx_t ProcessorAPI::Load( FILE* file )
{
	verify_method;

	IMMU* mmu = MMU();
	ILogic* logic = LogicProvider();

	ctx_t allocated_ctx = mmu->AllocateContextBuffer();
	logic->SwitchToContextBuffer( allocated_ctx );
	msg( E_INFO, E_VERBOSE, "Loading from stream -> context %zu", allocated_ctx );

	IReader* reader = Reader();
	cassert( reader, "Loader module is not attached" );

	FileType file_type = reader->RdSetup( file );
	switch( file_type ) {
	case FT_BINARY: {

		std::pair<MemorySectionIdentifier, size_t> section_info;

		msg( E_INFO, E_DEBUG, "Loading binary file absolute image" );

		while( ( section_info = reader->NextSection() ).first ) {
			if( section_info.first.SectionType() == SEC_SYMBOL_MAP ) {
				msg( E_INFO, E_DEBUG, "Reading symbols section: %zu records", section_info.second );

				symbol_map external_symbols = reader->ReadSymbols();
				cassert( external_symbols.size() == section_info.second, "Invalid symbol map size: %zu",
				         external_symbols.size() );
				mmu->SetSymbolImage( std::move( external_symbols ) );
			}

			else {
				msg( E_INFO, E_DEBUG, "Reading section type \"%s\": %zu records",
				     ProcDebug::Print( section_info.first.SectionType() ).c_str(),
					 section_info.second );

				llarray image_section_buffer = reader->ReadSectionImage();
				mmu->AppendSection( section_info.first, image_section_buffer, section_info.second );
			}
		} // while (next section)

		msg( E_INFO, E_DEBUG, "Binary file read completed" );
		break;
	} // binary file

	case FT_STREAM: {

		msg( E_INFO, E_DEBUG, "Stream file decode start" );
		ILinker* linker = Linker();
		linker->DirectLink_Init();

		while( DecodeResult* result = reader->ReadStream() ) {
			Offsets limits = mmu->QuerySectionLimits();

			if( !result->mentioned_symbols.empty() ) {
				msg( E_INFO, E_DEBUG, "Adding symbols" );
				linker->DirectLink_Add( std::move( result->mentioned_symbols ), limits );
			}

			if( !result->commands.empty() ) {
				msg( E_INFO, E_DEBUG, "Processing commands: %zu", result->commands.size() );
				for( Command& cmd: result->commands ) {
					if( CommandSet()->DecodeCommand( cmd.id )->arg_type == A_REFERENCE ) {
						linker->DirectLink_HandleReference( cmd.arg.ref, limits );
					}
				}
				msg( E_INFO, E_DEBUG, "Adding commands: %zu", result->commands.size() );
				mmu->AppendSection( SEC_CODE_IMAGE, result->commands.data(), result->commands.size() );
			}

			if( !result->data.empty() ) {
				msg( E_INFO, E_DEBUG, "Adding data: %zu", result->data.size() );
				mmu->AppendSection( SEC_DATA_IMAGE, result->data.data(), result->data.size() );
			}

			if( !result->bytepool.empty() ) {
				msg( E_INFO, E_DEBUG, "Adding bytepool data: %zu bytes", result->bytepool.size() );
				mmu->AppendSection( SEC_BYTEPOOL_IMAGE, result->bytepool.data(), result->bytepool.size() );
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

	logic->RestoreCurrentContext();
	return allocated_ctx;
}

void ProcessorAPI::Dump( ctx_t id, FILE* file )
{
	verify_method;

	msg( E_INFO, E_VERBOSE, "Writing context to stream (ctx %zu)", CurrentContext().buffer );

	IWriter* writer = Writer();
	cassert( writer, "Writer module is not attached" );
	writer->WrSetup( file );
	writer->Write( id );
	writer->WrReset();
}

ctx_t ProcessorAPI::MergeContexts( const std::vector<ctx_t>& contexts )
{
	verify_method;

	IMMU* mmu = MMU();
	ILinker* linker = Linker();
	ILogic* logic = LogicProvider();
	msg( E_INFO, E_VERBOSE, "Merging %zu context buffers", contexts.size() );

	ctx_t result_ctx = mmu->AllocateContextBuffer();
	logic->SwitchToContextBuffer( result_ctx );
	linker->DirectLink_Init();

	for( std::vector<ctx_t>::const_reverse_iterator it = contexts.crbegin(); it != contexts.crend(); ++it ) {
		ctx_t source_ctx = *it;
		msg( E_INFO, E_VERBOSE, "Adding context buffer %zu", source_ctx );

		logic->SwitchToContextBuffer( source_ctx );

		Offsets source_limits = mmu->QuerySectionLimits();

		msg( E_INFO, E_DEBUG, "Reading source context symbol map" );
		symbol_map src_symbols = mmu->DumpSymbolImage();

		logic->RestoreCurrentContext();

		// Relocate dest section to free space for pasting
		mmu->ShiftImages( source_limits );
		mmu->PasteFromContext( source_ctx );

		linker->Relocate( source_limits );
		linker->MergeLink_Add( std::move( src_symbols ) );
	}

	linker->DirectLink_Commit();
	logic->RestoreCurrentContext();

	msg( E_INFO, E_VERBOSE, "Merging completed" );
	return result_ctx;
}

void ProcessorAPI::Compile()
{
	verify_method;

	try {
		IBackend* backend = Backend();
		ILogic* logic = LogicProvider();

		msg( E_INFO, E_VERBOSE, "Attempting to compile context %zu", CurrentContext().buffer );
		cassert( backend, "Backend is not attached" );

		size_t chk = logic->ChecksumState();

		backend->CompileBuffer( chk );
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

	msg( E_INFO, E_VERBOSE, "Starting execution of context %zu", CurrentContext().buffer );

	size_t chk = logic->ChecksumState();

	msg( E_INFO, E_VERBOSE, "System checksum %zx", chk );

	// Try to use backend if image was compiled
	if( backend && backend->ImageIsOK( chk ) ) {
		msg( E_INFO, E_VERBOSE, "Backend reports image is OK. Using precompiled image" );

		try {
			abi_native_fn_t address = backend->GetImage( chk );
			calc_t result = ExecuteGate( address );

			msg( E_INFO, E_VERBOSE, "Native code execution COMPLETED." );
			return result;
		}

		catch( std::exception& e ) {
			msg( E_CRITICAL, E_USER, "Execution FAILED: Error = \"%s\". Reverting to interpreter", e.what() );
		}
	}

	// Else fall back to the interpreter.
	msg( E_INFO, E_VERBOSE, "Using interpreter" );

	Command* last_command = nullptr;

	while( !( CurrentContext().flags & MASK( F_EXIT ) ) ) {

		last_command = &mmu->ACommand( CurrentContext().ip );

		try {
			logic->ExecuteSingleCommand( *last_command );
		}

		catch( std::exception& e ) {
			msg( E_WARNING, E_USER, "Last executed command: %s", logic->DumpCommand( *last_command ).c_str() );

			std::string reg_dump, stack_dump, ctx_dump;
			mmu->DumpContext( &reg_dump, &stack_dump );
			DumpExecutionContext( &ctx_dump );

			msg( E_WARNING, E_USER, "MMU context dump:" );
			msg( E_WARNING, E_USER, "%s", ctx_dump.c_str() );
			msg( E_WARNING, E_USER, "%s", reg_dump.c_str() );
			msg( E_WARNING, E_USER, "%s", stack_dump.c_str() );

			throw;
		}
	} // interpreter loop

	calc_t result;

	// Interpreter return value is in stack of last command.
	if( logic->StackSize() )
		result = logic->StackTop();

	msg( E_INFO, E_VERBOSE, "Interpreter COMPLETED." );
	return result;
}

void ProcessorAPI::DumpExecutionContext( std::string* ctx_dump )
{
	char temporary_buffer[STATIC_LENGTH];
	snprintf( temporary_buffer, STATIC_LENGTH,
	          "Ctx [%zd]: IP [%zu] FL [%08zx] DEPTH [%zu]",
	          CurrentContext().buffer, CurrentContext().ip, CurrentContext().flags, CurrentContext().depth );
	ctx_dump->assign( temporary_buffer );
}

} // namespace Processor
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
