#include "stdafx.h"
#include "Processor.h"

ImplementDescriptor (Processor, "dumb processor", MOD_APPMODULE);


enum COMMANDS
{
	C_INIT = 0,
	C_PUSH,
	C_POP,
	C_TOP,

	C_SLEEP,

	C_LOAD,
	C_WRITE,

	C_ADD,
	C_SUB,
	C_MUL,
	C_DIV,

	C_INC,
	C_DEC,

	C_NEG,
	C_SQRT,

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

	"sleep",

	"ld",
	"wr",

	"add",
	"sub",
	"mul",
	"div",

	"inc",
	"dec",

	"neg",
	"sqrt",

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

	"System: hand off control to the OS",

	"Data: load data memory/register",
	"Data: write data memory/register",

	"Arithmetic: addition",
	"Arithmetic: subtraction (minuend on top)",
	"Arithmetic: multiplication",
	"Arithmetic: division (dividend on top)",

	"Arithmetic: increment by one",
	"Arithmetic: decrement by one",
	"Arithmetic: negation",
	"Arithmetic: square root extraction",

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
Processor::VersionSignature Processor::expect_sig = {SYMCONST ("_BIN_IFX"), {{'2', '2'}}, 0};

// !SEC_SYM
const unsigned long long Processor::symbols_section_sig = SYMCONST ("_SYM_SEC");

// !SEC_CMD
const unsigned long long Processor::command_section_sig = SYMCONST ("_CMD_SEC");

Processor::Processor() :
state(),
is_initialized (0),
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

	__sverify (file_sig.magic == expect_sig.magic, "FILE SIGNATURE MISMATCH: READ %p EXPECT %p",
	           file_sig.magic, expect_sig.magic);

	smsg (E_INFO, E_VERBOSE, "File signature matched: READ %p", file_sig.magic);

	__sverify (file_sig.is_sparse == expect_sig.is_sparse, "SPARSE MODE MISMATCH: READ %s EXPECT %s",
	           file_sig.is_sparse ? "sparse" : "compact",
	           expect_sig.is_sparse ? "sparse" : "compact");

	smsg (E_INFO, E_VERBOSE, "File sparse mode: %s", file_sig.is_sparse ? "sparse" : "compact");

	__sverify (file_sig.version.major == expect_sig.version.major,
	           "FILE MAJOR VERSION MISMATCH: READ [%c] EXPECT [%c]",
	           file_sig.version.major,
	           expect_sig.version.major);

	if (file_sig.version.minor == expect_sig.version.minor)
		smsg (E_INFO, E_VERBOSE, "File version matched: READ %p [%d:%d]",
		      file_sig.ver_raw,
		      file_sig.version.major,
		      file_sig.version.minor);

	else
		smsg (E_WARNING, E_VERBOSE, "FILE MINOR VERSION MISMATCH: READ [%c] EXPECT [%c]",
		      file_sig.version.minor,
		      expect_sig.version.minor);

	return file_sig.is_sparse;
}

void Processor::DumpFlags()
{
	msg (E_INFO, E_DEBUG, "Dumping processor flags");

	msg (E_INFO, E_DEBUG, "Processor flags: EIP [%d] EXIT [%d] NFC [%d]",
		 !!(state.flags & MASK(F_EIP)),
		 !!(state.flags & MASK(F_EXIT)),
		 !!(state.flags & MASK(F_NFC)));

	msg (E_INFO, E_DEBUG, "Analyze flags: ZERO [%d] NEGATIVE [%d]",
		 !!(state.flags & MASK(F_ZERO)),
		 !!(state.flags & MASK(F_NEGATIVE)));
}

size_t Processor::DecodeRegister (const char* str)
{
	if (!strcmp (str, "ra"))
		return R_A;

	if (!strcmp (str, "rb"))
		return R_B;

	if (!strcmp (str, "rc"))
		return R_C;

	if (!strcmp (str, "rd"))
		return R_D;

	if (!strcmp (str, "re"))
		return R_E;

	if (!strcmp (str, "rf"))
		return R_F;

	return R_MAX;
}

