#include "stdafx.h"
#include "MMU.h"

namespace ProcessorImplementation
{
using namespace Processor;

calc_t& MMU::AStackTop (size_t offset)
{
	verify_method;

	__assert (offset < main_stack.size(), "Stack top offset overflow: %lu [max %lu]", offset, main_stack.size());
	return main_stack.at (main_stack.size() - (offset + 1));
}

calc_t& MMU::AStackFrame (int offset)
{
	verify_method;

	long final_offset = context.frame + offset;
	__assert (final_offset >= 0, "Stack frame offset underflow: %d [min %ld]",
			  offset, -context.frame);

	__assert (static_cast<size_t> (final_offset) < main_stack.size(), "Stack frame offset overflow: %d [max %ld]",
			  offset, main_stack.size() - context.frame);

	return main_stack.at (final_offset);
}

Processor::symbol_type& MMU::ASymbol (size_t hash)
{
	verify_method;

	auto sym_iter = CurrentBuffer().sym_table.find (hash);
	__assert (sym_iter != CurrentBuffer().sym_table.end(), "Unknown symbol (hash %p)", hash);

	return sym_iter ->second;
}

calc_t& MMU::ARegister (Register reg_id)
{
	verify_method;

	__assert (reg_id < R_MAX, "Invalid register #: %d [max %d]", reg_id, R_MAX);
	return CurrentBuffer().registers[reg_id];
}

DecodedCommand& MMU::ACommand (size_t ip)
{
	verify_method;

	__assert (ip < GetTextSize(), "IP overflow: %ld [max %ld]", ip, GetTextSize());
	return CurrentBuffer().commands.at (ip);
}

calc_t& MMU::AData (size_t addr)
{
	verify_method;

	__assert (addr < GetDataSize(), "Data offset overflow: %ld [max %ld]", addr, GetDataSize());
	return CurrentBuffer().data.at (addr);
}

Context& MMU::CurrentContext()
{
	verify_method;

	return context;
}

Buffer& MMU::CurrentBuffer()
{
	verify_method;

	return buffers[context.buffer];
}

size_t MMU::GetDataSize()
{
	verify_method;

	return CurrentBuffer().data.size();
}

size_t MMU::GetTextSize()
{
	verify_method;

	return CurrentBuffer().commands.size();
}

bool MMU::Verify_() const
{
	verify_statement (!context_stack.empty(), "Call stack underflow");
	verify_statement (context.buffer < buffers.Capacity(), "Invalid context buffer ID [%ld]", context.buffer);

	return 1;
}


} // namespace ProcessorImplementation