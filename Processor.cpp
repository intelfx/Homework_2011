#include "stdafx.h"
#include "Processor.h"

ImplementDescriptor (Processor, "dumb processor", MOD_APPMODULE, Processor);


enum COMMANDS
{
	C_INIT = 0,
	C_PUSH,
	C_POP,
	C_TOP,

	C_ADD,
	C_SUB,
	C_MUL,
	C_DIV,

	C_ANAL,
	C_CMP,
	C_SWAP,
	C_DUP,

	C_JE,
	C_JNE,
	C_JA,
	C_JNA,
	C_JAE,
	C_JNAE,
	C_JB,
	C_JNB,
	C_JBE,
	C_JNBE,

	C_JMP,
	C_CALL,
	C_RET,

	C_DFL,
	C_SNFC,
	C_CNFC,

	C_CSWITCH,
	C_EXEC,
	C_INVD,

	C_LASM,
	C_LBIN,
	C_WASM,
	C_WBIN,

	C_QUIT,

	C_MAX,
	C_NONE
};

const char* Processor::commands_list[C_MAX] =
{
	"init",
	"push",
	"pop",
	"top",

	"add",
	"sub",
	"mul",
	"div",

	"anal", // Fuck yeah
	"cmp",
	"swap",
	"dup",

	"je",
	"jne",
	"ja",
	"jna",
	"jae",
	"jnae",
	"jb",
	"jnb",
	"jbe",
	"jnbe",

	"jmp",
	"call",
	"ret",

	"dumpfl",
	"snfc",
	"cnfc",

	"cswitch",
	"exec",
	"invd",

	"lasm",
	"lbin",
	"wasm",
	"wbin",

	"quit"
};

const char* Processor::commands_desc[C_MAX] =
{
	"Stack: initialize stack and execution environment",
	"Stack: push a value onto the stack",
	"Stack: remove a value from the stack",
	"Stack: peek a value from the stack",

	"Arithmetic: addition",
	"Arithmetic: subtraction (minuend on top)",
	"Arithmetic: multiplication",
	"Arithmetic: division (dividend on top)",

	"Accumulator: analyze top of the stack",
	"Accumulator: compare two values on the stack (R/O subtraction)",
	"Accumulator: swap two values on the stack",
	"Accumulator: duplicate the value on the stack",

	"Address: jump if ZERO",
	"Address: jump if not ZERO",
	"Address: jump if ABOVE",
	"Address: jump if not ABOVE",
	"Address: jump if ABOVE or EQUAL",
	"Address: jump if not ABOVE or EQUAL",
	"Address: jump if BELOW",
	"Address: jump if not BELOW",
	"Address: jump if BELOW or EQUAL",
	"Address: jump if not BELOW or EQUAL",

	"Address: unconditional jump",
	"Address: unconditional call",
	"Address: unconditional return",

	"Flags: dump flag register",
	"Flags: set No-Flag-Change flag",
	"Flags: clear No-Flag-Change flag",

	"Management: switch to next context (useful only in decoding mode)",
	"Management: execute next context buffer",
	"Management: clear (invalidate) next context buffer",

	"Management: load next context buffer [ASM]",
	"Management: load next context buffer [BIN]",
	"Management: write next context buffer [ASM]",
	"Management: write next context buffer [BIN]",

	"Management: stop execution/quit context"
};

Processor::CommandTraits Processor::commands_traits[C_MAX] =
{
	{A_NONE},
	{A_VALUE},
	{A_NONE},
	{A_NONE},

	{A_NONE},
	{A_NONE},
	{A_NONE},
	{A_NONE},

	{A_NONE},
	{A_NONE},
	{A_NONE},
	{A_NONE},

	{A_REFERENCE},
	{A_REFERENCE},
	{A_REFERENCE},
	{A_REFERENCE},
	{A_REFERENCE},
	{A_REFERENCE},
	{A_REFERENCE},
	{A_REFERENCE},
	{A_REFERENCE},
	{A_REFERENCE},

	{A_REFERENCE},
	{A_REFERENCE},
	{A_NONE},

	{A_NONE},
	{A_NONE},
	{A_NONE},

	{A_NONE},
	{A_NONE},
	{A_NONE},

	{A_NONE},
	{A_NONE},
	{A_NONE},
	{A_NONE},

	{A_NONE}
};

// BIN_IFX
Processor::VersionSignature Processor::expect_sig = {'XFI_NIB', {{0x00, 0x01}}, 0};

// !SEC_SYM
const unsigned long long Processor::symbols_section_sig = 'MYS_CES';

// !SEC_CMD
const unsigned long long Processor::command_section_sig = 'DMC_CES';

Processor::Processor() :
state(),
bin_dump_file (0),
asm_dump_file (0)
{
	for (unsigned i = 0; i < C_MAX; ++i)
	{
		__assert (commands_list[i] && commands_desc[i], "Invalid command entry for %d", i);
		commands_index.insert (std::make_pair (commands_list[i], i));
	}

	SetFilenamePrefix ("default");
	InitContexts();
}

