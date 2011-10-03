#include "stdafx.h"
#include "Stack.h"

#include <strings.h>

// -----------------------------------------------------------------------------
// Library		Antided
// File			Processor.h
// Author		intelfx
// Description	Dumb processor implementation for Ded's pleasure
// -----------------------------------------------------------------------------

DeclareDescriptor (Processor);

typedef double calc_t;
static const int COMMAND_BUFFER = 100;
static const int BUFFER_NUM = 4;

/*
 * Epigraph:
 *
 * Roter Sand und zwei Patronen
 * Eine stirbt in Pulverkuss
 * Die zweite sollt ihr Ziel nicht schonen
 * Steckt jetzt tief in meiner Brust
 */

class Processor : LogBase (Processor)
{
	enum ProcessorFlags
	{
		F_EIP = 1, // Execute In Place
		F_EXIT, // Context exit condition
		F_NFC, // No Flag Change - prohibits flag file from being changed as side-effect
		F_ZERO, // Zero Flag - set if last result was zero (or equal)
		F_NEGATIVE, // Negative Flag - set if last result was negative (or below)
	};

	enum ArgumentType
	{
		A_NONE = 0,
		A_VALUE,
		A_REFERENCE
	};

	enum SymbolType
	{
		S_NONE = 0,
		S_LABEL,
		S_VAR
	};

	struct Symbol
	{
		size_t hash; // Yes, redundant - but failsafe
		size_t address;

		SymbolType type;

		bool is_resolved; // When returning from decode, this means "is defined"
	};

	struct Reference
	{
		union
		{
			size_t address;
			size_t symbol_hash;
		};

		bool is_symbol;
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

	typedef std::map<size_t, std::pair<std::string, Symbol> > symbol_map;
	typedef symbol_map::value_type hashed_symbol;

	struct DecodedSet
	{
		DecodedCommand cmd;

		// This is something like TR1's std::unordered_map with manual hashing,
		// since we need to have direct access to hashes themselves.
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
	}; __attribute__((packed))

	struct Buffer
	{
		StaticAllocator<DecodedCommand, COMMAND_BUFFER> commands;
		size_t cmd_top;

		symbol_map sym_table;
	};

	struct ProcessorState
	{
		mask_t flags;
		size_t ip;
		size_t buffer;
	} state;

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

	static const unsigned line_length = 256;

	static VersionSignature expect_sig;
	static const unsigned long long symbols_section_sig;
	static const unsigned long long command_section_sig;


	// ---------------------------------------
	// System state
	// ---------------------------------------

	Stack<calc_t> calc_stack;
	Stack<ProcessorState> context_stack;
	StaticAllocator<Buffer, BUFFER_NUM> buffers;

	char* bin_dump_file;
	char* asm_dump_file;


	// ---------------------------------------
	// Methods
	// ---------------------------------------

	void DoJump (const Processor::Reference& ref);
	void Analyze (calc_t arg);

	void InternalHandler (const DecodedCommand& cmd);
	void DumpFlags();

	static void CheckStream (FILE* stream);
	bool ReadSignature (FILE* stream); // returns is_sparse_code

	void DecodeInsertSymbol (symbol_map* map, const char* label, Symbol symbol,
							 size_t* write_hash_ptr); // Writes symbol hash as side-effect
	void DecodeLinkSymbols (DecodedSet& set);
	DecodedSet DecodeCmd (char* buffer);

	void BinaryInsertSymbol (symbol_map* map, const char* label, Symbol symbol);
	void BinaryReadSymbols (symbol_map* map, FILE* stream, bool use_sparse_code);
	DecodedCommand BinaryReadCmd (FILE* stream, bool use_sparse_code);

	void ClearContext();
	void AuxPushContext(); // Save context without resetting it
	void PushContext();
	void RestoreContext();
	void InitContexts();

protected:
	//virtual bool Verify_() const;

public:
	Processor();
	virtual ~Processor();

	void SetExecuteInPlace (bool eip);

	// Default file name prefix used to load/dump buffer from code
	void SetFilenamePrefix (const char* fn);

	void AllocContext();

	void Decode (FILE* stream);
	void Load (FILE* stream);
	void ExecuteBuffer();
	void DumpBC (FILE* stream, bool use_sparse_code, size_t which_buffer);
	void DumpAsm (FILE* stream, size_t which_buffer);

	unsigned char GetMajorVersion();
	unsigned char GetMinorVersion();
	void DumpCommandSet();
};