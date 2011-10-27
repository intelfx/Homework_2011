#include "stdafx.h"
#include "Stack.h"

#include <strings.h>

// -----------------------------------------------------------------------------
// Library		Antided
// File			OLD_Processor.h
// Author		intelfx
// Description	Dumb processor implementation for Ded's pleasure
// -----------------------------------------------------------------------------

DeclareDescriptor (OLD_Processor);

#define SYMCONST(str)																	\
	((static_cast <unsigned long long> ( str[0])) |										\
	(static_cast <unsigned long long> ( str[1]) << 8) |									\
	(static_cast <unsigned long long> ( str[2]) << 16) |								\
	(static_cast <unsigned long long> ( str[3]) << 24) |								\
	(static_cast <unsigned long long> ( str[4]) << 32) |								\
	(static_cast <unsigned long long> ( str[5]) << 40) |								\
	(static_cast <unsigned long long> ( str[6]) << 48) |								\
	(static_cast <unsigned long long> ( str[7]) << 56))


typedef double calc_t;
static const int COMMAND_BUFFER = 100;
static const int BUFFER_NUM = 4;

static const unsigned line_length = 256;

/*
 * Epigraph:
 *
 * Roter Sand und zwei Patronen
 * Eine stirbt in Pulverkuss
 * Die zweite sollt ihr Ziel nicht schonen
 * Steckt jetzt tief in meiner Brust
 */

class OLD_Processor : LogBase (OLD_Processor)
{
	enum OLD_ProcessorFlags
	{
		F_EIP = 1, // Execute In Place
		F_EXIT, // Context exit condition
		F_NFC, // No Flag Change - prohibits flag file from being changed as side-effect
		F_ZERO, // Zero Flag - set if last result was zero (or equal)
		F_NEGATIVE, // Negative Flag - set if last result was negative (or below)
	};

	enum Registers
	{
		R_A = 0,
		R_B,
		R_C,
		R_D,
		R_E,
		R_F,
		R_MAX
	};

	enum ArgumentType
	{
		A_NONE = 0,
		A_VALUE,
		A_REFERENCE
	};

	enum AddrType
	{
		S_NONE = 0,
		S_CODE,
		S_DATA,
		S_REGISTER
	};

	struct Reference
	{
		struct Direct
		{
			size_t address;
			AddrType type;
		};


		union
		{
			Direct direct;
			size_t symbol_hash;
		};

		bool is_symbol;
	};

	struct Symbol
	{
		size_t hash; // Yes, redundant - but failsafe
		Reference ref;
		bool is_resolved; // When returning from decode, this means "is defined"
	};

	struct DecodedCommand
	{
		union
		{
			calc_t value;
			Reference ref;
			void* debug;
		};

		unsigned char command;
	};

	// This is something like TR1's std::unordered_map with manual hashing,
	// since we need to have direct access to hashes themselves.
	typedef std::map<size_t, std::pair<std::string, Symbol> > symbol_map;
	typedef symbol_map::value_type hashed_symbol;

	struct DecodedSet
	{
		DecodedCommand cmd;
		symbol_map symbols;
	};

	struct VersionSignature
	{
		unsigned long long magic;

		union
		{
			struct
			{
				unsigned char minor;
				unsigned char major;
			} version;

			unsigned short ver_raw;
		};

		bool is_sparse;
	} PACKED;

	struct Buffer
	{
		StaticAllocator<DecodedCommand, COMMAND_BUFFER> commands;
		size_t cmd_top;

		symbol_map sym_table;
	};

	struct OLD_ProcessorState
	{
		mask_t flags;
		size_t ip;
		size_t buffer;
		size_t depth;

		calc_t registers[R_MAX];
	} state;

	bool is_initialized;

	struct CommandTraits
	{
		ArgumentType arg_type;
	};

	// ---------------------------------------
	// Static definitions
	// ---------------------------------------

	std::map<std::string, unsigned char> commands_index;
	static const char* commands_list[];
	static const char* commands_desc[];
	static CommandTraits commands_traits[];

	static VersionSignature expect_sig;
	static const unsigned long long symbols_section_sig;
	static const unsigned long long command_section_sig;


	// ---------------------------------------
	// System state
	// ---------------------------------------

	Stack<calc_t> calc_stack;
	Stack<OLD_ProcessorState> context_stack;
	StaticAllocator<Buffer, BUFFER_NUM> buffers;

	char* bin_dump_file;
	char* asm_dump_file;


	// ---------------------------------------
	// Methods
	// ---------------------------------------

	Buffer& GetBuffer();
	void DumpFlags();

	void InternalHandler (const DecodedCommand& cmd);
	void Jump (const OLD_Processor::Reference& ref);
	calc_t Read (const OLD_Processor::Reference& ref);
	calc_t& Write (const OLD_Processor::Reference& ref);

	void Analyze (calc_t arg);

	static void CheckStream (FILE* stream);
	bool ReadSignature (FILE* stream); // returns is_sparse_code flag of read signature

	// Register names
	size_t DecodeRegister (const char* str);
	const char* EncodeRegister (size_t reg);

	// New symbol handling
	size_t InsertSymbolPrepare (symbol_map* map, const char* label, Symbol symbol); // Returns symbol hash
	void InsertSymbolRaw (symbol_map* map, const char* label, Symbol symbol);

	// Symbol resolvers
	const Reference::Direct& Resolve (const OLD_Processor::Reference& ref);
	Reference::Direct& Resolve (const char* name);

	// Format-specific functions : ASM decoding
	void DecodeLinkSymbols (DecodedSet& set);
	Reference DecodeReference (const char* ref, OLD_Processor::DecodedSet* set);
	DecodedSet DecodeCmd (char* buffer);

	// Format-specific functions : BIN loading
	void BinaryReadSymbols (symbol_map* map, FILE* stream, bool use_sparse_code);
	DecodedCommand BinaryReadCmd (FILE* stream, bool use_sparse_code);

	void ClearContextBuffer(); // Reset current context buffer

	void DumpContext(const OLD_Processor::OLD_ProcessorState& g_state);

	void SaveContext(); // Save context without resetting it
	void NewContext();
	void RestoreContext();

protected:
	//virtual bool Verify_() const;

public:
	OLD_Processor();
	virtual ~OLD_Processor();

	void SetExecuteInPlace (bool eip);

	// Default file name prefix used to load/dump buffer from code
	void SetFilenamePrefix (const char* fn);

	void InitContexts();
	void AllocContextBuffer(); // Switch to next context buffer, resetting it
	void NextContextBuffer(); // Switch to next context buffer while preserving its state

	bool Decode (FILE* stream) throw();
	bool Load (FILE* stream) throw();
	void ExecuteBuffer() throw();
	size_t GetCurrentBuffer() throw();
	void DumpBC (FILE* stream, bool use_sparse_code, size_t which_buffer);
	void DumpAsm (FILE* stream, size_t which_buffer);

	size_t GetRegistersNum()
	{
		return R_MAX;
	}

	void AccessRegister (size_t reg, calc_t value)
	{
		__assert (reg < R_MAX, "Invalid register ID");
		msg (E_INFO, E_DEBUGAPP, "Userland sets register %s to %lg", EncodeRegister (reg), value);

		state.registers[reg] = value;
	}

	calc_t AccessRegister (size_t reg)
	{
		__assert (reg < R_MAX, "Invalid register ID");

		return state.registers[reg];
	}

	unsigned char GetMajorVersion();
	unsigned char GetMinorVersion();
	void DumpCommandSet();
};