Processor::~Processor()
{
	free (bin_dump_file);
	free (asm_dump_file);
}

void Processor::CheckStream (FILE* stream)
{
	__sassert (stream && !ferror (stream), "Invalid stream");
}

bool Processor::ReadSignature (FILE* stream)
{
	VersionSignature file_sig;
	fread (&file_sig, sizeof (file_sig), 1, stream);

	__sassert (file_sig.magic == expect_sig.magic, "FILE SIGNATURE MISMATCH: READ %p EXPECT %p",
	           file_sig.magic, expect_sig.magic);

	smsg (E_INFO, E_VERBOSELIB, "File signature matched: READ %p", file_sig.magic);

	__sassert (file_sig.is_sparse == expect_sig.is_sparse, "SPARSE MODE MISMATCH: READ %s EXPECT %s",
	           file_sig.is_sparse ? "sparse" : "compact",
	           expect_sig.is_sparse ? "sparse" : "compact");

	smsg (E_INFO, E_VERBOSELIB, "File sparse mode: %s", file_sig.is_sparse ? "sparse" : "compact");

	__sassert (file_sig.version.major == expect_sig.version.major,
	           "FILE MAJOR VERSION MISMATCH: READ [%d] EXPECT [%d]",
	           file_sig.version.major,
	           expect_sig.version.major);

	if (file_sig.version.minor == expect_sig.version.minor)
		smsg (E_INFO, E_VERBOSELIB, "File version matched: READ %p [%d:%d]",
		      file_sig.ver_raw,
		      file_sig.version.major,
		      file_sig.version.minor);

	else
		smsg (E_WARNING, E_VERBOSELIB, "FILE MINOR VERSION MISMATCH: READ [%d] EXPECT [%d]",
		      file_sig.version.minor,
		      expect_sig.version.minor);

	return file_sig.is_sparse;
}

void Processor::DumpFlags()
{
	msg (E_INFO, E_DEBUGAPP, "Dumping processor flags");

	msg (E_INFO, E_DEBUGAPP, "Processor flags: EIP [%d] EXIT [%d] NFC [%d]",
		 !!(state.flags & MASK(F_EIP)),
		 !!(state.flags & MASK(F_EXIT)),
		 !!(state.flags & MASK(F_NFC)));

	msg (E_INFO, E_DEBUGAPP, "Analyze flags: ZERO [%d] NEGATIVE [%d]",
		 !!(state.flags & MASK(F_ZERO)),
		 !!(state.flags & MASK(F_NEGATIVE)));
}

void Processor::DecodeInsertSymbol (symbol_map* map, const char* label, Symbol symbol,
									size_t* hash_ptr)
{
	std::string l (label);
	size_t h = std::hash<std::string>() (l);

	symbol.hash = h;
	if (hash_ptr) *hash_ptr = h;

	map ->insert (std::make_pair (h, std::make_pair (std::move (l), symbol)));
}

void Processor::BinaryInsertSymbol (symbol_map* map, const char* label, Symbol symbol)
{
	std::string l (label);
	size_t h = symbol.hash;

	map ->insert (std::make_pair (h, std::make_pair (std::move (l), symbol)));
}

void Processor::InitContexts()
{
	msg (E_INFO, E_VERBOSE, "Initializing system");

	context_stack.Reset();
	calc_stack.Reset();

	state.ip = 0;
	state.flags = MASK (F_EXIT);
	state.buffer = -1; // debug placeholder
}

void Processor::DoJump (const Processor::Reference& ref)
{
	size_t new_ip = -1;

	if (ref.is_symbol)
	{
		const std::pair<std::string, Symbol>& pair	= buffers[state.buffer].sym_table.find (ref.symbol_hash) ->second;
		const std::string& name						= pair.first;
		const Symbol& sym							= pair.second;

		__assert (sym.is_resolved, "Unresolved symbol \"%s\"", name.c_str());
		new_ip = sym.address;
	}

	else
	{
		new_ip = ref.address;
	}

	msg (E_INFO, E_DEBUGAPP, "Jumping to address %ld", new_ip);
	state.ip = new_ip;
}

void Processor::Analyze (calc_t arg)
{
	state.flags &= ~(MASK(F_NEGATIVE) | MASK(F_ZERO));

	if (arg < 0)
		state.flags |= MASK(F_NEGATIVE);

	else if (!(arg > 0))
		state.flags |= MASK(F_ZERO);
}

void Processor::SaveContext()
{
	msg (E_INFO, E_VERBOSE, "Saving context");
	msg (E_INFO, E_DEBUGAPP, "Context: IP [%p], FL [%p], BUF [%ld]",
		 state.ip, state.flags, state.buffer);

	context_stack.Push (state);
}

