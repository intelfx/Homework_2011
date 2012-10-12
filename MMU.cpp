#include "stdafx.h"
#include "MMU.h"

namespace {

template <typename T>
void PasteVector( std::vector<T>& dest, const std::vector<T>& src )
{
	if( dest.size() <= src.size() ) {
		dest = src;
	} else {
		for( size_t i = 0; i < src.size(); ++i ) {
			dest[i] = src[i];
		}
	}
}

size_t PasteBytes( char* dest, size_t d_length, const char* source, size_t s_length )
{
	if( d_length < s_length ) {
		char* new_dest = reinterpret_cast<char*>( realloc( dest, s_length ) );
		s_cassert( new_dest, "Failed to reallocate %p from %zu to %zu bytes when pasting",
		           dest, d_length, s_length );
	}
	memcpy( dest, source, s_length );
	return std::max( d_length, s_length );
}

} // unnamed namespace

namespace ProcessorImplementation
{
using namespace Processor;

MMU::MMU() :
	integer_stack(),
	fp_stack(),
	current_stack( 0 ),
	frame_stack( 0 ),
	current_stack_type( Value::V_MAX ),
	frame_stack_type( Value::V_MAX ),
	context_stack(),
	buffers(),
	context()
{
}

void MMU::OnAttach()
{
	ResetEverything();
}

void MMU::OnDetach()
{
	ResetEverything();
}

void MMU::ClearStacks()
{
	integer_stack.clear();
	fp_stack.clear();
}

void MMU::AllocBytes( size_t destination_size )
{
	InternalContextBuffer& icb = CurrentBuffer();

	if( destination_size > icb.bytepool_length ) {
		msg( E_INFO, E_DEBUG, "Reallocating bytepool memory [ctx %zu] %p:%zu-> %zu",
		     context.buffer, icb.bytepool_data, icb.bytepool_length, destination_size );

		char* new_bytepool = reinterpret_cast<char*>( realloc( icb.bytepool_data, destination_size ) );
		cassert( new_bytepool,
		         "Failed to reallocate bytepool [ctx %zd] %p:%zu-> %zu",
		         context.buffer, icb.bytepool_data, icb.bytepool_length, destination_size );

		icb.bytepool_data = new_bytepool;
		icb.bytepool_length = destination_size;
	}
}

void MMU::InternalWrStackPointer( std::vector<calc_t>** ptr, Value::Type type )
{
	switch( type ) {
	case Value::V_FLOAT:
		*ptr = &fp_stack;
		break;

	case Value::V_INTEGER:
		*ptr = &integer_stack;
		break;

	case Value::V_MAX:
	default:
		casshole( "Switch error" );
		break;
	}
}

void MMU::SelectStack( Value::Type type )
{
	verify_method;

	// If everything is already selected, do nothing.
	if( current_stack_type == type )
		return;

	msg( E_INFO, E_DEBUG, "Selecting stack type: \"%s\"", ProcDebug::ValueType_ids[type] );

	current_stack = 0;
	frame_stack = 0;

	current_stack_type = type;
	frame_stack_type = Value::V_INTEGER;

	if( current_stack_type != Value::V_MAX )
		InternalWrStackPointer( &current_stack, current_stack_type );

	if( frame_stack_type != Value::V_MAX )
		InternalWrStackPointer( &frame_stack, frame_stack_type );
}

void MMU::QueryActiveStack( Value::Type* type, size_t* top )
{
	verify_method;

	if( type )
		*type = current_stack_type;

	if( top )
		*top = current_stack ? current_stack->size() : 0;
}

void MMU::AlterStackTop( short int offset )
{
	verify_method;
	CheckStackOperation();

	long new_size = current_stack->size() + offset;
	cassert( new_size >= 0, "Unable to alter stack top by %hd: current top is %zu, new top is negative",
	         offset, current_stack->size() );

	current_stack->resize( new_size );
}

calc_t& MMU::AStackTop( size_t offset )
{
	verify_method;
	CheckStackAddress( offset );
	return * ( current_stack->rbegin() + offset );
}

calc_t& MMU::AStackFrame( int offset )
{
	verify_method;
	CheckFrameAddress( offset );

	// Offset to stack frame may be either positive or negative.
	// It is added to stack frame verbatim.
	return frame_stack->at( context.frame + offset );
}

Processor::symbol_type& MMU::ASymbol( size_t hash )
{
	verify_method;

	auto sym_iter = CurrentBuffer().sym_table.find( hash );
	cassert( sym_iter != CurrentBuffer().sym_table.end(), "Unknown symbol (hash %zx)", hash );
	return sym_iter->second;
}

calc_t& MMU::ARegister( Register reg_id )
{
	verify_method;

	cassert( reg_id < R_MAX, "Invalid register #: %u [max %u]", reg_id, R_MAX );
	return CurrentBuffer().registers[reg_id];
}

Command& MMU::ACommand( size_t ip )
{
	verify_method;

	cassert( ip < CurrentBuffer().commands.size(),
	         "IP overflow: %zu [max %zu]", ip, CurrentBuffer().commands.size() );
	return CurrentBuffer().commands.at( ip );
}

calc_t& MMU::AData( size_t addr )
{
	verify_method;

	cassert( addr < CurrentBuffer().data.size(),
	         "Data section overflow: %zu [max %zu]", addr, CurrentBuffer().data.size() );
	return CurrentBuffer().data.at( addr );
}

char* MMU::ABytepool( size_t offset )
{
	verify_method;
	InternalContextBuffer& icb = CurrentBuffer();

	cassert( offset < icb.bytepool_length,
	         "Cannot reference bytepool address %zu [allocated size %zu]",
	         offset, icb.bytepool_length );

	return icb.bytepool_data + offset;
}

void MMU::QueryLimits( size_t limits[SEC_MAX] ) const
{
	memset( limits, 0, sizeof( size_t ) * SEC_MAX );

	const InternalContextBuffer& ctx = CurrentBuffer();

	limits[SEC_CODE_IMAGE] = ctx.commands.size();
	limits[SEC_DATA_IMAGE] = ctx.data.size();
	limits[SEC_BYTEPOOL_IMAGE] = ctx.bytepool_length;
}

void MMU::ReadSection( MemorySectionType section, void* image, size_t count )
{
	verify_method;
	cassert( image, "NULL section image pointer" );

	switch( section ) {
	case SEC_CODE_IMAGE: {
		msg( E_INFO, E_DEBUG, "Adding text (%p : %zu)-> ctx %zu",
		     image, count, context.buffer );

		std::vector<Command>& text_dest = CurrentBuffer().commands;
		Command* tmp_image = reinterpret_cast<Command*>( image );

		text_dest.insert( text_dest.end(), tmp_image, tmp_image + count );
		break;
	}

	case SEC_DATA_IMAGE: {
		msg( E_INFO, E_DEBUG, "Adding data (%p : %zu)-> ctx %zu",
		     image, count, context.buffer );

		std::vector<calc_t>& data_dest = CurrentBuffer().data;
		calc_t* tmp_image = reinterpret_cast<calc_t*>( image );

		data_dest.insert( data_dest.end(), tmp_image, tmp_image + count );
		break;
	}

	case SEC_BYTEPOOL_IMAGE: {
		msg( E_INFO, E_VERBOSE, "Adding raw data (%p : %zu)-> ctx %zu",
		     image, count, context.buffer );

		InternalContextBuffer& icb = CurrentBuffer();
		AllocBytes( icb.bytepool_length + count );

		memcpy( icb.bytepool_data + icb.bytepool_length, image, count );
		break;
	}

	case SEC_STACK_IMAGE: {
		msg( E_INFO, E_DEBUG, "Reading stack images (%p : %zu)-> global", image, count );
		ClearStacks();

		calc_t* src = reinterpret_cast<calc_t*>( image );

		while( count-- ) {
			switch( src->type ) {
			case Value::V_FLOAT:
				fp_stack.push_back( *src );
				break;

			case Value::V_INTEGER:
				integer_stack.push_back( *src );
				break;

			case Value::V_MAX:
				casshole( "Invalid input element type" );
				break;

			default:
				casshole( "Switch error" );
				break;
			}

			++src;
		}

		break;
	}

	case SEC_SYMBOL_MAP:
		casshole( "Cannot do binary operations on symbol map section" );
		break;

	case SEC_MAX:
	default:
		casshole( "Switch error" );
		break;
	}
}

void MMU::ReadSymbolImage( symbol_map && symbols )
{
	verify_method;

	msg( E_INFO, E_DEBUG, "Attaching symbol map (%zu records)-> ctx %zu", symbols.size(), context.buffer );
	CurrentBuffer().sym_table = std::move( symbols );
}

void MMU::WriteSymbolImage( symbol_map& symbols ) const
{
	verify_method;

	const InternalContextBuffer& ctx = CurrentBuffer();

	msg( E_INFO, E_DEBUG, "Saving symbol map [ctx %zu] - %zu records", context.buffer, ctx.sym_table.size() );
	symbols = ctx.sym_table;
}

void MMU::WriteSection( MemorySectionType section, void* image ) const
{
	verify_method;
	cassert( image, "NULL section image pointer" );

	switch( section ) {
	case SEC_CODE_IMAGE: {
		msg( E_INFO, E_VERBOSE, "Writing text image [ctx %zu]-> %p",
		     context.buffer, image );

		const std::vector<Command>& commands = CurrentBuffer().commands;
		Command* dest = reinterpret_cast<Command*>( image );

		std::vector<Command>::const_iterator src = commands.begin();

		while( src != commands.end() )
			*dest++ = *src++;

		break;
	}

	case SEC_DATA_IMAGE: {
		msg( E_INFO, E_VERBOSE, "Writing data image [ctx %zu]-> %p",
		     context.buffer, image );

		const std::vector<calc_t>& data = CurrentBuffer().data;
		calc_t* dest = reinterpret_cast<calc_t*>( image );

		std::vector<calc_t>::const_iterator src = data.begin();

		while( src != data.end() )
			*dest++ = *src++;

		break;
	}

	case SEC_BYTEPOOL_IMAGE: {
		msg( E_INFO, E_VERBOSE, "Writing byte data image [ctx %zu]-> %p",
		     context.buffer, image );

		memcpy( image, CurrentBuffer().bytepool_data, CurrentBuffer().bytepool_length );
		break;
	}

	case SEC_STACK_IMAGE: {
		CheckStackOperation();

		msg( E_INFO, E_DEBUG, "Writing stack image (%s)-> %p",
		     ProcDebug::ValueType_ids[current_stack_type], image );

		calc_t* dest = reinterpret_cast<calc_t*>( image );

		std::vector<calc_t>::const_iterator src = current_stack->begin();

		while( src != current_stack->end() )
			*dest++ = *src++;

		break;
	}

	case SEC_SYMBOL_MAP:
		casshole( "Cannot do binary operations on symbol map section" );
		break;

	case SEC_MAX:
	default:
		casshole( "Switch error" );
		break;
	}
}

void MMU::ShiftImages( size_t offsets[SEC_MAX] )
{
	verify_method;

	msg( E_INFO, E_DEBUG, "Shifting sections in context %zu", context.buffer );

	CurrentBuffer().commands.insert( CurrentBuffer().commands.begin(), offsets[SEC_CODE_IMAGE], Command() );
	CurrentBuffer().data.insert( CurrentBuffer().data.begin(), offsets[SEC_DATA_IMAGE], calc_t() );

	size_t bytepool_shift_offset = offsets[SEC_BYTEPOOL_IMAGE];
	char* new_bytepool = reinterpret_cast<char*>( malloc( CurrentBuffer().bytepool_length + bytepool_shift_offset ) );
	cassert( new_bytepool, "Could not allocate bytepool image for shifting context (shift by %zu)",
			 bytepool_shift_offset );
	memset( new_bytepool, 0, bytepool_shift_offset );
	memcpy( new_bytepool + bytepool_shift_offset, CurrentBuffer().bytepool_data, CurrentBuffer().bytepool_length );

	free( CurrentBuffer().bytepool_data );
	CurrentBuffer().bytepool_data = new_bytepool;
	CurrentBuffer().bytepool_length += bytepool_shift_offset;
}

void MMU::PasteFromContext( size_t ctx_id )
{
	verify_method;

	msg( E_INFO, E_DEBUG, "Pasting context %zu to current context (%zu)", ctx_id, context.buffer );
	cassert( ctx_id < buffers.size(), "Invalid given buffer ID [%zu]: max %zu", ctx_id, buffers.size() );

	InternalContextBuffer& rhs = buffers.at( ctx_id );

	PasteVector<Command>( CurrentBuffer().commands, rhs.commands );
	PasteVector<calc_t>( CurrentBuffer().data, rhs.data );
	CurrentBuffer().bytepool_length = PasteBytes( CurrentBuffer().bytepool_data, CurrentBuffer().bytepool_length,
	                                              rhs.bytepool_data, rhs.bytepool_length );
}

void MMU::ResetBuffers( size_t ctx_id )
{
	verify_method;

	msg( E_INFO, E_DEBUG, "Resetting images in context %zu", ctx_id );
	cassert( ctx_id < buffers.size(), "Invalid given buffer ID [%zu]: max %zu", ctx_id, buffers.size() );

	InternalContextBuffer& buffer_dest = buffers[ctx_id];
	free( buffer_dest.bytepool_data );
	buffer_dest = InternalContextBuffer();
}

void MMU::ResetEverything()
{
	// Firstly reset context to put MMU into uninitialised state
	ClearContext();
	context.buffer = -1;

	// Then clear stack contents
	ClearStacks();
	SelectStack( Value::V_MAX );
	context_stack.clear();

	// Finally, clear context buffers
	for( InternalContextBuffer& buffer: buffers ) {
		free( buffer.bytepool_data );
	}
	buffers.clear();
}

void MMU::SaveContext()
{
	verify_method;

	bool log_save = !context_stack.empty();

	if( log_save ) {
		msg( E_INFO, E_DEBUG, "Saving execution context" );
		LogDumpCtx( context );
	}

	context_stack.push_back( context );

	// Update current context
	++context.depth;
	context.frame = frame_stack ? frame_stack->size() : 0;

	if( log_save ) {
		msg( E_INFO, E_DEBUG, "New execution context:" );
		LogDumpCtx( context );
	}
}

void MMU::ClearContext()
{
	context.depth = 0;
	context.flags = 0;
	context.frame = 0;
	context.ip = 0;
}

void MMU::NextContextBuffer()
{
	SaveContext();
	ClearContext();
	++context.buffer;
	if( buffers.size() <= context.buffer ) {
		buffers.resize( context.buffer + 1 );
	}

	if( context.buffer )
		msg( E_INFO, E_DEBUG, "Switched to next context buffer [%zu]", context.buffer );

	else
		msg( E_INFO, E_DEBUG, "MMU reset (switch to context buffer 0)" );

	verify_method;
}

void MMU::AllocContextBuffer()
{
	NextContextBuffer();

	ResetBuffers( context.buffer );
}

void MMU::RestoreContext()
{
	verify_method;

	msg( E_INFO, E_DEBUG, "Restoring execution context %zu", context.buffer );
	LogDumpCtx( context );

	context = context_stack.back();
	context_stack.pop_back();

	msg( E_INFO, E_DEBUG, "Restored context:" );
	LogDumpCtx( context );

	if( context_stack.empty() )
		msg( E_INFO, E_DEBUG, "MMU back to uninitialized state" );
}

void MMU::InternalDumpCtx( const Context& w_context ) const
{
	// Dump registers

	char* reg_out = dump_data_registers;
	reg_out += sprintf( reg_out, "Reg:" );

	if( w_context.buffer < buffers.size() ) {
		const InternalContextBuffer& cb = buffers[w_context.buffer];

		for( unsigned reg_id = 0; reg_id < R_MAX; ++reg_id ) {
			reg_out += sprintf( reg_out, " %s [%s]",
			                    proc_->LogicProvider()->EncodeRegister( static_cast<Register>( reg_id ) ),
			                    ( ProcDebug::PrintValue( cb.registers[reg_id] ), ProcDebug::debug_buffer ) );
		}
	}

	else {
		reg_out += sprintf( reg_out, " N/A" );
	}

	// Dump main context

	sprintf( dump_data_context,
	         "Ctx [%zd]: IP [%zu] FL [%08zx] DEPTH [%zu]",
	         w_context.buffer, w_context.ip, w_context.flags, w_context.depth );

	// Dump stacks

	char* stacks_out = dump_data_stacks;
	stacks_out += sprintf( stacks_out, "Stacks:" );

	// Main stack

	if( current_stack ) {
		if( current_stack->size() ) {
			ProcDebug::PrintValue( current_stack->back() );
			stacks_out += sprintf( stacks_out, " MAIN [\"%s\"] = (top (%zu) value (%s))",
			                       ProcDebug::ValueType_ids[current_stack_type],
			                       current_stack->size(),
			                       ProcDebug::debug_buffer );
		}

		else {
			stacks_out += sprintf( stacks_out, " MAIN [\"%s\"] = (empty)",
			                       ProcDebug::ValueType_ids[current_stack_type] );
		}
	}

	else {
		stacks_out += sprintf( stacks_out, " MAIN = (disabled)" );
	}

	// Frame stack

	if( frame_stack ) {
		stacks_out += sprintf( stacks_out, " FRAME [\"%s\"] = (top (%zu) + %s frame (%zu))",
		                       ProcDebug::ValueType_ids[frame_stack_type],
		                       frame_stack->size(),
		                       ( w_context.frame <= frame_stack->size() ) ? "valid" : "invalid",
		                       w_context.frame );
	}

	else {
		stacks_out += sprintf( stacks_out, " FRAME = (disabled)" );
	}
}

void MMU::LogDumpCtx( const Context& w_context ) const
{
	verify_method;

	InternalDumpCtx( w_context );

	msg( E_INFO, E_DEBUG, "%s", dump_data_context );
	msg( E_INFO, E_DEBUG, "%s", dump_data_registers );
	msg( E_INFO, E_DEBUG, "%s", dump_data_stacks );
}

void MMU::DumpContext( const char** ctx, const char** regs, const char** stacks ) const
{
	verify_method;

	InternalDumpCtx( context );

	if( ctx )
		*ctx = dump_data_context;

	if( regs )
		*regs = dump_data_registers;

	if( stacks )
		*stacks = dump_data_stacks;
}

Context& MMU::GetContext()
{
	verify_method;

	return context;
}

MMU::InternalContextBuffer& MMU::CurrentBuffer()
{
	verify_method;

	return buffers[context.buffer];
}

const MMU::InternalContextBuffer& MMU::CurrentBuffer() const
{
	verify_method;

	return buffers[context.buffer];
}

bool MMU::_Verify() const
{
	if( context.buffer != static_cast<size_t>( -1 ) ) {
		verify_statement( !context_stack.empty(), "Call/context stack underflow" );

		verify_statement( context.buffer < buffers.size(), "Invalid context buffer ID [%zd]: max %zu",
		                  context.buffer, buffers.size() );

		if( !buffers[context.buffer].commands.empty() )
			verify_statement( context.ip < buffers[context.buffer].commands.size(),
			                  "Invalid instruction pointer [%zu]: max %zu",
			                  context.ip, buffers[context.buffer].commands.size() );

	}

	return 1;
}

void MMU::VerifyReference( const Processor::DirectReference& ref ) const
{
	switch( ref.section ) {
	case S_CODE:
		cverify( ref.address < CurrentBuffer().commands.size(),
		         "Invalid reference [TEXT:%zu] : limit %zu", ref.address, CurrentBuffer().commands.size() );
		break;

	case S_DATA:
		cverify( ref.address < CurrentBuffer().data.size(),
		         "Invalid reference [DATA:%zu] : limit %zu", ref.address, CurrentBuffer().data.size() );
		break;

	case S_REGISTER:
		cverify( ref.address < R_MAX,
		         "Invalid reference [REG:%zu] : max register ID %d", ref.address, R_MAX - 1 );
		break;

	case S_FRAME:
		CheckFrameOperation();

		cverify( context.frame + ref.address < frame_stack->size(),
		         "Invalid reference [FRAME:%zu] : frame at %zu | top at %zu",
		         ref.address, context.frame, frame_stack->size() );
		break;


	case S_FRAME_BACK:
		CheckFrameOperation();

		cverify( context.frame >= ref.address,
		         "Invalid reference [BACKFRAME:%zu] : frame at %zu | top at %zu",
		         ref.address, context.frame, frame_stack->size() );
		break;

	case S_BYTEPOOL:
		cverify( ref.address < CurrentBuffer().bytepool_length,
		         "Invalid reference [BYTEPOOL:%zx] : allocated %zx bytes",
		         ref.address, CurrentBuffer().bytepool_length );
		break;

	case S_NONE:
	case S_MAX:
	default:
		casshole( "Switch error" );
		break;
	}
}
} // namespace ProcessorImplementation
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
