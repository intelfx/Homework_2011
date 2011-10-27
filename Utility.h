#ifndef _UTILITY_H
#define _UTILITY_H

#include "build.h"
#include "Verifier.h"

// -----------------------------------------------------------------------------
// Library		Antided
// File			Utility.h
// Author		intelfx
// Description	Utilitary structures and definitions.
// -----------------------------------------------------------------------------

#define SYMCONST(str)																	\
	((static_cast <unsigned long long> ( str[0])) |										\
	 (static_cast <unsigned long long> ( str[1]) << 8) |								\
	 (static_cast <unsigned long long> ( str[2]) << 16) |								\
	 (static_cast <unsigned long long> ( str[3]) << 24) |								\
	 (static_cast <unsigned long long> ( str[4]) << 32) |								\
	 (static_cast <unsigned long long> ( str[5]) << 40) |								\
	 (static_cast <unsigned long long> ( str[6]) << 48) |								\
	 (static_cast <unsigned long long> ( str[7]) << 56))


namespace Processor
{

typedef double calc_t;
static const int BUFFER_NUM = 4;

static const unsigned line_length = 256;


enum ProcessorFlags
{
	F_EIP = 1, // Execute In Place
	F_EXIT, // Context exit condition
	F_NFC, // No Flag Change - prohibits flag file from being changed as side-effect
	F_ZERO, // Zero Flag - set if last result was zero (or equal)
	F_NEGATIVE, // Negative Flag - set if last result was negative (or below)
};

enum Register
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
	union Argument
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
typedef symbol_map::value_type symbol_type;

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
	MallocAllocator<DecodedCommand> commands;
	size_t cmd_top;

	symbol_map sym_table;

	calc_t registers[R_MAX];
};

struct ProcessorState
{
	mask_t flags;
	size_t ip;
	size_t buffer;
	size_t depth;
};

struct CommandTraits
{
	size_t id;
	const char* mnemonic;
	const char* description;
	ArgumentType arg_type;

	std::map<size_t, void*> execution_handles;
};

size_t InsertSymbol (symbol_map* map, Symbol& symbol, const char* label, bool update_hash);

class IReader;
class IWriter;
class IMMU;
class ILinker;
class IExecutor;

} // namespace Processor

#endif // _UTILITY_H

// kate: indent-mode cstyle; replace-tabs off; tab-width 4;
