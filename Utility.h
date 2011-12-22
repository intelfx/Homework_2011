#ifndef _UTILITY_H
#define _UTILITY_H

#include "build.h"
#include "Verifier.h"

// -----------------------------------------------------------------------------
// Library		Homework
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

#define init(x) memset (&x, 0xDEADBEEF, sizeof (x));


namespace Processor
{
	typedef double calc_t;
	typedef float abiprep_t;
	typedef unsigned abiret_t;

	typedef unsigned short cid_t;

	static_assert (sizeof (abiprep_t) == sizeof (abiret_t),
				   "ABI data type size does not equal ABI return type size");

	static const size_t BUFFER_NUM = 4;


	enum ProcessorFlags
	{
		F_EIP = 1, // Execute In Place
		F_EXIT, // Context exit condition
		F_NFC, // No Flag Change - prohibits flag file from being changed as side-effect
		F_ZERO, // Zero Flag - set if last result was zero (or equal)
		F_NEGATIVE, // Negative Flag - set if last result was negative (or below)
		F_INVALIDFP // Invalid Floating-Point Flag - set if last result was infinite or NAN
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
		S_REGISTER,
		S_FRAME,
		S_FRAME_BACK, // Parameters to function
		S_MAX
	};

	enum FileSectionType
	{
		SEC_NON_UNIFORM = 0,
		SEC_SYMBOL_MAP,
		SEC_CODE_IMAGE,
		SEC_DATA_IMAGE,
		SEC_STACK_IMAGE,
		SEC_MAX
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

	struct Command
	{
		union Argument
		{
			calc_t value;
			Reference ref;
		} arg;

		cid_t id;
	};

	// This is something like TR1's std::unordered_map with manual hashing,
	// since we need to have direct access to hashes themselves.
	typedef std::map<size_t, std::pair<std::string, Symbol> > symbol_map;
	typedef symbol_map::value_type::second_type symbol_type;

	symbol_map::value_type PrepareSymbol (const char* label, Symbol sym, size_t* hash = 0);

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

	struct Context
	{
		mask_t flags;
		size_t ip;
		size_t buffer;
		size_t depth;
		size_t frame;
	};

	struct CommandTraits
	{
		const char* mnemonic;
		const char* description;
		ArgumentType arg_type;

		cid_t id; // assigned by command set
		bool executed_at_decode;

		std::map<size_t, void*> execution_handles;
	};

	struct FileProperties
	{
		std::list< std::pair<FileSectionType, size_t> > file_description;
		FILE* file;
	};

	struct DecodeResult
	{
		union
		{
			Command command;
			calc_t data;
		};

		enum
		{
			DEC_COMMAND,
			DEC_DATA,
			DEC_NOTHING
		} type;

		symbol_map mentioned_symbols;

		DecodeResult() : type (DEC_NOTHING) {}
	};

	namespace ProcDebug
	{
		extern const char* FileSectionType_ids[SEC_MAX]; // debug section IDs
		extern const char* AddrType_ids[S_MAX]; // debug address type IDs

		extern char debug_buffer[STATIC_LENGTH];

		void PrintReference (const Reference& ref);
	}

	class IReader;
	class IWriter;
	class IMMU;
	class ILinker;
	class IExecutor;
	class IBackend;
	class ICommandSet;
	class ILogic;
	class IModuleBase;

} // namespace Processor

#endif // _UTILITY_H

// kate: indent-mode cstyle; replace-tabs off; indent-width 4; tab-width 4;