void Processor::PushContext()
{
	msg (E_INFO, E_VERBOSE, "Changing context");
	msg (E_INFO, E_DEBUGAPP, "Old context: IP [%p], FL [%p], BUF [%ld]",
	     state.ip, state.flags, state.buffer);

	ProcessorState oldc = state, newc;
	newc.ip = 0;
	newc.flags = 0;
	newc.buffer = oldc.buffer;

	state = newc;
	context_stack.Push (oldc);
}

void Processor::ClearContextBuffer()
{
	buffers[state.buffer].cmd_top = 0;
	buffers[state.buffer].sym_table.clear();
}

void Processor::NextContextBuffer()
{
	PushContext();
	++state.buffer;
	__assert (state.buffer < buffers.Capacity(), "Not enough execution buffers");
	msg (E_INFO, E_DEBUGAPP, "Switched to next execution context buffer");
}

void Processor::AllocContextBuffer()
{
	PushContext();
	++state.buffer;
	__assert (state.buffer < buffers.Capacity(), "Not enough execution buffers");

	ClearContextBuffer();
	msg (E_INFO, E_DEBUGAPP, "New execution context buffer allocated");
}

void Processor::RestoreContext()
{
	msg (E_INFO, E_VERBOSE, "Restoring context");

	state = context_stack.Pop();

	msg (E_INFO, E_VERBOSE, "Restored context: IP [%p], FL [%p], BUF [%ld]",
	     state.ip, state.flags, state.buffer);
}

void Processor::DecodeLinkSymbols (Processor::DecodedSet& set)
{
	msg (E_INFO, E_DEBUGAPP, "Linking mentioned symbols: %d", set.symbols.size());

	for (hashed_symbol& symbol: set.symbols)
	{
		Symbol& linked_desc		= symbol.second.second;
		const std::string& name	= symbol.second.first;
		size_t hash				= symbol.first;
		size_t address			= buffers[state.buffer].cmd_top;
		symbol_map& sym_table	= buffers[state.buffer].sym_table;
		auto existing_record	= sym_table.find (hash);

		// If symbol is defined here, check for multiple definitions and link.
		if (linked_desc.is_resolved)
		{
			msg (E_INFO, E_DEBUGAPP, "Definition of symbol \"%s\" (type %d): assigning address %ld",
			     name.c_str(), linked_desc.type, address);

			linked_desc.address = address;

			if (existing_record != sym_table.end())
			{
				// If we have forward-references, check them and resolve the symbol.
				__verify (!existing_record ->second.second.is_resolved,
				          "Symbol redefinition (previous at address %ld)",
				          existing_record ->second.second.address);

				// Resolve the symbol by assigning to existing record.
				existing_record ->second.second = linked_desc;
			}
		}

		// If symbol is used here, check for existing definitions and verify types (now absent).
		else
		{
			msg (E_INFO, E_DEBUGAPP, "Usage of symbol \"%s\" (type %d)",
				 name.c_str(), linked_desc.type);

			linked_desc.address = -1;
		}

		// Post-actions
		if (existing_record == sym_table.end())
			sym_table.insert (symbol);
	}

	msg (E_INFO, E_DEBUGAPP, "Link completed");
}


