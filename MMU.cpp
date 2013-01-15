#include "stdafx.h"
#include "MMU.h"

namespace {

template <typename T, typename Iter>
void PasteVector( std::vector<T>& dest, size_t address, Iter first, Iter last )
{
	if( last == first ) {
		return;
	}

	ptrdiff_t count = last - first;
	s_cassert( count > 0, "Negative or zero elements count to insert: %td", count );

	if( dest.size() < address + count ) {
		dest.resize( address + count );
	}

	for( Iter i = first; i != last; ++i ) {
		dest[address++] = *i;
	}
}

} // unnamed namespace

namespace ProcessorImplementation
{
using namespace Processor;

MMU::MMU() :
	stacks_(),
	buffers_(),
	current_buffer_( buffers_.end() )
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
	for( unsigned i = 0; i < Value::V_MAX; ++i ) {
		stacks_[i].clear();
	}
}

ctx_t MMU::CurrentContextBuffer() const
{
	return (current_buffer_ == buffers_.end()) ? 0 : current_buffer_->first;
}

ctx_t MMU::AllocateContextBuffer()
{
	ctx_t allocated_index = buffers_.empty() ? 1 : buffers_.rbegin()->first + 1;
	msg( E_INFO, E_DEBUG, "Allocating a new context buffer ID %lu", allocated_index );

	auto insert_result = buffers_.insert( std::make_pair( allocated_index, InternalContextBuffer() ) );
	cassert( insert_result.second, "Context buffer ID %lu was already allocated", allocated_index );

	return allocated_index;
}

void MMU::SelectContextBuffer( ctx_t id )
{
	if( id ) {
		msg( E_INFO, E_DEBUG, "Selecting the context buffer ID %lu", id );
		auto new_buffer = buffers_.find( id );
		cassert( new_buffer != buffers_.end(), "Attempt to select an inexistent context buffer ID %lu", id );
		current_buffer_ = new_buffer;
	} else {
		msg( E_WARNING, E_DEBUG, "Deselecting the context buffer" );
		current_buffer_ = buffers_.end();
	}
}

void MMU::ReleaseContextBuffer( ctx_t id )
{
	msg( E_INFO, E_DEBUG, "Freeing the context buffer ID %lu", id );
	size_t count_erased = buffers_.erase( id );
	cassert( count_erased > 0, "Attempt to remove an inexistent context buffer ID %lu", id );
}

size_t MMU::QueryStackTop( Value::Type type ) const
{
	cassert( type < Value::V_MAX, "Invalid stack type required: \"%s\"", ProcDebug::Print( type ).c_str() );
	return stacks_[type].size();
}

void MMU::SetStackTop( Value::Type type, ssize_t adjust )
{
	cassert( type < Value::V_MAX, "Invalid stack type required: \"%s\"", ProcDebug::Print( type ).c_str() );

	ssize_t new_size = stacks_[type].size() + adjust;
	cassert( new_size >= 0, "Invalid adjustment requested: %zd (T: %zu)", adjust, stacks_[type].size() );

	calc_t pattern;
	pattern.type = type;
	pattern.Set( type, 0, true );
	stacks_[type].resize( stacks_[type].size() + adjust, pattern );
}

calc_t& MMU::AStackTop( Value::Type type, size_t offset )
{
	verify_method;
	CheckStackAddress( type, offset );
	return *( stacks_[type].rbegin() + offset );
}

calc_t& MMU::AStackFrame( Value::Type type, ssize_t offset )
{
	verify_method;
	CheckFrameAddress( type, offset );

	// Offset to stack frame may be either positive or negative.
	// It is added to stack frame verbatim.
	return stacks_[type].at( proc_->CurrentContext().frame + offset );
}

symbol_type& MMU::ASymbol( size_t hash )
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

	cassert( offset < icb.bytepool.size(),
	         "Cannot reference bytepool address %zu [allocated size %zu]",
	         offset, icb.bytepool.size() );

	return icb.bytepool + offset;
}

Offsets MMU::QuerySectionLimits() const
{
	Offsets ret;
	const InternalContextBuffer& icb = CurrentBuffer();

	ret.Code() = icb.commands.size();
	ret.Data() = icb.data.size();
	ret.Bytepool() = icb.bytepool.size();
	for( unsigned i = 0; i < Value::V_MAX; ++i ) {
		ret.Stack( static_cast<Value::Type>( i ) ) = stacks_[i].size();
	}
	return ret;
}

