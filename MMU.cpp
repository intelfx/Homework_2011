#include "stdafx.h"
#include "MMU.h"

namespace ProcessorImplementation
{
	using namespace Processor;

	MMU::MMU() :
	main_stack(),
	integer_stack(),
	fp_stack(),
	current_stack (0),
	frame_stack (0),
	current_stack_type (Value::V_MAX),
	frame_stack_type (Value::V_MAX),
	context_stack(),
	buffers(),
	context()
	{
	}

	MMU::~MMU()
	{
		for (size_t buf_id = 0; buf_id < BUFFER_NUM; ++buf_id)
			free (buffers[buf_id].bytepool_data);
	}

	void MMU::OnAttach()
	{
		ResetEverything();
	}

	void MMU::ClearStacks()
	{
		main_stack.clear();
		integer_stack.clear();
		fp_stack.clear();
	}

	void MMU::AllocBytes (size_t base, size_t length)
	{
		size_t destination_size = base + length;
		InternalContextBuffer& icb = CurrentBuffer();

		if (destination_size > icb.bytepool_length)
		{
			msg (E_INFO, E_DEBUG, "Reallocating bytepool memory [ctx %zd] %p:%zu -> %zu to accomplish access at %zu+%zu",
				 context.buffer, icb.bytepool_data, icb.bytepool_length, destination_size, base, length);

			char* new_bytepool = reinterpret_cast<char*> (realloc (icb.bytepool_data, destination_size));
			__assert (new_bytepool,
					  "Failed to reallocate bytepool [ctx %zd] %p:%zu -> %zu",
					  context.buffer, icb.bytepool_data, icb.bytepool_length, length);

			icb.bytepool_data = new_bytepool;
			icb.bytepool_length = destination_size;
		}
	}

	void MMU::SetBytes (size_t base, size_t length, const char* data)
	{
		AllocBytes (base, length);
		memcpy (CurrentBuffer().bytepool_data + base, data, length);
	}

	void MMU::InternalWrStackPointer (std::vector<calc_t>** ptr, Value::Type type)
	{
		switch (type)
		{
			case Value::V_FLOAT:
				*ptr = &fp_stack;
				break;

			case Value::V_INTEGER:
				*ptr = &integer_stack;
				break;

			case Value::V_MAX:
				*ptr = &main_stack;
				break;

			default:
				__asshole ("Switch error");
				break;
		}
	}

	void MMU::SelectStack (Value::Type type)
	{
		if (current_stack && frame_stack && (current_stack_type == type))
			return;

		current_stack = 0;
		frame_stack = 0;

		if ((type == Value::V_MAX) ^ (current_stack_type == Value::V_MAX))
		{
			ClearStacks();

			if (!main_stack.empty())
			{
				if (type == Value::V_MAX)
					msg (E_INFO, E_DEBUG, "Reverting to single-stack implementation");

				else
					msg (E_INFO, E_DEBUG, "Entering multi-stack implementation");
			}
		}

		current_stack_type = type;
		frame_stack_type = (type == Value::V_MAX) ? Value::V_MAX : Value::V_INTEGER;

		InternalWrStackPointer (&current_stack, current_stack_type);
		InternalWrStackPointer (&frame_stack, frame_stack_type);
	}

	void MMU::AlterStackTop (short int offset)
	{
		verify_method;

		long new_size = current_stack ->size() + offset;
		__assert (new_size >= 0, "Unable to alter stack top by %hd: current top is %zu",
				  offset, current_stack ->size());

		current_stack ->resize (new_size);
	}

	calc_t& MMU::AStackTop (size_t offset)
	{
		verify_method;

		__assert (offset < current_stack ->size(), "Stack top offset overflow: %zu [max %zu]",
				  offset, current_stack ->size());
		return *(current_stack ->rbegin() + offset);
	}