const char* Processor::EncodeRegister (size_t reg)
{
	switch (reg)
	{
		case R_A:
			return "ra";

		case R_B:
			return "rb";

		case R_C:
			return "rc";

		case R_D:
			return "rd";

		case R_E:
			return "re";

		case R_F:
			return "rf";

		default:
			__asshole ("Switch error");
	}

	return 0;
}



Processor::Buffer& Processor::GetBuffer()
{
	__verify (state.buffer != static_cast<size_t> (-1),
			  "Invalid buffer index, possible call stack underflow");

	return buffers.Access (state.buffer);
}


void Processor::Jump (const Processor::Reference& ref)
{
	const Reference::Direct& dref = Resolve (ref);

	__verify (dref.type == S_CODE, "Cannot jump to non-CODE reference");

	msg (E_INFO, E_DEBUG, "Jumping to address %ld", dref.address);
	state.ip = dref.address;
}

calc_t Processor::Read (const Processor::Reference& ref)
{
	const Reference::Direct& dref = Resolve (ref);

	switch (dref.type)
	{
		case S_DATA:
			__asshole ("DATA is unsupported");
			break;

		case S_REGISTER:
			return state.registers[dref.address];

		case S_FRAME:
			return calc_stack.Absolute (state.frame + dref.address);

		case S_FRAME_BACK:
			return calc_stack.Absolute (state.frame - dref.address);

		case S_CODE:
			__asshole ("Cannot read CODE segment");
			break;

		case S_NONE:
		default:
			__asshole ("Switch error");
			break;
	}

	return -1;
}

calc_t& Processor::Write (const Processor::Reference& ref)
{
	const Reference::Direct& dref = Resolve (ref);
	calc_t* ptr = 0;

	switch (dref.type)
	{
		case S_DATA:
			__asshole ("DATA is unsupported");
			break;

		case S_REGISTER:
			ptr = &state.registers[dref.address];
			break;

		case S_FRAME:
			ptr = &calc_stack.Absolute (state.frame + dref.address);
			break;

		case S_FRAME_BACK:
			ptr = &calc_stack.Absolute (state.frame - dref.address);
			break;

		case S_CODE:
			__asshole ("Cannot read CODE segment");
			break;

		case S_NONE:
		default:
			__asshole ("Switch error");
			break;
	}

	return *ptr;
}


const Processor::Reference::Direct& Processor::Resolve (const Processor::Reference& ref)
{
	const Processor::Reference* temp_ref = &ref;

	while (temp_ref ->is_symbol)
	{
		auto sym_iter = GetBuffer().sym_table.find (ref.symbol_hash);
		__assert (sym_iter != GetBuffer().sym_table.end(), "Invalid symbol");

		const Symbol& sym = sym_iter ->second.second;
		__assert (sym.is_resolved, "Unresolved symbol \"%s\"", sym_iter ->second.first.c_str());
		temp_ref = &sym.ref;
	}

	return temp_ref ->direct;
}

void Processor::Analyze (calc_t arg)
{
	state.flags &= ~(MASK(F_NEGATIVE) | MASK(F_ZERO));

	if (arg < 0)
		state.flags |= MASK(F_NEGATIVE);

	else if (!(arg > 0))
		state.flags |= MASK(F_ZERO);
}

void Processor::DumpContext (const Processor::ProcessorState& g_state)
{
	msg (E_INFO, E_DEBUG, "Context: IP [%d], FL [0x%08p], BUF [%ld], DEPTH [%ld], FRAME [%ld]",
		 g_state.ip, g_state.flags, g_state.buffer, g_state.depth, g_state.frame);

	msg (E_INFO, E_DEBUG,
		 "Registers: A [%lg], B [%lg], C [%lg], D [%lg], E [%lg], F [%lg]",
	     g_state.registers[R_A],
	     g_state.registers[R_B],
	     g_state.registers[R_C],
	     g_state.registers[R_D],
	     g_state.registers[R_E],
	     g_state.registers[R_F]);
}