Processor::DecodedSet Processor::DecodeCmd (char* buffer)
{
	char command[line_length], arg[line_length];
	unsigned short label_count = 0;
	DecodedSet set;

	memset (command, '\0', line_length);
	memset (arg, '\0', line_length);
	memset (&set.cmd, 0xDEADBEEF, sizeof (set.cmd));

	set.cmd.command = C_MAX;

	if (char* cmt = strchr (buffer, ';')) *cmt = '\0'; // Drop commentary
	else if (char* nl = strchr (buffer, '\n')) *nl = '\0'; // Drop newline (if no commentary)

	msg (E_INFO, E_DEBUGAPP, "Decoding \"%s\"", buffer);

	// Parse labels
	while (char* colon = strchr (buffer, ':'))
	{
		++label_count;

		Symbol label_sym;
		label_sym.type = S_LABEL;
		label_sym.address = -1;
		label_sym.is_resolved = 1;

		*colon++ = '\0';

		msg (E_INFO, E_DEBUGAPP, "Adding label \"%s\" as resolved", buffer);
		DecodeInsertSymbol (&set.symbols, buffer, label_sym, 0);

		buffer = colon; // Shift buffer
		while (isspace (*buffer)) ++buffer;
	}

	msg (E_INFO, E_DEBUGAPP, "Parsed labels: %d", label_count);


	// Parse command
	short arg_count = sscanf (buffer, "%s %s", command, arg);

	if (arg_count == EOF)
	{
		msg (E_INFO, E_DEBUGAPP, "Command: No command");
		set.cmd.command = C_NONE;

		set.cmd.ref.is_symbol = 0;
		set.cmd.ref.address = 0;
	}

	else
	{
		auto cmd_index_iter = commands_index.find (command);
		__verify (cmd_index_iter != commands_index.end(), "Unknown command: \"%s\"", command);

		set.cmd.command = cmd_index_iter ->second;

		msg (E_INFO, E_DEBUGAPP, "Command arguments: %d", --arg_count); // Decrement since it contains command itself

		switch (commands_traits[set.cmd.command].arg_type)
		{
		case A_NONE:
			__assert (arg_count == 0, "Invalid argument count: %d", arg_count);

			set.cmd.ref.is_symbol = 0;
			set.cmd.ref.address = 0;

			msg (E_INFO, E_DEBUGAPP, "Command: [%d] -> \"%s\"",
			     set.cmd.command, commands_list[set.cmd.command]);
			break;

		case A_REFERENCE:
			__assert (arg_count == 1, "Invalid argument count: %d", arg_count);

			if (sscanf (arg, "%ld", &set.cmd.ref.address))
			{
				set.cmd.ref.is_symbol = 0;

				msg (E_INFO, E_DEBUGAPP, "Command: [%d %p] -> \"%s\" %ld",
				     set.cmd.command, set.cmd.debug, commands_list[set.cmd.command], set.cmd.ref.address);
			}

			else
			{
				Symbol sym;
				memset (&sym, 0xDEADBEEF, sizeof (sym));

				sym.is_resolved = 0;
				sym.type = S_NONE;
				sym.address = -1;

				msg (E_INFO, E_DEBUGAPP, "Adding symbol \"%s\" as unresolved", arg);

				// Add symbol and write its hash to the command
				DecodeInsertSymbol (&set.symbols, arg, sym, &set.cmd.ref.symbol_hash);
				set.cmd.ref.is_symbol = 1;

				msg (E_INFO, E_DEBUGAPP, "Command: [%d %p] -> \"%s\" \"%s\"",
					 set.cmd.command,
				     set.cmd.debug,
				     commands_list[set.cmd.command],
				     set.symbols.find (set.cmd.ref.symbol_hash) ->second.first.c_str());
			}

			break;

		case A_VALUE:
			__assert (arg_count == 1, "Invalid argument count: %d", arg_count);

			sscanf (arg, "%lg", &set.cmd.value);

			msg (E_INFO, E_DEBUGAPP, "Command: [%d %p] -> \"%s\" %lg",
			     set.cmd.command, set.cmd.debug, commands_list[set.cmd.command], set.cmd.value);
			break;

		default:
			__asshole ("Switch error");
		}
	}

	return set;
}

Processor::DecodedCommand Processor::BinaryReadCmd (FILE* stream, bool use_sparse_code)
{
	DecodedCommand cmd;
	memset (&cmd, 0xDEADBEEF, sizeof (cmd));

	if (use_sparse_code)
	{
		fread (&cmd, sizeof (cmd), 1, stream);
		__verify (cmd.command < C_MAX, "Invalid command code read: %d", cmd.command);
	}

	else
	{
		fread (& (cmd.command), sizeof (cmd.command), 1, stream);
		__verify (cmd.command < C_MAX, "Invalid command code read: %d", cmd.command);

		// Read argument
		switch (commands_traits[cmd.command].arg_type)
		{
		case A_NONE:
			break;

		case A_REFERENCE:
			fread (&cmd.ref, sizeof (cmd.ref), 1, stream);
			break;

		case A_VALUE:
			fread (&cmd.value, sizeof (cmd.value), 1, stream);
			break;

		default:
			__asshole ("Switch error");
		}
	}

	// Log the command
	switch (commands_traits[cmd.command].arg_type)
	{
	case A_NONE:
		msg (E_INFO, E_DEBUGAPP, "Command: [%d] -> \"%s\"",
		     cmd.command, commands_list[cmd.command]);
		break;

	case A_REFERENCE:

		if (cmd.ref.is_symbol)
			msg (E_INFO, E_DEBUGAPP, "Command: [%d %p] -> \"%s\" <symbol>",
			     cmd.command, cmd.debug, commands_list[cmd.command]);

		else
			msg (E_INFO, E_DEBUGAPP, "Command: [%d %p] -> \"%s\" %ld",
			     cmd.command, cmd.ref.address, commands_list[cmd.command], cmd.ref.address);

		break;

	case A_VALUE:
		msg (E_INFO, E_DEBUGAPP, "Command: [%d %p] -> \"%s\" %lg",
		     cmd.command, cmd.debug, commands_list[cmd.command], cmd.value);
		break;

	default:
		__asshole ("Switch error");
	}

	return cmd;
}