	calc_t& MMU::AStackFrame (int offset)
	{
		verify_method;

		long final_offset_tmp = context.frame + offset;

		__assert (final_offset_tmp >= 0, "Stack frame offset underflow: %d [frame %zu]",
				  offset, context.frame);

		size_t final_offset = context.frame + offset;

		__assert (final_offset < frame_stack ->size(), "Stack frame offset overflow: %d [frame %zu | top %zu]",
				  offset, context.frame, frame_stack ->size());

		return frame_stack ->at (final_offset);
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

	char* MMU::ABytepool (size_t offset)
	{
		verify_method;
		InternalContextBuffer& icb = CurrentBuffer();

		__assert (offset < icb.bytepool_length,
				  "Cannot reference bytepool address %zu: allocated size %zu",
			offset, icb.bytepool_length);

		return icb.bytepool_data + offset;
	}

	void MMU::ReadStack (calc_t* image, size_t size, bool selected_only)
	{
		verify_method;

		__assert (image, "NULL stack image pointer");

		if (selected_only)
		{
			msg (E_INFO, E_VERBOSE, "Reading stack image (%p : %zu) -> global type \"%s\"",
				 image, size, ProcDebug::ValueType_ids[current_stack_type]);

			current_stack ->clear();
			current_stack ->reserve (size + 1);

			for (size_t i = 0; i < size; ++i)
				current_stack ->push_back (*image++);
		}

		else
		{
			msg (E_INFO, E_VERBOSE, "Reading multiple stack images (%p : %zu) -> global",
				 image, size);

			ClearStacks();

			for (size_t i = 0; i < size; ++i)
			{
				const calc_t& element = *image;

				switch (element.type)
				{
					case Value::V_FLOAT:
						fp_stack.push_back (element);
						break;

					case Value::V_INTEGER:
						integer_stack.push_back (element);
						break;

					case Value::V_MAX:
						__asshole ("Invalid input element type");
						break;

					default:
						__asshole ("Switch error");
						break;
				}
			}
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

		msg (E_INFO, E_DEBUG, "Attaching symbol map (size %zu) -> ctx %zu", syms.size(), context.buffer);
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
			auto symbol_record = i ->second;
			const Symbol* sptr = &symbol_record.second;
			size_t nlen = symbol_record.first.length();
			const char* name = symbol_record.first.c_str();

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

			msg (E_INFO, E_DEBUG, "Symbol load: name \"%s\" -> %s",
				 name.c_str(), (ProcDebug::PrintReference (sym ->ref), ProcDebug::debug_buffer));

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
			__assert (image, "NULL text image pointer");

			for (unsigned i = 0; i < size; ++i)
				text_dest.push_back (*image++);
		}
	}

	size_t MMU::InternalWriteStack (const std::vector<calc_t>* stack, Processor::calc_t* pointer) const
	{
		for (std::vector<calc_t>::const_iterator i = stack ->begin(); i != stack ->end(); ++i)
			*pointer++ = *i;

		return stack ->size();
	}

	void MMU::WriteStack (Processor::calc_t* image, bool selected_only) const
	{
		verify_method;

		__assert (image, "Invalid destination image");

		if (selected_only)
		{
			msg (E_INFO, E_DEBUG, "Writing stack image type \"%s\" [ctx %zu] -> %p",
				 ProcDebug::ValueType_ids[current_stack_type], context.buffer, image);

			for (std::vector<calc_t>::const_iterator i = current_stack ->begin(); i != current_stack ->end(); ++i)
				*image++ = *i;
		}

		else
		{
			msg (E_INFO, E_DEBUG, "Writing all stack images [ctx %zu] -> %p", context.buffer, image);

			image += InternalWriteStack (&integer_stack, image);
			image += InternalWriteStack (&fp_stack, image);
		}
	}

	void MMU::ResetBuffers (size_t ctx_id)
	{
		if (!context_stack.empty())
		{
			verify_method;

			msg (E_INFO, E_DEBUG, "Resetting images in context %zu", ctx_id);
			__assert (ctx_id < buffers.Capacity(), "Invalid given buffer ID [%zu]: max %zu", ctx_id, buffers.Capacity());
		}

		InternalContextBuffer& buffer_dest = buffers[ctx_id];

		buffer_dest.commands.clear();
		buffer_dest.data.clear();
		buffer_dest.sym_table.clear();

		free (buffer_dest.bytepool_data);
		buffer_dest.bytepool_data = 0;
		buffer_dest.bytepool_length = 0;

		for (size_t reg_id = 0; reg_id < R_MAX; ++reg_id)
			buffer_dest.registers[reg_id] = Value();
	}

	void MMU::ResetEverything()
	{
		SelectStack (Value::V_MAX); // will clean up the stacks
		context_stack.clear();

		for (size_t buf_id = 0; buf_id < BUFFER_NUM; ++buf_id)
			ResetBuffers (buf_id);

		ClearContext();
		context.buffer = -1;
	}

	void MMU::SaveContext()
	{
		bool log_save = !context_stack.empty();
		if (log_save)
		{
			verify_method;

			msg (E_INFO, E_DEBUG, "Saving execution context");
			InternalDumpCtx (context);
		}

		context_stack.push_back (context);

		// Update current context
		++context.depth;
		context.frame = frame_stack ->size();

		if (log_save)
			InternalDumpCtx (context);
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
			msg (E_INFO, E_DEBUG, "MMU reset (switch to context buffer 0): using %s implementation",
				 (current_stack_type == Value::V_MAX) ? "single-stack" : "multi-stack");

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

		msg (E_INFO, E_DEBUG, "Restoring execution context", context.buffer);

		InternalDumpCtx (context);
		InternalDumpCtx (context_stack.back());

		if (context_stack.size() == 1)
			msg (E_INFO, E_DEBUG, "MMU back to uninitialized state");

		context = context_stack.back();
		context_stack.pop_back();
	}

	void MMU::InternalDumpCtx (const Context& w_context) const
	{
		msg (E_INFO, E_DEBUG, "Ctx [%zd]: IP [%zu] FL [%zu] FRAME [\"%s\" frm %zu] MAIN [\"%s\" top %zu] DEPTH [%zu]",
			 w_context.buffer, w_context.ip, w_context.flags,
			 ProcDebug::ValueType_ids[frame_stack_type], w_context.frame,
			 ProcDebug::ValueType_ids[current_stack_type], GetStackTop(),
			 w_context.depth);

// 		if (w_context.buffer < buffers.Capacity())
// 		{
// 			const calc_t* registers = CurrentBuffer().registers;
// 			msg (E_INFO, E_DEBUG, "Reg: A [%lg] B [%lg] C [%lg|%zu] D [%lg] E [%lg] F [%lg]", /* a little unroll */
// 				 registers[R_A],
// 				 registers[R_B],
// 				 registers[R_C],
// 				 registers[R_D],
// 				 registers[R_E],
// 				 registers[R_F]);
// 		}
//
// 		else
// 			msg (E_INFO, E_DEBUG, "Reg: N/A");

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

	size_t MMU::GetPoolSize() const
	{
		verify_method;

		return CurrentBuffer().bytepool_length;
	}

	size_t MMU::GetStackTop() const
	{
		verify_method;

		return current_stack ->size();
	}

	bool MMU::_Verify() const
	{
		verify_statement (current_stack, "No stack selected");
		verify_statement (frame_stack, "No auxiliary stack selected");

		if (context.buffer != static_cast<size_t> (-1))
		{
			verify_statement (!context_stack.empty(), "Call/context stack underflow");

			verify_statement (context.buffer < buffers.Capacity(), "Invalid context buffer ID [%zd]: max %zu",
							  context.buffer, buffers.Capacity());

			if (!buffers[context.buffer].commands.empty())
				verify_statement (context.ip < buffers[context.buffer].commands.size(),
								  "Invalid instruction pointer [%zu]: max %zu",
								  context.ip, buffers[context.buffer].commands.size());

		}

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
			__verify (context.frame + ref.address < frame_stack ->size(),
					  "Invalid reference [FRAME:%zu] : frame at %zu | top at %zu", ref.address, context.frame, frame_stack ->size());
			break;


		case S_FRAME_BACK:
			__verify (context.frame >= ref.address,
					  "Invalid reference [BACKFRAME:%zu] : frame at %zu | top at %zu", ref.address, context.frame, frame_stack ->size());
			break;

		case S_BYTEPOOL:
			__verify (ref.address < CurrentBuffer().bytepool_length,
					  "Invalid reference [BYTEPOOL:%zx] : allocated %zx bytes", ref.address, CurrentBuffer().bytepool_length);
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
