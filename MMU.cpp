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

		long final_offset_tmp = context.frame + offset;
		__assert (final_offset_tmp >= 0, "Stack frame offset underflow: %d [min %ld]",
		          offset, -context.frame);

		size_t final_offset = context.frame + offset;

		__assert (static_cast<size_t> (final_offset) < main_stack.size(), "Stack frame offset overflow: %lu [max %ld]",
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

		__assert (reg_id < R_MAX, "Invalid register #: %u [max %u]", reg_id, R_MAX);
		return CurrentBuffer().registers[reg_id];
	}

	DecodedCommand& MMU::ACommand (size_t ip)
	{
		verify_method;

		__assert (ip < GetTextSize(), "IP overflow: %lu [max %lu]", ip, GetTextSize());
		return CurrentBuffer().commands.at (ip);
	}

	calc_t& MMU::AData (size_t addr)
	{
		verify_method;

		__assert (addr < GetDataSize(), "Data offset overflow: %lu [max %lu]", addr, GetDataSize());
		return CurrentBuffer().data.at (addr);

	}

	void MMU::ReadStack (calc_t* image, size_t size)
	{
		verify_method;

		msg (E_INFO, E_VERBOSELIB, "Reading stack image (%p : %lu) -> global",
			 image, size);

		main_stack.clear();
		main_stack.reserve (size + 1);

		if (size)
		{
			calc_t* tmp_image = 0;
			__assert (tmp_image = image, "NULL stack image pointer");

			for (unsigned i = 0; i < size; ++i)
				main_stack.push_back (*tmp_image++);
		}
	}

	void MMU::ReadData (calc_t* image, size_t size)
	{
		verify_method;

		msg (E_INFO, E_VERBOSELIB, "Reading data image (%p : %lu) -> ctx %lu",
			 image, size, context.buffer);

		std::vector<calc_t>& data_dest = CurrentBuffer().data;

		data_dest.clear();
		data_dest.reserve (size + 1);

		if (size)
		{
			calc_t* tmp_image = 0;
			__assert (tmp_image = image, "NULL data image pointer");

			for (unsigned i = 0; i < size; ++i)
				data_dest.push_back (*tmp_image++);
		}
	}

	void MMU::ReadSyms (symbol_map* image)
	{
		verify_method;

		msg (E_INFO, E_VERBOSELIB, "Attaching symbol map (%p) -> ctx %lu",
			 image, context.buffer);

		symbol_map& dest = CurrentBuffer().sym_table;

		if (image && !image ->empty())
		{
			dest = std::move (*image);
		}

		else
		{
			msg (E_WARNING, E_DEBUGAPP, "Not attaching empty or NULL symbol map");
			dest.clear();
		}
	}

	void MMU::ReadText (DecodedCommand* image, size_t size)
	{
		verify_method;

		msg (E_INFO, E_VERBOSELIB, "Reading text image (%p : %lu) -> ctx %lu",
			 image, size, context.buffer);

		std::vector<DecodedCommand>& text_dest = CurrentBuffer().commands;

		text_dest.clear();
		text_dest.reserve (size + 1);

		if (size)
		{
			DecodedCommand* tmp_image = 0;
			__assert (tmp_image = image, "NULL text image pointer");

			for (unsigned i = 0; i < size; ++i)
				text_dest.push_back (*tmp_image++);
		}
	}

	void MMU::ResetBuffers (size_t ctx_id)
	{
		verify_method;

		msg (E_INFO, E_VERBOSELIB, "Resetting images in context %lu", ctx_id);

		__assert (ctx_id < buffers.Capacity(), "Invalid given buffer ID [%lu]: max %lu");
		InternalContextBuffer& buffer_dest = buffers[ctx_id];

		buffer_dest.commands.clear();
		buffer_dest.data.clear();
		buffer_dest.sym_table.clear();

		for (size_t reg_id = 0; reg_id < R_MAX; ++reg_id)
			buffer_dest.registers[reg_id] = 0;
	}

	void MMU::ResetEverything()
	{
		verify_method;

		main_stack.clear();
		context_stack.clear();

		for (size_t buf_id = 0; buf_id < BUFFER_NUM; ++buf_id)
			ResetBuffers (buf_id);
	}

	void MMU::SaveContext()
	{
		// Save buffer before doing anything else (we may get called with empty stack)
		context_stack.push_back (context);

		verify_method;

		msg (E_INFO, E_DEBUGAPP, "Saving execution context");
		DumpContext (context);

		// Update current context
		++context.depth;
		context.frame = main_stack.size() - 1;
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
		// Increment buffer before doing anything else (we may get called with buffer ID -1)
		SaveContext();
		++context.buffer;

		verify_method;

		ClearContext();
		msg (E_INFO, E_DEBUGAPP, "Switched to next context buffer [%lu]", context.buffer);
	}

	void MMU::AllocContextBuffer()
	{
		// Don't call verify_method
		NextContextBuffer();

		verify_method;

		ResetBuffers (context.buffer);
	}

	void MMU::RestoreContext()
	{
		verify_method;

		msg (E_INFO, E_DEBUGAPP, "Restoring execution context");

		context = context_stack.back();
		context_stack.pop_back();
		DumpContext (context);
	}

	void MMU::DumpContext (Context& w_context)
	{
		// See NextContextBuffer() for info on why don't we call verify_method

		msg (E_INFO, E_DEBUGAPP, "Ctx [%ld]: IP [%lu] FL [%lu] STACK [T %lu F %lu] DEPTH [%lu]",
			 w_context.buffer, w_context.ip, w_context.flags, main_stack.size(),
			 w_context.frame, w_context.depth);

		// Placeholder context can be dumped as well - handle it
		if (w_context.buffer < buffers.Capacity())
		{
			calc_t* registers = CurrentBuffer().registers;
			msg (E_INFO, E_DEBUGAPP, "Reg: A [%lg] B [%lg] C [%lg] D [%lg] E [%lg] F [%lg]",
			     registers[R_A],
			     registers[R_B],
			     registers[R_C],
			     registers[R_D],
			     registers[R_E],
			     registers[R_F]);
		}

		else
			msg (E_INFO, E_DEBUGAPP, "Reg: N/A");

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

	size_t MMU::GetStackTop()
	{
		verify_method;

		return main_stack.size();
	}


	bool MMU::Verify_() const
	{
		verify_statement (!context_stack.empty(), "Possible call stack underflow or MMU uninitialized");

		if (!main_stack.empty())
			verify_statement (context.frame < main_stack.size(), "Invalid stack frame [%lu]: top at %lu",
			                  context.frame, main_stack.size());

		verify_statement (context.buffer < buffers.Capacity(), "Invalid context buffer ID [%lu]: max %lu",
						  context.buffer, buffers.Capacity());

		if (!buffers[context.buffer].commands.empty())
			verify_statement (context.ip < buffers[context.buffer].commands.size(),
			                  "Invalid instruction pointer [%lu]: max %lu",
			                  context.ip, buffers[context.buffer].commands.size());

		return 1;
	}


} // namespace ProcessorImplementation