void Processor::BinaryReadSymbols (Processor::symbol_map* map, FILE* stream, bool use_sparse_code)
{
	unsigned long long section_id = 0;
	fread (&section_id, sizeof (section_id), 1, stream);
	__verify (section_id == symbols_section_sig, "Malformed symbol section: READ %p EXPECT %p",
			  section_id, symbols_section_sig);

	char* temporary = reinterpret_cast<char*> (malloc (line_length));
	__assert (temporary, "Unable to malloc label placeholder");

	size_t decl_count = 0;
	fread (&decl_count, sizeof (section_id), 1, stream);

	msg (E_INFO, E_DEBUGAPP, "Reading symbols section: %ld symbols", decl_count);

	while (decl_count--)
	{
		Symbol sym;
		fread (&sym, sizeof (sym), 1, stream);

		// Sparse code: no labels are stored
		if (use_sparse_code)
		{
			snprintf (temporary, line_length, "__l_%p", reinterpret_cast<void*> (sym.hash));

			msg (E_INFO, E_DEBUGAPP, "Symbol: [address %ld] <hash %p>", sym.address, sym.hash);
		}

		else
		{
			size_t label_size;
			fread (&label_size, sizeof (label_size), 1, stream);
			__verify (label_size, "Malformed binary: label size is zero! (hash %p)", sym.hash);

			fread (temporary, 1, label_size, stream);
			temporary[label_size] = '\0';

			msg (E_INFO, E_DEBUGAPP, "Symbol: [address %ld] \"%s\"", sym.address, temporary);
		}

		BinaryInsertSymbol (map, temporary, sym);
	}

	free (temporary);
}

void Processor::Decode (FILE* stream) throw()
{
	msg (E_INFO, E_VERBOSE, "Decoding and/or executing text commands");

	// Store the initial context # because in EIP mode we must watch context # change
	size_t initial_context_no = state.buffer;
	char* line = 0;
	size_t line_num = 0;

	try
	{
		CheckStream (stream);

		line = reinterpret_cast<char*> (malloc (line_length));
		__assert (line, "Unable to malloc input buffer of %ld bytes", line_length);

		msg (E_INFO, E_DEBUGAPP, "Initial context # is %ld", initial_context_no);

		// Exit the parse/decode loop if we're off the initial (primary) context.
		// This is for proper handling of "cswitch" statements.
		while ( (state.buffer != static_cast<size_t> (-1)) &&
		        (state.buffer >= initial_context_no))
		{
			if (feof (stream))
			{
				msg (E_WARNING, E_VERBOSE, "Preliminary EOS - setting exit condition");
				state.flags |= F_EXIT;
			}

			else
			{
				++line_num;
				memset (line, '\0', line_length);
				fgets (line, line_length, stream);
				DecodedSet set = DecodeCmd (line);

				if (state.flags & MASK (F_EIP))
				{
					if (set.cmd.command == C_NONE)
						continue;

					__verify (set.symbols.empty(), "Symbols are not allowed in EIP mode");
					InternalHandler (set.cmd);
				}

				else
				{
					if (!set.symbols.empty())
						DecodeLinkSymbols (set);

					if (set.cmd.command == C_NONE)
						continue;

					buffers[state.buffer].commands.Access (buffers[state.buffer].cmd_top++) = set.cmd;

					if (set.cmd.command == C_QUIT)
						InternalHandler (set.cmd);
				}
			}

			// Do graceful exit if we've reached quit command or condition (EOF).
			if (state.flags & MASK (F_EXIT))
				RestoreContext();
		}
	}

	catch (std::exception& e)
	{
		msg (E_CRITICAL, E_USER, "Error decoding text: %s", e.what());
		msg (E_CRITICAL, E_USER, "Exiting decode loop");
	}

	free (line);
}

void Processor::Load (FILE* stream) throw()
{
	msg (E_INFO, E_VERBOSE, "Loading and executing bytecode");

	try
	{
		CheckStream (stream);
		bool use_sparse_code = ReadSignature (stream);

		if (! (state.flags & MASK (F_EIP)))
		{
			msg (E_INFO, E_DEBUGAPP, "Reading symbol section");
			BinaryReadSymbols (&buffers[state.buffer].sym_table, stream, use_sparse_code);
		}

		size_t initial_context_no = state.buffer;

		// Files without command section are OK
		if (feof (stream))
			return;

		msg (E_INFO, E_DEBUGAPP, "Reading command section");

		unsigned long long section_id = 0;
		fread (&section_id, sizeof (section_id), 1, stream);
		__verify (section_id == command_section_sig, "Malformed command section: READ %p EXPECT %p",
		          section_id, command_section_sig);

		// Exit the parse/decode loop if we're off the initial (primary) context,
		// not simply on EXIT flag.
		// This is for proper handling of "cswitch" statements and context-switching.
		while ( (state.buffer != static_cast<size_t> (-1)) &&
		        (state.buffer >= initial_context_no))
		{
			if (feof (stream))
			{
				msg (E_CRITICAL, E_USER, "Preliminary EOS reading binary file");
				state.flags |= MASK (F_EXIT);
			}

			else
			{
				DecodedCommand cmd = BinaryReadCmd (stream, use_sparse_code);

				if (state.flags & MASK (F_EIP))
					InternalHandler (cmd);

				else
				{
					buffers[state.buffer].commands.Access (buffers[state.buffer].cmd_top++) = cmd;

					if (cmd.command == C_QUIT)
						InternalHandler (cmd);
				}
			}

			// Do graceful exit if we've reached quit command or condition (EOF).
			if (state.flags & MASK (F_EXIT))
				RestoreContext();
		}
	}

	catch (std::exception& e)
	{
		msg (E_CRITICAL, E_USER, "Error loading bytecode: \"%s\"", e.what());
		msg (E_CRITICAL, E_USER, "Exiting loading loop");
	}
}

