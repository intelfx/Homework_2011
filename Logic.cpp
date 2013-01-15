#include "stdafx.h"
#include "Logic.h"

#include <uXray/fxhash_functions.h>
#include <math.h>

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		Logic.cpp
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	Default supplementary logic plugin implementation.
// -------------------------------------------------------------------------------------

namespace ProcessorImplementation
{
using namespace Processor;

std::string Logic::DumpCommand( Command& command ) const
{
	char temporary_buffer[STATIC_LENGTH];
	const CommandTraits* cmd_traits = proc_->CommandSet()->DecodeCommand( command.id );

	if( cmd_traits )
		snprintf( temporary_buffer, STATIC_LENGTH, "[PC=%zu] \"%s\" (%s type) argument %s",
		         proc_->CurrentContext().ip, cmd_traits->mnemonic, ProcDebug::Print( command.type ).c_str(),
		         ProcDebug::PrintArgument( cmd_traits->arg_type, command.arg, proc_->MMU() ).c_str() );

	else
		snprintf( temporary_buffer, STATIC_LENGTH, "[PC=%zu] id=0x%04hd (%s type) command unknown",
		         proc_->CurrentContext().ip, command.id, ProcDebug::Print( command.type ).c_str() );

	return temporary_buffer;
}

void Logic::ExecuteSingleCommand( Command& command )
{
	verify_method;

	ICommandSet* command_set = proc_->CommandSet();
	Context& command_context = proc_->CurrentContext();

	msg( E_INFO, E_DEBUG, "Executing %s", DumpCommand( command ).c_str() );

	// Perform caching of executor/handle since execution must be O(1)
	if( !command.cached_handle ) {
		const CommandTraits* command_traits = command_set->DecodeCommand( command.id );
		cassert( command_traits, "Could not decode command (invalid ID: 0x%04hx)", command.id );

		// User-supplied handle (pointer to function) should be registered with module ID 0
		if( void* user_handle = command_set->GetExecutionHandle( *command_traits, 0 ) ) {
			command.cached_executor = nullptr;
			command.cached_handle = user_handle;
		}

		// Select conventional handle
		else {
			// Select valid executor based on command type and flavor
			command.cached_executor = proc_->Executor( command_traits->is_service_command ? Value::V_MAX : command.type );

			cassert( command.cached_executor, "Was unable to select executor for command type \"%s\"",
			         ProcDebug::Print( command.type ).c_str() );

			command.cached_handle = command_set->GetExecutionHandle( *command_traits, command.cached_executor->ID() );
		}
	}

	// Reset jump flag
	command_context.flags &= ~MASK( F_WAS_JUMP );

	// Zero handle means no-op
	if( !command.cached_handle )
		return;

	// Select the stack for subsequent logic operations
	current_stack_type_ = command.type;

	// Execute the command
	if( command.cached_executor )
		command.cached_executor->Execute( command.cached_handle, command );

	else
		Fcast<void(*)( ProcessorAPI*, Command& )>( command.cached_handle )( proc_, command );


	// If there was neither exit nor jump, advance the PC
	if( !( command_context.flags & MASK( F_EXIT ) ) &&
	    !( command_context.flags & MASK( F_WAS_JUMP ) ) ) {
		++command_context.ip;
	}
}

size_t Logic::ChecksumState()
{
	verify_method;

	size_t checksum = 0;

	/*
	 * Checksum contains:
	 * - current section limits
	 * - current code image
	 */

	IMMU* mmu = proc_->MMU();

	Context& ctx = proc_->CurrentContext();
	checksum = hasher_xroll( &ctx, sizeof( ctx ), checksum );

	Offsets limits = mmu->QuerySectionLimits();
	checksum = hasher_xroll( limits.Raw(), SEC_COUNT, checksum );

	for( size_t i = 0; i < limits.Code(); ++i ) {
		checksum = hasher_xroll( &mmu->ACommand( i ), sizeof( Command ), checksum );
	}

	// TODO hash symbol map
	return checksum;
}

void Logic::Analyze( calc_t value )
{
	verify_method;

	Context& ctx = proc_->CurrentContext();
	ctx.flags &= ~( MASK( F_ZERO ) | MASK( F_NEGATIVE ) | MASK( F_INVALIDFP ) );

	switch( value.type ) {
	case Value::V_FLOAT: {
		int classification = fpclassify( value.fp );

		switch( classification ) {
		case FP_NAN:
		case FP_INFINITE:
			ctx.flags |= MASK( F_INVALIDFP );
			break;

		case FP_ZERO:
			ctx.flags |= MASK( F_ZERO );
			break;

		case FP_SUBNORMAL:
		case FP_NORMAL:
			if( value.fp < 0 )
				ctx.flags |= MASK( F_NEGATIVE );

			break;

		default:
			casshole( "fpclassify() error - returned %d", classification );
			break;
		}

		break;
	}

	case Value::V_INTEGER: {
		if( value.integer == 0 )
			ctx.flags |= MASK( F_ZERO );

		else if( value.integer < 0 )
			ctx.flags |= MASK( F_NEGATIVE );

		break;
	}

	case Value::V_MAX:
		casshole( "Uninitialised value" );
		break;

	default:
		casshole( "Switch error" );
		break;
	}
}

Register Logic::DecodeRegister( const char* reg )
{
	verify_method;
	cassert( reg && reg[0], "Invalid or NULL register string to decode" );

	for( unsigned rid = R_A; rid < R_MAX; ++rid ) {
		if( !strcmp( reg, RegisterIDs[rid] ) ) {
			return static_cast<Register>( rid );
		}
	}

	casshole( "Unknown register string: \"%s\"", reg );
	return R_MAX; /* for GCC to be quiet */
}

const char* Logic::EncodeRegister( Register reg )
{
	verify_method;

	cassert( reg < R_MAX, "Invalid register ID to encode: %d", reg );
	return RegisterIDs[reg];
}

void Logic::Jump( const DirectReference& ref )
{
	verify_method;

	cverify( ref.section == S_CODE, "Cannot jump to non-CODE reference to %s",
	         ProcDebug::PrintReference( ref ).c_str() );

	msg( E_INFO, E_DEBUG, "Jumping -> %zu", ref.address );

	Context& ctx = proc_->CurrentContext();
	ctx.ip = ref.address;
	ctx.flags |= MASK( F_WAS_JUMP );
}

void Logic::UpdateType( const DirectReference& ref, Value::Type requested_type )
{
	verify_method;

	switch( ref.section ) {
	case S_CODE:
		casshole( "Attempt to write type to CODE section: reference to %s",
		          ProcDebug::PrintReference( ref ).c_str() );
		break;

	case S_DATA:
		proc_->MMU()->AData( ref.address ).type = requested_type;
		break;

	case S_FRAME:
	case S_FRAME_BACK:
		casshole( "Attempt to write type to STACK: reference to %s",
		          ProcDebug::PrintReference( ref ).c_str() );
		break;

	case S_REGISTER:
		msg( E_WARNING, E_VERBOSE, "Attempt to change type of register - will not be changed" );
		break;

	case S_BYTEPOOL:
		casshole( "Attempt to write type to BYTEPOOL: reference to %s",
		          ProcDebug::PrintReference( ref ).c_str() );
		break;

	case S_NONE:
	case S_MAX:
	default:
		casshole( "Invalid address type or switch error" );
		break;
	}
}

void Logic::Write( const DirectReference& ref, calc_t value )
{
	verify_method;

	switch( ref.section ) {
	case S_CODE:
		casshole( "Attempt to write to CODE section: reference to %s",
		          ProcDebug::PrintReference( ref ).c_str() );
		break;

	case S_DATA:
		// do type checking here
		proc_->MMU()->AData( ref.address ).Assign( value );
		break;

	case S_FRAME:
		// do type checking here - stack must be homogeneous
		proc_->MMU()->AStackFrame( frame_stack_type_, ref.address ).Assign( value );

	case S_FRAME_BACK:
		msg( E_WARNING, E_VERBOSE, "Attempt to write to function parameter: reference to %s",
		     ProcDebug::PrintReference( ref ).c_str() );

		// do type checking here - stack must be homogeneous
		proc_->MMU()->AStackFrame( frame_stack_type_, -ref.address ).Assign( value );
		break;

	case S_REGISTER:
		// no type checking here
		proc_->MMU()->ARegister( static_cast<Register>( ref.address ) ) = value;
		break;

	case S_BYTEPOOL:
		value.Expect( Value::V_INTEGER );
		*proc_->MMU()->ABytepool( ref.address ) = reinterpret_cast<char&>( value.integer ); // truncation
		break;

	case S_NONE:
	case S_MAX:
	default:
		casshole( "Invalid address type or switch error" );
		break;
	}
}

calc_t Logic::Read( const DirectReference& ref )
{
	verify_method;

	switch( ref.section ) {
	case S_CODE:
		casshole( "Attempt to read from CODE section: reference to %s",
		          ProcDebug::PrintReference( ref ).c_str() );

	case S_DATA:
		return proc_->MMU()->AData( ref.address );

	case S_FRAME:
		return proc_->MMU()->AStackFrame( frame_stack_type_, ref.address );

	case S_FRAME_BACK:
		return proc_->MMU()->AStackFrame( frame_stack_type_, -ref.address );

	case S_REGISTER:
		return proc_->MMU()->ARegister( static_cast<Register>( ref.address ) );

	case S_BYTEPOOL:
		return static_cast<int_t>( *proc_->MMU()->ABytepool( ref.address ) );

	case S_NONE:
	case S_MAX:
	default:
		casshole( "Invalid address type or switch error" );
		break;
	}

	return static_cast<fp_t>( strtof( "NAN", nullptr ) ); /* for GCC not to complain */
}

size_t Logic::StackSize()
{
	verify_method;

	return proc_->MMU()->QueryStackTop( current_stack_type_ );
}

calc_t Logic::StackPop()
{
	verify_method;

	IMMU* mmu = proc_->MMU();
	calc_t data = mmu->AStackTop( current_stack_type_, 0 );
	mmu->SetStackTop( current_stack_type_, -1 );

	return data;
}

void Logic::StackPush( calc_t value )
{
	verify_method;

	IMMU* mmu = proc_->MMU();
	mmu->SetStackTop( current_stack_type_, 1 );
	mmu->AStackTop( current_stack_type_, 0 ) = value;
}

calc_t Logic::StackTop()
{
	verify_method;

	return proc_->MMU()->AStackTop( current_stack_type_, 0 );
}

void Logic::ResetCurrentContextState()
{
	verify_method;

	Context& ctx = proc_->CurrentContext();
	ctx.flags = 0;
	ctx.frame = 0;
	ctx.ip = 0;
}

void Logic::SetCurrentContextBuffer( ctx_t id )
{
	verify_method;

	Context& ctx = proc_->CurrentContext();
	proc_->MMU()->SelectContextBuffer( id );
	ctx.__buffer_rw = id;
	ctx.depth = 0;
}

void Logic::SaveCurrentContext()
{
	verify_method;

	Context& ctx = proc_->CurrentContext();
	call_stack_.push( ctx );
	++ctx.depth;
}

void Logic::RestoreCurrentContext()
{
	verify_method;

	Context ctx = call_stack_.top();
	// The context buffer may get updated.
	proc_->MMU()->SelectContextBuffer( ctx.buffer );
	proc_->CurrentContext() = ctx;
	call_stack_.pop();
}

void Logic::ClearContextStack()
{
	verify_method;

	std::stack<Context> empty;
	call_stack_.swap( empty );
}

void Logic::Syscall( size_t index )
{
	msg( E_INFO, E_VERBOSE, "System call with index %zu", index );

	switch( index ) {
	case 0: {
		calc_t input_data = proc_->MMU()->ARegister( R_F );
		input_data.Expect( Value::V_INTEGER, true );
		msg( E_INFO, E_USER, "Application output: \"%s\"", proc_->MMU()->ABytepool( input_data.integer ) );
		break;
	}

	case 1: {
		msg( E_INFO, E_USER, "Reading integer from command-line" );
		char buffer[STATIC_LENGTH];
		fgets( buffer, STATIC_LENGTH, stdin );
		proc_->MMU()->ARegister( R_A ).Set( Value::V_INTEGER, atoi( buffer ), true );
	}

	default:
		msg( E_WARNING, E_VERBOSE, "Undefined syscall index: %zu", index );
		break;
	}
}

const char* Logic::RegisterIDs[R_MAX] = {
	"ra",
	"rb",
	"rc",
	"rd",
	"re",
	"rf"
};

} // namespace ProcessorImplementation
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