// -----------------------------------------------------------------------------
// ---- Contexts
// -----------------------------------------------------------------------------

void Processor::InitContexts()
{
	is_initialized = 0;
	msg (E_INFO, E_VERBOSE, "Initializing system");

	context_stack.Reset();
	calc_stack.Reset();

	state.ip = 0;
	state.depth = 0;
	state.flags = MASK (F_EXIT);
	state.buffer = -1; // debug placeholder
	state.frame = 0;
	state.depth = 0;

	is_initialized = 1;
}

void Processor::SaveContext()
{
	msg (E_INFO, E_VERBOSE, "Saving execution context");
	DumpContext (state);

	context_stack.Push (state);
	++state.depth;
	state.frame = calc_stack.CurrentTopIndex();
}

void Processor::NewContext()
{
	ProcessorState newc;
	newc.ip = 0;
	newc.flags = 0;
	newc.buffer = state.buffer;
	newc.depth = 0;
	newc.frame = 0;

	for (size_t reg = 0; reg < R_MAX; ++reg)
		newc.registers[reg] = 0;

	context_stack.Push (state);
	state = newc;
}

void Processor::ClearContextBuffer()
{
	GetBuffer().cmd_top = 0;
	GetBuffer().sym_table.clear();
}

void Processor::NextContextBuffer()
{
	NewContext();
	++state.buffer;
	state.frame = calc_stack.CurrentTopIndex();

	msg (E_INFO, E_DEBUG, "Switched to next execution context buffer");
	DumpContext (state);
}

void Processor::AllocContextBuffer()
{
	NewContext();
	++state.buffer;
	state.frame = calc_stack.CurrentTopIndex();

	ClearContextBuffer();

	msg (E_INFO, E_DEBUG, "New execution context buffer allocated");
	DumpContext (state);
}

void Processor::RestoreContext()
{
	msg (E_INFO, E_VERBOSE, "Restoring context");

	DumpContext (context_stack.Top());
	state = context_stack.Pop();
}


// -----------------------------------------------------------------------------
// ---- Linker
// -----------------------------------------------------------------------------

size_t Processor::InsertSymbolPrepare (symbol_map* map, const char* label, Symbol symbol)
{
	std::string prep_label (label);
	size_t prep_hash = std::hash<std::string>() (prep_label); // Hashes the whole string

	symbol.hash = prep_hash;

	map ->insert (std::make_pair (prep_hash, std::make_pair (std::move (prep_label), symbol)));
	return prep_hash;
}

void Processor::InsertSymbolRaw (symbol_map* map, const char* label, Symbol symbol)
{
	std::string prep_label (label);
	size_t prep_hash = symbol.hash;

	map ->insert (std::make_pair (prep_hash, std::make_pair (std::move (prep_label), symbol)));
}