void Processor::DumpAsm (FILE* stream, size_t which_buffer)
{
	msg (E_INFO, E_VERBOSE, "Writing assembler code from buffer %ld (mnemonics)", which_buffer);

	Buffer& buffer = buffers[which_buffer]; // It's not going to change

	// Build index of the symbols by address
	std::multimap<size_t, const std::pair<std::string, Symbol>& > symbol_addr_index;
	for (const hashed_symbol& sym: buffer.sym_table)
	{
		// We add only resolved symbols to the index
		if (sym.second.second.is_resolved)
			symbol_addr_index.insert (std::make_pair (sym.second.second.address, sym.second));
	}


	for (size_t addr = 0; addr < buffer.cmd_top; ++addr)
	{
		DecodedCommand& cmd = buffer.commands.Access (addr);
		__assert (cmd.command < C_MAX, "Invalid command code: %d", cmd.command);

		// Write symbols
		auto symbols_range = symbol_addr_index.equal_range (addr);
		for (auto s_iter = symbols_range.first; s_iter != symbols_range.second; ++s_iter)
		{
			const Symbol& sym = s_iter ->second.second;
			switch (sym.type)
			{
				case S_LABEL:
					fprintf (stream, "%s: ", s_iter ->second.first.c_str());
					break;

				case S_VAR:
					__asshole ("Variables are not implemented: \"%s\" at address %ld",
							   s_iter ->second.first.c_str(),
							   addr);
					break;

				case S_NONE:
					__asshole ("Invalid symbol type");
					break;

				default:
					__asshole ("Switch error");
					break;
			}
		}

		switch (commands_traits[cmd.command].arg_type)
		{
		case A_REFERENCE:

			if (cmd.ref.is_symbol)
				fprintf (stream, "%s %s\n",
						 commands_list[cmd.command],
						 buffer.sym_table.find (cmd.ref.symbol_hash) ->second.first.c_str());

			else
				fprintf (stream, "%s %ld\n", commands_list[cmd.command], cmd.ref.address);

			break;

		case A_VALUE:
			fprintf (stream, "%s %lg\n", commands_list[cmd.command], cmd.value);
			break;

		case A_NONE:
			fprintf (stream, "%s\n", commands_list[cmd.command]);
			break;

		default:
			__asshole ("Switch error");
		}
	}

	msg (E_INFO, E_VERBOSE, "Assembler write of buffer %ld completed", which_buffer);
}

void Processor::DumpBC (FILE* stream, bool use_sparse_code, size_t which_buffer)
{
	msg (E_INFO, E_VERBOSE, "Writing bytecode from buffer %ld to binary stream",
	     which_buffer);

	msg (E_INFO, E_DEBUGAPP, "Writing signature: MAGIC %p VERSION [%d:%d]",
	     expect_sig.magic, expect_sig.version.major, expect_sig.version.minor);

	VersionSignature write_sig = expect_sig;
	write_sig.is_sparse = use_sparse_code;
	fwrite (&write_sig, sizeof (write_sig), 1, stream);

	msg (E_INFO, E_DEBUGAPP, "Using %s mode", use_sparse_code ? "sparse" : "compact");

	Buffer& buffer = buffers[which_buffer]; // It's not going to change

	msg (E_INFO, E_DEBUGAPP, "Writing symbol section");
	fwrite (&symbols_section_sig, sizeof (symbols_section_sig), 1, stream);
	{
		size_t sym_count = buffer.sym_table.size();
		fwrite (&sym_count, sizeof (sym_count), 1, stream);

		msg (E_INFO, E_DEBUGAPP, "Writing symbol table: %ld symbols", sym_count);

		for (const hashed_symbol& s_ref: buffer.sym_table)
		{
			const Symbol& symbol = s_ref.second.second;
			const std::string& name = s_ref.second.first;

			fwrite (&symbol, sizeof (symbol), 1, stream);

			if (!use_sparse_code)
			{
				size_t name_len = name.size();
				fwrite (&name_len, sizeof (name_len), 1, stream);
				fwrite (name.c_str(), 1, name_len, stream);
			}
		}
	}


	if (!buffer.cmd_top)
	{
		msg (E_WARNING, E_DEBUGAPP, "Not writing empty code section");
	}

	else
	{
		msg (E_INFO, E_DEBUGAPP, "Writing code section");
		fwrite (&command_section_sig, sizeof (command_section_sig), 1, stream);

		msg (E_INFO, E_DEBUGAPP, "Writing %ld instructions", buffer.cmd_top);
		for (size_t i = 0; i < buffer.cmd_top; ++i)
		{
			DecodedCommand& cmd = buffer.commands.Access (i);

			if (use_sparse_code)
				fwrite (&cmd, sizeof (cmd), 1, stream);

			else
			{
				fwrite (&cmd.command, sizeof (cmd.command), 1, stream);

				switch (commands_traits[cmd.command].arg_type)
				{
				case A_REFERENCE:
					fwrite (&cmd.ref, sizeof (cmd.ref), 1, stream);
					break;

				case A_VALUE:
					fwrite (&cmd.value, sizeof (cmd.value), 1, stream);
					break;

				case A_NONE:
					break;

				default:
					__asshole ("Switch error");
				}
			}
		}
	}

	msg (E_INFO, E_VERBOSE, "Binary code dump of buffer %ld completed", which_buffer);
}

