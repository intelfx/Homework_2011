#include "stdafx.h"
#include "MMU.h"

namespace ProcessorImplementation
{
	using namespace Processor;

	void MMU::OnAttach()
	{
		ResetEverything();
	}

	void MMU::AlterStackTop (short int offset)
	{
		verify_method;

		long new_size = main_stack.size() + offset;
		__assert (new_size >= 0, "Unable to alter stack top by %hd: current top is %zu",
				  offset, main_stack.size());

		main_stack.resize (new_size, 0);
	}

	calc_t& MMU::AStackTop (size_t offset)
	{
		verify_method;

		__assert (offset < main_stack.size(), "Stack top offset overflow: %zu [max %zu]", offset, main_stack.size());
		return main_stack.at (main_stack.size() - (offset + 1));
	}

	calc_t& MMU::AStackFrame (int offset)
	{
		verify_method;

		long final_offset_tmp = context.frame + offset;
		__assert (final_offset_tmp >= 0, "Stack frame offset underflow: %d [min %ld]",
				  offset, -context.frame);

		size_t final_offset = context.frame + offset;

		__assert (final_offset < main_stack.size(), "Stack frame offset overflow: %zu [max %ld]",
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

	Command& MMU::ACommand (size_t ip)
	{
		verify_method;

		__assert (ip < GetTextSize(), "IP overflow: %zu [max %zu]", ip, GetTextSize());
		return CurrentBuffer().commands.at (ip);
	}

	calc_t& MMU::AData (size_t addr)
	{
		verify_method;

		__assert (addr < GetDataSize(), "Data offset overflow: %zu [max %zu]", addr, GetDataSize());
		return CurrentBuffer().data.at (addr);

	}

	void MMU::ReadStack (calc_t* image, size_t size)
	{
		verify_method;

		msg (E_INFO, E_VERBOSE, "Reading stack image (%p : %zu) -> global",
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

	void MMU::WriteData (calc_t* image) const
	{
		verify_method;

		msg (E_INFO, E_VERBOSE, "Writing data image (ctx %zu length %zu) -> %p",
			 context.buffer, GetDataSize(), image);

		__assert (image, "Invalid image destination");

		std::vector<calc_t>::const_iterator src = CurrentBuffer().data.begin();

		while (src != CurrentBuffer().data.end())
			*image++ = *src++;
	}

	void MMU::WriteText (Command* image) const
	{
		verify_method;

		msg (E_INFO, E_VERBOSE, "Writing text image (ctx %zu length %zu) -> %p",
			 context.buffer, GetTextSize(), image);

		__assert (image, "Invalid image destination");

		std::vector<Command>::const_iterator src = CurrentBuffer().commands.begin();

		while (src != CurrentBuffer().commands.end())
			*image++ = *src++;
	}

	void MMU::ReadData (calc_t* image, size_t size)
	{
		verify_method;

		msg (E_INFO, E_VERBOSE, "Reading data image (%p : %zu) -> ctx %zu",
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

	void MMU::InsertSyms (symbol_map && syms)
	{
		verify_method;

		msg (E_INFO, E_VERBOSE, "Attaching symbol map (size %zu) -> ctx %zu", syms.size(), context.buffer);
		CurrentBuffer().sym_table = std::move (syms);
	}

	void MMU::WriteSyms (void** image, size_t* bytes, size_t* count) const
	{
		verify_method;

		msg (E_INFO, E_VERBOSE, "Serializing symbol map (ctx %zu size %zu)",
			 context.buffer, CurrentBuffer().sym_table.size());

		const symbol_map& src = CurrentBuffer().sym_table;

		size_t i_cnt = src.size(), i_bytes = (sizeof (Symbol) + sizeof (size_t) + STATIC_LENGTH) * i_cnt;
		size_t i_idx = 0;
		char* i_img = reinterpret_cast<char*> (malloc (i_bytes));

		for (symbol_map::const_iterator i = src.begin(); i != src.end(); ++i)
		{
			const Symbol* sptr = &i ->second.second;
			size_t nlen = i ->second.first.length();
			const char* name = i ->second.first.c_str();

			memcpy (i_img, sptr, sizeof (Symbol));
			i_img += sizeof (Symbol);
			i_idx += sizeof (Symbol);

			*reinterpret_cast<size_t*> (i_img) = nlen;
			i_img += sizeof (size_t);
			i_idx += sizeof (size_t);

			memcpy (i_img, name, nlen);
			i_img += nlen;
			i_idx += nlen;
		}


		*image = realloc (i_img, i_idx);
		*bytes = i_idx;
		*count = i_cnt;
	}


	void MMU::ReadSyms (void* image, size_t size)
	{
		verify_method;

		msg (E_INFO, E_VERBOSE, "Deserializing symbol image (%p : %zu) -> ctx %zu",
			 image, size, context.buffer);
		symbol_map& dest = CurrentBuffer().sym_table;
		dest.clear();

		char* current_ptr = reinterpret_cast<char*> (image);

		/*
		 * The format:
		 * Symbol		symbol record
		 * size_t		name length
		 * char[]		name (non-ASCIZ)
		 */

		for (; size != 0; --size)
		{
			/* deserialize */
			Symbol* sym = reinterpret_cast<Symbol*> (current_ptr);
			current_ptr += sizeof (Symbol);

			size_t length = *reinterpret_cast<size_t*> (current_ptr);
			current_ptr += sizeof (size_t);

			/* prepare name - we must correctly handle unspecified names */
			std::string name;

			if (length)
			{
				name.assign (current_ptr, length);
				current_ptr += length;
			}

			else
			{
				char tmp_ptr[STATIC_LENGTH];
				snprintf (tmp_ptr, STATIC_LENGTH, "__unk_%zx", sym ->hash);
				name.assign (tmp_ptr);
			}

			/* commit */
			__assert (sym ->is_resolved, "Deserialize failed: unresolved symbol \"%s\"", name.c_str());

			ProcDebug::PrintReference (sym ->ref);
			msg (E_INFO, E_DEBUG, "Symbol load: name \"%s\" -> %s", name.c_str(), ProcDebug::debug_buffer);

			dest.insert (std::make_pair (sym ->hash, std::make_pair (std::move (name), *sym)));
		}
	}

	void MMU::ReadText (Command* image, size_t size)
	{
		verify_method;

		msg (E_INFO, E_VERBOSE, "Reading text image (%p : %zu) -> ctx %zu",
			 image, size, context.buffer);

		std::vector<Command>& text_dest = CurrentBuffer().commands;

		text_dest.clear();
		text_dest.reserve (size + 1);

		if (size)
		{
			Command* tmp_image = 0;
			__assert (tmp_image = image, "NULL text image pointer");

			for (unsigned i = 0; i < size; ++i)
				text_dest.push_back (*tmp_image++);
		}
	}

	void MMU::WriteStack (calc_t* image) const
	{
		verify_method;

		msg (E_INFO, E_DEBUG, "Writing stack image [ctx %zu] -> %p", context.buffer, image);
		__assert (image, "Invalid destination image");

		for (std::vector<calc_t>::const_iterator i = main_stack.begin(); i != main_stack.end(); ++i)
		{
			*image++ = *i;
		}
	}

	void MMU::ResetBuffers (size_t ctx_id)
	{
		if (!context_stack.empty())
		{
			verify_method;

			msg (E_INFO, E_VERBOSE, "Resetting images in context %zu", ctx_id);
			__assert (ctx_id < buffers.Capacity(), "Invalid given buffer ID [%zu]: max %zu", ctx_id, buffers.Capacity());
		}

		InternalContextBuffer& buffer_dest = buffers[ctx_id];

		buffer_dest.commands.clear();
		buffer_dest.data.clear();
		buffer_dest.sym_table.clear();

		for (size_t reg_id = 0; reg_id < R_MAX; ++reg_id)
			buffer_dest.registers[reg_id] = 0;
	}

	void MMU::ResetEverything()
	{
		main_stack.clear();
		context_stack.clear();

		for (size_t buf_id = 0; buf_id < BUFFER_NUM; ++buf_id)
			ResetBuffers (buf_id);

		ClearContext();
		context.buffer = -1;
	}

	void MMU::SaveContext()
	{
		if (!context_stack.empty())
		{
			verify_method;

			msg (E_INFO, E_DEBUG, "Saving execution context");
			InternalDumpCtx (context);
		}

		context_stack.push_back (context);

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
		SaveContext();
		ClearContext();
		++context.buffer;

		if (context.buffer)
			msg (E_INFO, E_DEBUG, "Switched to next context buffer [%zu]", context.buffer);

		else
			msg (E_INFO, E_DEBUG, "MMU reset (switch to context buffer 0)");

		verify_method;
	}

	void MMU::AllocContextBuffer()
	{
		NextContextBuffer();

		ResetBuffers (context.buffer);
	}

	void MMU::RestoreContext()
	{
		verify_method;

		msg (E_INFO, E_DEBUG, "Restoring execution context [was %zd]", context.buffer);

		InternalDumpCtx (context_stack.back());

		if (context_stack.size() == 1)
			msg (E_INFO, E_DEBUG, "MMU back to uninitialized state");

		context = context_stack.back();
		context_stack.pop_back();
	}

	void MMU::InternalDumpCtx (const Context& w_context) const
	{
		msg (E_INFO, E_DEBUG, "Ctx [%zd]: IP [%zu] FL [%zu] STACK [T %zu F %zu] DEPTH [%zu]",
			 w_context.buffer, w_context.ip, w_context.flags, main_stack.size(),
			 w_context.frame, w_context.depth);

		// Placeholder context can be dumped as well - handle it
		if (w_context.buffer < buffers.Capacity())
		{
			const calc_t* registers = CurrentBuffer().registers;
			msg (E_INFO, E_DEBUG, "Reg: A [%lg] B [%lg] C [%lg] D [%lg] E [%lg] F [%lg]", /* a little unroll */
				 registers[R_A],
				 registers[R_B],
				 registers[R_C],
				 registers[R_D],
				 registers[R_E],
				 registers[R_F]);
		}

		else
			msg (E_INFO, E_DEBUG, "Reg: N/A");

	}

	void MMU::DumpContext() const
	{
		verify_method;

		InternalDumpCtx (context);
	}

	Context& MMU::GetContext()
	{
		verify_method;

		return context;
	}

	void MMU::InsertData (calc_t value)
	{
		verify_method;

		CurrentBuffer().data.push_back (value);
	}

	void MMU::InsertText (const Command& command)
	{
		verify_method;

		CurrentBuffer().commands.push_back (command);
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

	size_t MMU::GetDataSize() const
	{
		verify_method;

		return CurrentBuffer().data.size();
	}

	size_t MMU::GetTextSize() const
	{
		verify_method;

		return CurrentBuffer().commands.size();
	}

	size_t MMU::GetStackTop() const
	{
		verify_method;

		return main_stack.size();
	}


	bool MMU::_Verify() const
	{
		verify_statement (!context_stack.empty(), "Possible call stack underflow or MMU uninitialized");

		if (!main_stack.empty())
			verify_statement (context.frame < main_stack.size(), "Invalid stack frame [%zu]: top at %zu",
							  context.frame, main_stack.size());

		verify_statement (context.buffer < buffers.Capacity(), "Invalid context buffer ID [%zd]: max %zu",
						  context.buffer, buffers.Capacity());

		if (!buffers[context.buffer].commands.empty())
			verify_statement (context.ip < buffers[context.buffer].commands.size(),
							  "Invalid instruction pointer [%zu]: max %zu",
							  context.ip, buffers[context.buffer].commands.size());

		return 1;
	}

	void MMU::VerifyReference (const Reference::Direct& ref) const
	{
		switch (ref.type)
		{
		case S_CODE:
			__verify (ref.address < GetTextSize(),
					  "Invalid reference [TEXT:%zu] : limit %zu", ref.address, GetTextSize());
			break;

		case S_DATA:
			__verify (ref.address < GetDataSize(),
					  "Invalid reference [DATA:%zu] : limit %zu", ref.address, GetDataSize());
			break;

		case S_REGISTER:
			__verify (ref.address < R_MAX,
					  "Invalid reference [REG:%zu] : max register ID %zu", ref.address, R_MAX - 1);
			break;

		case S_FRAME:
			__verify (context.frame + ref.address < main_stack.size(),
					  "Invalid reference [FRAME:%zu] : offset limit %zu", main_stack.size() - context.frame);
			break;


		case S_FRAME_BACK:
			__verify (context.frame - ref.address < main_stack.size(),
					  "Invalid reference [BACKFRAME:%zu] : offset limit %zu", context.frame + 1);
			break;

		case S_NONE:
		case S_MAX:
		default:
			__asshole ("Switch error");
			break;
		}
	}
} // namespace ProcessorImplementation
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