void Processor::DecodeLinkSymbols (Processor::DecodedSet& set)
{
	msg (E_INFO, E_DEBUG, "Linking decoded symbols: %d", set.symbols.size());

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
			if (linked_desc.ref.is_symbol)
			{
				msg (E_INFO, E_DEBUG, "Definition of alias \"%s\": aliasing %p",
					 name.c_str(), linked_desc.ref.symbol_hash);
			}

			else switch (linked_desc.ref.direct.type)
			{
				case S_CODE:
					msg (E_INFO, E_DEBUG, "Definition of label \"%s\": assigning address %ld",
						 name.c_str(), address);
					linked_desc.ref.direct.address = buffers[state.buffer].cmd_top;
					break;

				case S_DATA:
					__asshole ("DATA is unsupported");
					break;

				case S_REGISTER:
					msg (E_INFO, E_DEBUG, "Definition of register alias \"%s\": aliasing \"%s\"",
						 name.c_str(), EncodeRegister (linked_desc.ref.direct.address));
					break;

				case S_FRAME:
					msg (E_INFO, E_DEBUG, "Definition of stack frame alias \"%s\": aliasing relative address %ld",
						 name.c_str(), linked_desc.ref.direct.address);
					break;

				case S_FRAME_BACK:
					msg (E_INFO, E_DEBUG, "Definition of parameter alias \"%s\": aliasing parameter %ld",
						 name.c_str(), linked_desc.ref.direct.address);
					break;

				default:
				case S_NONE:
					__asshole ("Invalid symbol type");
					break;
			}

			// Attach to any existing record (checking it is reference, not definition).
			if (existing_record != sym_table.end())
			{
				__verify (!existing_record ->second.second.is_resolved,
				          "Symbol redefinition");

				existing_record ->second.second = linked_desc;
			}
		}

		// If symbol is used here, check for existing definitions and verify types (now absent).
		else
		{
			msg (E_INFO, E_DEBUG, "Usage of symbol \"%s\"", name.c_str());

			linked_desc.ref.direct.address = -1;
		}

		// Post-actions
		if (existing_record == sym_table.end())
			sym_table.insert (symbol);
	}

	msg (E_INFO, E_DEBUG, "Link completed");
}

// -----------------------------------------------------------------------------
// ---- Reference
// -----------------------------------------------------------------------------