size_t Processor::GetCurrentBuffer() throw()
{
	return state.buffer;
}

void Processor::ExecuteBuffer() throw()
{
	try
	{
		NextContextBuffer();
		msg (E_INFO, E_VERBOSE, "Executing context with existing commands (%ld)", buffers[state.buffer].cmd_top);

		size_t initial_context = state.buffer;

		for (;;)
		{
			if (state.ip >= buffers[state.buffer].cmd_top)
			{
				msg (E_CRITICAL, E_USER, "IP overflow - exiting context");
				RestoreContext();
				return;
			}

			InternalHandler (buffers[state.buffer].commands.Access (state.ip++));

			if (state.flags & MASK (F_EXIT))
			{
				size_t final_context = state.buffer;
				RestoreContext();

				// Exit the loop if we've finished the initial (primary) context.
				if (final_context == initial_context)
					break;
			}
		}

		msg (E_INFO, E_VERBOSE, "Execution of context completed: the result is %lg",
		     calc_stack.Top());
	}

	catch (std::exception& e)
	{
		msg (E_CRITICAL, E_USER, "Execution failed: \"%s\"", e.what());
	}
}

void Processor::InternalHandler (const DecodedCommand& cmd)
{
	__assert (cmd.command < C_MAX, "Invalid command code: %d", cmd.command);

	switch (cmd.command)
	{
	case C_INIT:
	{
		size_t ip = state.ip;
		InitContexts();
		AllocContextBuffer();
		state.ip = ip;
		break;
	}

	case C_PUSH:
		calc_stack.Push (cmd.value);
		if (!(state.flags & MASK(F_NFC))) Analyze (cmd.value);
		break;

	case C_POP:
		if (!(state.flags & MASK(F_NFC))) Analyze (calc_stack.Top());
		msg (E_INFO, E_USER, "Popped value: %lg", calc_stack.Pop());
		break;

	case C_TOP:
		msg (E_INFO, E_USER, "Value on top: %lg", calc_stack.Top());
		if (!(state.flags & MASK(F_NFC))) Analyze (calc_stack.Top());
		break;

	case C_ADD:
		calc_stack.Push (calc_stack.Pop() + calc_stack.Pop());
		if (!(state.flags & MASK(F_NFC))) Analyze (calc_stack.Top());
		break;

	case C_SUB:
		calc_stack.Push (calc_stack.Pop() - calc_stack.Pop());
		if (!(state.flags & MASK(F_NFC))) Analyze (calc_stack.Top());
		break;

	case C_MUL:
		calc_stack.Push (calc_stack.Pop() * calc_stack.Pop());
		if (!(state.flags & MASK(F_NFC))) Analyze (calc_stack.Top());
		break;

	case C_DIV:
		calc_stack.Push (calc_stack.Pop() / calc_stack.Pop());
		if (!(state.flags & MASK(F_NFC))) Analyze (calc_stack.Top());
		break;

	case C_ANAL:
		Analyze (calc_stack.Top());
		break;

	case C_CMP:
		Analyze (calc_stack.Pop() - calc_stack.Top());
		break;

	case C_SWAP:
	{
		calc_t arg1 = calc_stack.Pop();
		calc_t arg2 = calc_stack.Pop();

		calc_stack.Push (arg1);
		calc_stack.Push (arg2);

		if (!(state.flags & MASK(F_NFC))) Analyze (calc_stack.Top());
		break;
	}

	case C_DUP:
		calc_stack.Push (calc_stack.Top());
		if (!(state.flags & MASK(F_NFC))) Analyze (calc_stack.Top());
		break;

	case C_JE:
		if (state.flags & MASK(F_ZERO))
			DoJump (cmd.ref);
		break;

	case C_JNE:
		if (!(state.flags & MASK(F_ZERO)))
			DoJump (cmd.ref);
		break;

	case C_JA:
	case C_JNBE:
		if (!(state.flags & (MASK(F_NEGATIVE) | MASK(F_ZERO))))
			DoJump (cmd.ref);
		break;

	case C_JNA:
	case C_JBE:
		if (state.flags & (MASK(F_NEGATIVE) | MASK(F_ZERO)))
			DoJump (cmd.ref);
		break;

	case C_JAE:
	case C_JNB:
		if ((state.flags & MASK(F_ZERO)) || !(state.flags & MASK(F_NEGATIVE)))
			DoJump (cmd.ref);
		break;

	case C_JNAE:
	case C_JB:
		if ((state.flags & MASK(F_NEGATIVE)) && !(state.flags & (MASK(F_ZERO))))
			DoJump (cmd.ref);
		break;

	case C_JMP:
		DoJump (cmd.ref);
		break;

	case C_CALL:
		SaveContext();
		DoJump (cmd.ref);
		break;

	case C_RET:
		RestoreContext();
		break;

	case C_DFL:
		DumpFlags();
		break;

	case C_SNFC:
		state.flags |= MASK(F_NFC);
		break;

	case C_CNFC:
		state.flags &= ~MASK(F_NFC);
		break;

	case C_CSWITCH:
		NextContextBuffer();
		break;

	case C_EXEC:
		ExecuteBuffer();
		break;

	case C_LASM:
	{
		FILE* f = fopen (asm_dump_file, "rt");

#ifndef NDEBUG
		setbuf (f, 0);
#endif

		if (f)
		{
			AllocContextBuffer();
			Decode (f);
			fclose (f);
		}

		else
			msg (E_WARNING, E_VERBOSE, "ASM dump file \"%s\" does not exist", asm_dump_file);

		break;
	}

	case C_LBIN:
	{
		FILE* f = fopen (bin_dump_file, "rb");

#ifndef NDEBUG
		setbuf (f, 0);
#endif

		if (f)
		{
			AllocContextBuffer();
			Load (f);
			fclose (f);
		}

		else
			msg (E_WARNING, E_VERBOSE, "BIN dump file \"%s\" does not exist", bin_dump_file);

		break;
	}

	case C_WASM:
	{
		FILE* f = fopen (asm_dump_file, "wt");

#ifndef NDEBUG
		setbuf (f, 0);
#endif

		if (f)
		{
			DumpAsm (f, state.buffer + 1);
			fclose (f);
		}

		else
			msg (E_WARNING, E_VERBOSE, "ASM dump file \"%s\" cannot be created", asm_dump_file);

		break;
	}

	case C_WBIN:
	{
		FILE* f = fopen (bin_dump_file, "wb");

#ifndef NDEBUG
		setbuf (f, 0);
#endif

		if (f)
		{
			DumpBC (f, 0, state.buffer + 1);
			fclose (f);
		}

		else
			msg (E_WARNING, E_VERBOSE, "BIN dump file \"%s\" cannot be created", bin_dump_file);

		break;
	}

	case C_INVD:
		// RestoreContext();
		++state.buffer;
		ClearContextBuffer();
		--state.buffer; // RestoreContext();
		break;

	case C_QUIT:
		msg (E_INFO, E_VERBOSE, "Stopping execution");
		state.flags |= MASK (F_EXIT);
		break;

	default:
		__asshole ("Switch error");
		break;
	}
}