void MMU::AppendSection( MemorySectionIdentifier section, const void* image, size_t count )
{
	verify_method;
	cassert( image, "NULL section image pointer" );

	switch( section.SectionType() ) {
	case SEC_CODE_IMAGE: {
		msg( E_INFO, E_DEBUG, "Adding text (count: %zu) -> buffer %zu",
		     count, CurrentContextBuffer() );

		std::vector<Command>& text_dest = CurrentBuffer().commands;
		const Command* tmp_image = reinterpret_cast<const Command*>( image );

		text_dest.insert( text_dest.end(), tmp_image, tmp_image + count );
		break;
	}

	case SEC_DATA_IMAGE: {
		msg( E_INFO, E_DEBUG, "Adding data (count: %zu) -> buffer %zu",
		     count, CurrentContextBuffer() );

		std::vector<calc_t>& data_dest = CurrentBuffer().data;
		const calc_t* tmp_image = reinterpret_cast<const calc_t*>( image );

		data_dest.insert( data_dest.end(), tmp_image, tmp_image + count );
		break;
	}

	case SEC_BYTEPOOL_IMAGE: {
		msg( E_INFO, E_VERBOSE, "Adding raw data (bytes: %zu) -> buffer %zu",
		     count, CurrentContextBuffer() );

		CurrentBuffer().bytepool.append( count, image );
		break;
	}

	case SEC_STACK_IMAGE: {
		msg( E_INFO, E_DEBUG, "Reading stacks (overall size : %zu)", count );
		ClearStacks();

		size_t stats[Value::V_MAX] = {};
		const calc_t* src = reinterpret_cast<const calc_t*>( image );

		while( count-- ) {
			cassert( src->type < Value::V_MAX, "Invalid stack image element type" );
			stacks_[src->type].push_back( *src );
			++stats[src->type];
			++src;
		}

		for( unsigned i = 0; i < Value::V_MAX; ++i ) {
			msg( E_INFO, E_DEBUG, "Done: read %zu elements of type \"%s\"",
				 stats[i], ProcDebug::Print( static_cast<Value::Type>( i ) ).c_str() );
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

void MMU::ModifySection( MemorySectionIdentifier section, size_t address,
						 const void* image, size_t count, bool insert )
{
	verify_method;
	cassert( image, "NULL section image pointer" );

	const char* dbg_op = insert ? "Inserting" : "Pasting";

	switch( section.SectionType() ) {
	case SEC_CODE_IMAGE: {
		msg( E_INFO, E_DEBUG, "%s text (count: %zu) -> buffer %zu at %zu",
			 dbg_op, count, CurrentContextBuffer(), address );

		std::vector<Command>& text_dest = CurrentBuffer().commands;
		const Command* tmp_image = reinterpret_cast<const Command*>( image );

		if( insert ) {
			text_dest.insert( text_dest.begin() + address, tmp_image, tmp_image + count );
		} else {
			PasteVector( text_dest, address, tmp_image, tmp_image + count );
		}
		break;
	}

	case SEC_DATA_IMAGE: {
		msg( E_INFO, E_DEBUG, "%s data (count: %zu) -> buffer %zu at %zu",
		     dbg_op, count, CurrentContextBuffer(), address );

		std::vector<calc_t>& data_dest = CurrentBuffer().data;
		const calc_t* tmp_image = reinterpret_cast<const calc_t*>( image );

		if( insert ) {
			data_dest.insert( data_dest.begin() + address, tmp_image, tmp_image + count );
		} else {
			PasteVector( data_dest, address, tmp_image, tmp_image + count );
		}
		break;
	}

	case SEC_BYTEPOOL_IMAGE: {
		msg( E_INFO, E_VERBOSE, "%s raw data (bytes: %zu) -> buffer %zu at %zu",
		     dbg_op, count, CurrentContextBuffer(), address );

		if( insert ) {
			CurrentBuffer().bytepool.insert( address, count, image );
		} else {
			CurrentBuffer().bytepool.paste( address, count, image );
		}
		break;
	}

	case SEC_STACK_IMAGE: {
		casshole( "Modifying stacks is not supported/needed" );
		/*
		 * Reason: too much work for nearly no uses.
		 * Such insertion is meaningful only if all data elements have the same type.
		 */
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

void MMU::SetSymbolImage( symbol_map&& symbols )
{
	verify_method;

	msg( E_INFO, E_DEBUG, "Setting symbol map (records: %zu) -> buffer %zu",
		 symbols.size(), CurrentContextBuffer() );
	CurrentBuffer().sym_table = std::move( symbols );
}

symbol_map MMU::DumpSymbolImage() const
{
	verify_method;

	const InternalContextBuffer& ctx = CurrentBuffer();

	msg( E_INFO, E_DEBUG, "Dumping symbol map (buffer %zu) -> %zu records",
		 CurrentContextBuffer(), ctx.sym_table.size() );
	return CurrentBuffer().sym_table;
}

llarray MMU::DumpSection( MemorySectionIdentifier section, size_t address, size_t count )
{
	verify_method;

	InternalContextBuffer& icb = CurrentBuffer();

	switch( section.SectionType() ) {
	case SEC_CODE_IMAGE: {
		msg( E_INFO, E_VERBOSE, "Dumping text (buffer %zu) -> range %zu:%zu",
		     CurrentContextBuffer(), address, count );

		cassert( address + count <= icb.commands.size(),
				 "Invalid range requested (section limit: %zu)", icb.commands.size() );

		return llarray( icb.commands.data() + address, sizeof( Command ) * count );
	}

	case SEC_DATA_IMAGE: {
		msg( E_INFO, E_VERBOSE, "Dumping data (buffer %zu) -> range %zu:%zu",
		     CurrentContextBuffer(), address, count );

		cassert( address + count <= icb.data.size(),
				 "Invalid range requested (section limit: %zu)", icb.data.size() );

		return llarray( icb.data.data() + address, sizeof( calc_t ) * count );
	}

	case SEC_BYTEPOOL_IMAGE: {
		msg( E_INFO, E_VERBOSE, "Dumping bytepool (buffer %zu) -> range %zu:%zu",
		     CurrentContextBuffer(), address, count );

		cassert( address + count <= icb.bytepool.size(),
				 "Invalid range requested (section limit: %zu)", icb.data.size() );

		return llarray( icb.bytepool + address, count );
	}

	case SEC_STACK_IMAGE: {
		Value::Type stack_type = section.DataType();
		cassert( stack_type < Value::V_MAX, "Invalid stack type requested" );

		msg( E_INFO, E_DEBUG, "Dumping %s stack image -> range %zu:%zu",
		     ProcDebug::Print( stack_type ).c_str(), address, count );

		cassert( address + count <= stacks_[stack_type].size(),
				 "Invalid range requested (stack top: %zu)", stacks_[stack_type].size() );

		return llarray( stacks_[stack_type].data() + address, sizeof( calc_t ) * count );
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

void MMU::ShiftImages( const Offsets& offsets )
{
	verify_method;

	msg( E_INFO, E_DEBUG, "Shifting sections in context %zu", CurrentContextBuffer() );

	InternalContextBuffer& icb = CurrentBuffer();

	icb.commands.insert( icb.commands.begin(), offsets.Code(), Command() );
	icb.data.insert( icb.data.begin(), offsets.Data(), calc_t() );
	icb.bytepool.insert( 0, offsets.Bytepool(), nullptr );
}

void MMU::PasteFromContext( ctx_t id )
{
	verify_method;
	ctx_t main_ctx = CurrentContextBuffer();
	msg( E_INFO, E_DEBUG, "Pasting context %zu -> %zu", id, main_ctx );

	Debug::API::ClrObjectFlag( this, Debug::OF_USEVERIFY );
	InternalContextBuffer& dest = CurrentBuffer();
	SelectContextBuffer( id );
	InternalContextBuffer& src = CurrentBuffer();
	SelectContextBuffer( main_ctx );
	Debug::API::SetObjectFlag( this, Debug::OF_USEVERIFY );

	PasteVector( dest.commands, 0, src.commands.begin(), src.commands.end() );
	PasteVector( dest.data, 0, src.data.begin(), src.data.end() );
	dest.bytepool.paste( 0, src.bytepool.size(), src.bytepool );
}

void MMU::ResetContextBuffer( ctx_t id )
{
	verify_method;
	ctx_t main_ctx = CurrentContextBuffer();
	msg( E_INFO, E_DEBUG, "Clearing the context buffer ID %lu", id );

	SelectContextBuffer( id );
	InternalContextBuffer& dest = CurrentBuffer();
	SelectContextBuffer( main_ctx );

	dest = InternalContextBuffer();
}

void MMU::ResetEverything()
{
	// Firstly reset context to put MMU into uninitialised state
	SelectContextBuffer( 0 );
	ClearStacks();
	buffers_.clear();
}

void MMU::InternalDumpCtx( const InternalContextBuffer* icb, std::string& registers, std::string& stacks ) const
{
	char temporary_buffer[STATIC_LENGTH];
	char* out = nullptr;

	// Dump registers
	out = temporary_buffer;
	out += sprintf( out, "Reg:" );
	if( icb ) {
		for( unsigned reg_id = 0; reg_id < R_MAX; ++reg_id ) {
			out += sprintf( out, " %s [%s]",
			                proc_->LogicProvider()->EncodeRegister( static_cast<Register>( reg_id ) ),
			                ProcDebug::PrintValue( icb->registers[reg_id] ).c_str() );
		}
	} else {
		out += sprintf( out, " N/A" );
	}
	registers.assign( temporary_buffer );

	// Dump stacks
	out = temporary_buffer;
	out += sprintf( out, "Stacks:" );
	for( unsigned i = 0; i < Value::V_MAX; ++i ) {
		const std::vector<calc_t>& stack = stacks_[i];

		out += sprintf( out, " [\"%s\"] = ",
						ProcDebug::Print( static_cast<Value::Type>( i ) ).c_str() );

		if( stack.size() ) {
			out += sprintf( out, "(top (%zu) value (%s))",
			                stack.size(),
			                ProcDebug::PrintValue( stack.back() ).c_str() );
		} else {
			out += sprintf( out, "(empty)" );
		}
	}
	stacks.assign( temporary_buffer );

}

void MMU::DumpContext( std::string* regs, std::string* stacks ) const
{
	verify_method;

	InternalDumpCtx( current_buffer_ == buffers_.end() ? nullptr : &current_buffer_->second,
	                 *regs, *stacks );
}

bool MMU::_Verify() const
{
	verify_statement( buffers_.find( 0 ) == buffers_.end(), "Reserved context buffer id is allocated" );
	const Context& current_ctx = proc_->CurrentContext();
	if( current_ctx.buffer != 0 )
	{
		auto it = buffers_.find( current_ctx.buffer );
		verify_statement( it != buffers_.end(), "Inexistent context buffer ID %lu is selected in the core", current_ctx.buffer );

		verify_statement( !it->second.commands.size() ||
		                  current_ctx.ip < it->second.commands.size(),
		                  "Invalid instruction pointer [%zu]: max %zu",
		                  current_ctx.ip, it->second.commands.size() );
	}

	return 1;
}

void MMU::VerifyReference( const DirectReference& ref, Value::Type frame_stack_type ) const
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

	case S_FRAME: {
		CheckFrameOperation( frame_stack_type );
		ssize_t address = proc_->CurrentContext().frame + ref.address;

		cassert( address >= 0,
		         "Invalid reference [FRAME:%zu]: (F: %zu)",
		         ref.address, proc_->CurrentContext().frame );
		cassert( static_cast<size_t>( address ) < stacks_[frame_stack_type].size(),
		         "Invalid reference [FRAME:%zu]: (F: %zu T: %zu)",
		         ref.address, proc_->CurrentContext().frame, stacks_[frame_stack_type].size() );
		break;
	}

	case S_FRAME_BACK: {
		CheckFrameOperation( frame_stack_type );
		ssize_t address = proc_->CurrentContext().frame - ref.address;

		cassert( address >= 0,
		         "Invalid reference [BACKFRAME:%zu]: (F: %zu)",
		         ref.address, proc_->CurrentContext().frame );
		cassert( static_cast<size_t>( address ) < stacks_[frame_stack_type].size(),
		         "Invalid reference [BACKFRAME:%zu]: (F: %zu T: %zu)",
		         ref.address, proc_->CurrentContext().frame, stacks_[frame_stack_type].size() );
		break;
	}

	case S_BYTEPOOL:
		cverify( ref.address < CurrentBuffer().bytepool.size(),
		         "Invalid reference [BYTEPOOL:%zu] : allocated %zu bytes",
		         ref.address, CurrentBuffer().bytepool.size() );
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