Processor::Reference Processor::DecodeReference (const char* ref, DecodedSet* set)
{
	Reference target_ref;
	symbol_map& target_syms = set ->symbols;

	memset (&target_ref, 0xDEADBEEF, sizeof (target_ref));

	char reg[line_length], segment;

	if (sscanf (ref, "%c:%ld", &segment, &target_ref.direct.address) == 2)
	{
		const char* _reftype = 0;

		target_ref.is_symbol = 0;
		switch (segment)
		{
			case 'c':
				target_ref.direct.type = S_CODE;
				_reftype = "CODE";
				break;

			case 'd':
				target_ref.direct.type = S_DATA;
				_reftype = "DATA";
				break;

			case 's':
				target_ref.direct.type = S_FRAME;
				_reftype = "FRAME";
				break;

			case 'p':
				target_ref.direct.type = S_FRAME_BACK;
				_reftype = "PARAM";
				break;

			default:
				__asshole ("Invalid reference segment specificator: %c", segment);
		}

		msg (E_INFO, E_DEBUG, "Reference \"%s\": decoded direct reference to \"%s:%ld\"",
		     ref, _reftype, target_ref.direct.address);
	}

	else if (sscanf (ref, "@%s", reg) == 1)
	{
		target_ref.is_symbol = 0;
		target_ref.direct.type = S_REGISTER;
		target_ref.direct.address = DecodeRegister (reg);

		msg (E_INFO, E_DEBUG, "Reference \"%s\": decoded access to register \"%s\"", ref, reg);
	}

	else
	{
		Symbol reference_sym;
		memset (&reference_sym, 0xDEADBEEF, sizeof (reference_sym));

		reference_sym.is_resolved = 0;

		msg (E_INFO, E_DEBUG, "Reference \"%s\": decoded access of symbol", ref);

		// Add symbol and write its hash to the command
		target_ref.is_symbol = 1;
		target_ref.symbol_hash = InsertSymbolPrepare (&target_syms, ref, reference_sym);
	}

	return target_ref;
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

	msg (E_INFO, E_DEBUG, "Decoding \"%s\"", buffer);

	// Parse labels
	while (char* colon = strchr (buffer, ':'))
	{
		++label_count;

		Symbol label_sym;
		memset (&label_sym, 0xDEADBEEF, sizeof (label_sym));

		label_sym.ref.is_symbol = 0;
		label_sym.ref.direct.type = S_CODE;
		label_sym.is_resolved = 1;

		*colon++ = '\0';

		msg (E_INFO, E_DEBUG, "Label \"%s\": adding resolved symbol", buffer);
		InsertSymbolPrepare (&set.symbols, buffer, label_sym);

		buffer = colon;
		while (isspace (*buffer)) ++buffer; // Shift buffer
	}

	msg (E_INFO, E_DEBUG, "Parsed labels: %d", label_count);


	// Parse command
	short arg_count = sscanf (buffer, "%s %s", command, arg);

	if ((arg_count == EOF) || !arg_count)
	{
		msg (E_INFO, E_DEBUG, "Command: No command");
		set.cmd.command = C_NONE;
	}

	else
	{
		auto cmd_index_iter = commands_index.find (command);
		__verify (cmd_index_iter != commands_index.end(), "Unknown command: \"%s\"", command);

		set.cmd.command = cmd_index_iter ->second;

		msg (E_INFO, E_DEBUG, "Command arguments: %d", --arg_count); // Decrement since it contains command itself

		switch (commands_traits[set.cmd.command].arg_type)
		{
		case A_NONE:
			__assert (arg_count == 0, "Invalid argument count: %d", arg_count);

			msg (E_INFO, E_DEBUG, "Command: [%d] -> \"%s\"",
			     set.cmd.command, commands_list[set.cmd.command]);
			break;

		case A_REFERENCE:
			__assert (arg_count == 1, "Invalid argument count: %d", arg_count);

			msg (E_INFO, E_DEBUG, "Command: [%d] -> \"%s\" <reference>",
				 set.cmd.command, commands_list[set.cmd.command]);

			set.cmd.ref = DecodeReference (arg, &set);
			break;

		case A_VALUE:
			__assert (arg_count == 1, "Invalid argument count: %d", arg_count);

			sscanf (arg, "%lg", &set.cmd.value);

			msg (E_INFO, E_DEBUG, "Command: [%d] -> \"%s\" %lg",
			     set.cmd.command, commands_list[set.cmd.command], set.cmd.value);
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
		msg (E_INFO, E_DEBUG, "Command: [%d] -> \"%s\"",
		     cmd.command, commands_list[cmd.command]);
		break;

	case A_REFERENCE:

		msg (E_INFO, E_DEBUG, "Command: [%d] -> \"%s\" <reference>",
		     cmd.command, commands_list[cmd.command]);

		if (cmd.ref.is_symbol)
		{
			msg (E_INFO, E_DEBUG, "Reference: reference of symbol %p", cmd.ref.symbol_hash);
		}

		else
		{
			__assert (cmd.ref.direct.type != S_NONE, "Invalid direct reference type");

			if (cmd.ref.direct.type == S_REGISTER)
			{
				msg (E_INFO, E_DEBUG, "Reference: access of register %s",
				     EncodeRegister (cmd.ref.direct.address));
			}

			else
			{
				msg (E_INFO, E_DEBUG, "Reference: direct reference to %s:%ld",
				     cmd.ref.direct.type == S_CODE ? "CODE" :
				     cmd.ref.direct.type == S_DATA ? "DATA" : 0,
				     cmd.ref.direct.address);
			}

		}

		break;

	case A_VALUE:
		msg (E_INFO, E_DEBUG, "Command: [%d] -> \"%s\" %lg",
		     cmd.command, commands_list[cmd.command], cmd.value);
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

	msg (E_INFO, E_DEBUG, "Reading symbols section: %ld symbols", decl_count);

	while (decl_count--)
	{
		Symbol sym;
		fread (&sym, sizeof (sym), 1, stream);

		// Sparse code: no labels are stored
		if (use_sparse_code)
		{
			snprintf (temporary, line_length, "__l_%p", reinterpret_cast<void*> (sym.hash));

			msg (E_INFO, E_DEBUG, "Read symbol: <hash %p>", sym.hash);
		}

		else
		{
			size_t label_size;
			fread (&label_size, sizeof (label_size), 1, stream);
			__verify (label_size, "Malformed binary: label size is zero! (hash %p)", sym.hash);

			fread (temporary, 1, label_size, stream);
			temporary[label_size] = '\0';

			msg (E_INFO, E_DEBUG, "Read symbol: \"%s\"", temporary);
		}

		InsertSymbolRaw (map, temporary, sym);
	}

	free (temporary);
}

bool Processor::Decode (FILE* stream) throw()
{
	msg (E_INFO, E_VERBOSE, "Decoding and/or executing text commands");
	bool result = 1;

	// Store the initial context # because in EIP mode we must watch context # change
	size_t initial_context_no = state.buffer;
	char* line = 0;
	size_t line_num = 0;

	try
	{
		CheckStream (stream);

		line = reinterpret_cast<char*> (malloc (line_length));
		__assert (line, "Unable to malloc input buffer of %ld bytes", line_length);

		msg (E_INFO, E_DEBUG, "Initial context # is %ld", initial_context_no);

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

					GetBuffer().commands.Access (GetBuffer().cmd_top++) = set.cmd;

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
		ClearContextBuffer();

		while ( (state.buffer >= initial_context_no) &&
		        (state.buffer != static_cast<size_t> (-1)))
			RestoreContext();

		result = 0;
	}

	free (line);
	return result;
}

bool Processor::Load (FILE* stream) throw()
{
	msg (E_INFO, E_VERBOSE, "Loading and executing bytecode");
	bool result = 1;

	size_t initial_context_no = state.buffer;

	try
	{
		CheckStream (stream);
		bool use_sparse_code = ReadSignature (stream);

		if (! (state.flags & MASK (F_EIP)))
		{
			msg (E_INFO, E_DEBUG, "Reading symbol section");
			BinaryReadSymbols (&GetBuffer().sym_table, stream, use_sparse_code);
		}

		// Files without command section are OK
		if (feof (stream))
			return result;

		msg (E_INFO, E_DEBUG, "Reading command section");

		unsigned long long section_id = 0;
		fread (&section_id, sizeof (section_id), 1, stream);
		__verify (section_id == command_section_sig, "Malformed command section: READ %p EXPECT %p",
		          section_id, command_section_sig);

		size_t cmd_count;
		fread (&cmd_count, sizeof (cmd_count), 1, stream);

		// Exit the parse/decode loop if we're off the initial (primary) context,
		// not simply on EXIT flag.
		// This is for proper handling of "cswitch" statements and context-switching.
		while ( (state.buffer != static_cast<size_t> (-1)) &&
		        (state.buffer >= initial_context_no))
		{
			__verify (!feof (stream), "Preliminary EOS reading binary file");

			if (!cmd_count)
			{
				msg (E_WARNING, E_USER, "Reached end of the command section");
				state.flags |= MASK (F_EXIT);
			}

			else
			{
				--cmd_count;

				DecodedCommand cmd = BinaryReadCmd (stream, use_sparse_code);

				if (state.flags & MASK (F_EIP))
					InternalHandler (cmd);

				else
				{
					GetBuffer().commands.Access (GetBuffer().cmd_top++) = cmd;

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
		ClearContextBuffer();

		while ( (state.buffer >= initial_context_no) &&
		        (state.buffer != static_cast<size_t> (-1)))
			RestoreContext();

		result = 0;
	}

	return result;
}

void Processor::DumpAsm (FILE* stream, size_t which_buffer)
{
	msg (E_INFO, E_VERBOSE, "Writing assembler code from buffer %ld (mnemonics)", which_buffer);

	Buffer& buffer = buffers[which_buffer]; // It's not going to change

	// Build index of the symbols by address (CODE section)
	std::multimap<size_t, const std::pair<std::string, Symbol>& > code_symbol_index;
	for (const hashed_symbol& sym: buffer.sym_table)
	{
		// Skip unresolved and indirect-pointing
		if (!sym.second.second.is_resolved || sym.second.second.ref.is_symbol)
			continue;

		const Reference::Direct& dref = sym.second.second.ref.direct;

		if (dref.type == S_CODE)
			code_symbol_index.insert (std::make_pair (dref.address, sym.second));

		else
			msg (E_WARNING, E_USER, "Unsupported symbol type %d in symbol \"%s\", not dumping",
			     dref.type,
			     sym.second.first.c_str());

	}


	for (size_t addr = 0; addr < buffer.cmd_top; ++addr)
	{
		DecodedCommand& cmd = buffer.commands.Access (addr);
		__assert (cmd.command < C_MAX, "Invalid command code: %d", cmd.command);

		// Write labels
		auto labels_range = code_symbol_index.equal_range (addr);
		for (auto l_iter = labels_range.first; l_iter != labels_range.second; ++l_iter)
		{
			const Symbol& sym = l_iter ->second.second;
			__assert (sym.is_resolved && !sym.ref.is_symbol && sym.ref.direct.type == S_CODE,
					  "Unsupported label format");

			msg (E_INFO, E_DEBUG, "Address %ld: writing label \"%s\" [address %ld]",
				 addr, l_iter ->second.first.c_str(), sym.ref.direct.address);

			fprintf (stream, "%s: ", l_iter ->second.first.c_str());
		}


		const char* cmd_str = commands_list[cmd.command];

		switch (commands_traits[cmd.command].arg_type)
		{
		case A_REFERENCE:

			if (cmd.ref.is_symbol)
				fprintf (stream, "%s %s",
						 cmd_str,
						 buffer.sym_table.find (cmd.ref.symbol_hash) ->second.first.c_str());

			else switch (cmd.ref.direct.type)
				{
				case S_CODE:
					fprintf (stream, "%s c:%ld", cmd_str, cmd.ref.direct.address);
					break;

				case S_DATA:
					fprintf (stream, "%s d:%ld", cmd_str, cmd.ref.direct.address);
					break;

				case S_FRAME:
					fprintf (stream, "%s s:%ld", cmd_str, cmd.ref.direct.address);
					break;

				case S_FRAME_BACK:
					fprintf (stream, "%s p:%ld", cmd_str, cmd.ref.direct.address);
					break;

				case S_REGISTER:
					fprintf (stream, "%s @%s", cmd_str, EncodeRegister (cmd.ref.direct.address));
					break;

				case S_NONE:
				default:
					__asshole ("Switch error");
					break;
				}

			break;

		case A_VALUE:
			fprintf (stream, "%s %lg", cmd_str, cmd.value);
			break;

		case A_NONE:
			break;

		default:
			__asshole ("Switch error");
		}

		putc ('\n', stream);
	}

	msg (E_INFO, E_VERBOSE, "Assembler write of buffer %ld completed", which_buffer);
}

void Processor::DumpBC (FILE* stream, bool use_sparse_code, size_t which_buffer)
{
	msg (E_INFO, E_VERBOSE, "Writing bytecode from buffer %ld to binary stream",
	     which_buffer);

	msg (E_INFO, E_DEBUG, "Writing signature: MAGIC %p VERSION [%d:%d]",
	     expect_sig.magic, expect_sig.version.major, expect_sig.version.minor);

	VersionSignature write_sig = expect_sig;
	write_sig.is_sparse = use_sparse_code;
	fwrite (&write_sig, sizeof (write_sig), 1, stream);

	msg (E_INFO, E_DEBUG, "Using %s mode", use_sparse_code ? "sparse" : "compact");

	Buffer& buffer = buffers[which_buffer]; // It's not going to change

	msg (E_INFO, E_DEBUG, "Writing symbol section");
	fwrite (&symbols_section_sig, sizeof (symbols_section_sig), 1, stream);
	{
		size_t sym_count = buffer.sym_table.size();
		fwrite (&sym_count, sizeof (sym_count), 1, stream);

		msg (E_INFO, E_DEBUG, "Writing symbol table: %ld symbols", sym_count);

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
		msg (E_WARNING, E_DEBUG, "Not writing empty code section");
	}

	else
	{
		msg (E_INFO, E_DEBUG, "Writing code section");
		fwrite (&command_section_sig, sizeof (command_section_sig), 1, stream);

		msg (E_INFO, E_DEBUG, "Writing %ld instructions", buffer.cmd_top);
		fwrite (&buffer.cmd_top, sizeof (buffer.cmd_top), 1, stream);
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
		msg (E_INFO, E_VERBOSE, "Executing context with existing commands (%ld)", buffers[state.buffer].cmd_top);

		size_t initial_context = state.buffer;

		for (;;)
		{
			if (state.ip >= GetBuffer().cmd_top)
			{
				msg (E_CRITICAL, E_USER, "IP overflow - exiting context");
				RestoreContext();
				return;
			}

			InternalHandler (GetBuffer().commands.Access (state.ip++));

			if (state.flags & MASK (F_EXIT))
			{
				size_t final_context = state.buffer;
				RestoreContext();

				// Exit the loop if we've finished the initial (primary) context.
				if (final_context == initial_context)
					break;
			}
		}

		msg (E_INFO, E_USER, "Execution of context completed: the result is %lg",
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
		InitContexts();
		AllocContextBuffer();
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

	case C_SLEEP:
		sleep (0);
		break;

	case C_LOAD:
		calc_stack.Push (Read (cmd.ref));
		break;

	case C_WRITE:
		Write (cmd.ref) = calc_stack.Pop();
		break;

	case C_ADD:
		calc_stack.Push (calc_stack.Pop() + calc_stack.Pop());
		if (!(state.flags & MASK(F_NFC))) Analyze (calc_stack.Top());
		break;

	case C_SUB:
	{
		calc_t arg1 = calc_stack.Pop();
		calc_t arg2 = calc_stack.Pop();

		calc_stack.Push (arg2 - arg1);
		if (!(state.flags & MASK(F_NFC))) Analyze (calc_stack.Top());
		break;
	}

	case C_MUL:
		calc_stack.Push (calc_stack.Pop() * calc_stack.Pop());
		if (!(state.flags & MASK(F_NFC))) Analyze (calc_stack.Top());
		break;

	case C_DIV:
	{
		calc_t arg1 = calc_stack.Pop();
		calc_t arg2 = calc_stack.Pop();

		calc_stack.Push (arg2 / arg1);
		if (!(state.flags & MASK(F_NFC))) Analyze (calc_stack.Top());
		break;
	}

	case C_INC:
		++calc_stack.Top();
		break;

	case C_DEC:
		--calc_stack.Top();
		break;

	case C_NEG:
		calc_stack.Top() = -(calc_stack.Top());
		break;

	case C_SQRT:
		calc_stack.Top() = sqrt (calc_stack.Top());
		break;

	case C_ANAL:
		Analyze (calc_stack.Top());
		break;

	case C_CMP:
		Analyze (-(calc_stack.Pop() - calc_stack.Top()));
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
			Jump (cmd.ref);
		break;

	case C_JNE:
		if (!(state.flags & MASK(F_ZERO)))
			Jump (cmd.ref);
		break;

	case C_JA:
	case C_JNBE:
		if (!(state.flags & (MASK(F_NEGATIVE) | MASK(F_ZERO))))
			Jump (cmd.ref);
		break;

	case C_JNA:
	case C_JBE:
		if (state.flags & (MASK(F_NEGATIVE) | MASK(F_ZERO)))
			Jump (cmd.ref);
		break;

	case C_JAE:
	case C_JNB:
		if ((state.flags & MASK(F_ZERO)) || !(state.flags & MASK(F_NEGATIVE)))
			Jump (cmd.ref);
		break;

	case C_JNAE:
	case C_JB:
		if ((state.flags & MASK(F_NEGATIVE)) && !(state.flags & (MASK(F_ZERO))))
			Jump (cmd.ref);
		break;

	case C_JMP:
		Jump (cmd.ref);
		break;

	case C_CALL:
		SaveContext();
		Jump (cmd.ref);
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
		NextContextBuffer();
		ExecuteBuffer();
		break;

	case C_LASM:
	{
		FILE* f = fopen (asm_dump_file, "rt");

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