void Processor::SetExecuteInPlace (bool eip)
{
	if (eip)
		state.flags |= MASK (F_EIP);

	else
		state.flags &= ~MASK (F_EIP);
}

unsigned char Processor::GetMajorVersion()
{
	return expect_sig.version.major;
}

unsigned char Processor::GetMinorVersion()
{
	return expect_sig.version.minor;
}

void Processor::DumpCommandSet()
{
	for (unsigned i = 0; i < C_MAX; ++i)
	{
		msg (E_INFO, E_VERBOSE, "\"%s\" (%d args)\t:: %s",
		     commands_list[i],
		     commands_traits[i].arg_type == A_NONE ? 0 : 1,
		     commands_desc[i]);
	}
}

void Processor::SetFilenamePrefix (const char* fn)
{
	msg (E_INFO, E_VERBOSE, "Setting dump file prefix to \"%s\"", fn);

	free (bin_dump_file);
	free (asm_dump_file);

	bin_dump_file = reinterpret_cast<char*> (malloc (line_length));
	asm_dump_file = reinterpret_cast<char*> (malloc (line_length));

	__assert (bin_dump_file, "Unable to malloc file name buffer");
	__assert (asm_dump_file, "Unable to malloc file name buffer");

	sprintf (bin_dump_file, "%s.dbin", fn);
	sprintf (asm_dump_file, "%s.dasm", fn);

	msg (E_INFO, E_VERBOSE, "BIN file is set to \"%s\"", bin_dump_file);
	msg (E_INFO, E_VERBOSE, "ASM file is set to \"%s\"", asm_dump_file);
}


// kate: indent-mode cstyle; replace-tabs off; tab-width 4;
