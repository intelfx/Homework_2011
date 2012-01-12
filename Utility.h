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

#define init(x) memset (&x, 0, sizeof (x));


namespace Processor
{
	// Main processing types

	typedef double fp_t; // main floating-point type
	typedef long int_t; // main integer type

	static_assert (sizeof (fp_t) == sizeof (int_t),
				   "FP data type size does not equal integer data type size");

	// ABI types

	/*
	 * ABI conversion scheme:
	 *  first conversion is a valid C cast (with precision loss)
	 *  second conversion is type punning to integer type (same size, but exact copying)
	 *  fp_t -> fp_abi_t -> abiret_t
	 *  int_t -> int_abi_t -> abiret_t
	 */

	typedef float fp_abi_t; // ABI intermediate floating-point type
	typedef unsigned int_abi_t; // ABI intermediate integer type

	typedef unsigned abiret_t; // final ABI interaction type


	static_assert (sizeof (fp_abi_t) == sizeof (abiret_t),
				   "ABI data type size does not equal FP intermediate data type size");

	static_assert (sizeof (int_abi_t) == sizeof (abiret_t),
				   "ABI data type size does not equal integer intermediate data type size");


	typedef unsigned short cid_t; // command identifier type

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

	const Register indirect_addressing_register = R_F;

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
			Direct plain;
			size_t symbol_hash;
		};

		enum Type
		{
			RT_DIRECT,
			RT_INDIRECT,
			RT_SYMBOL
		} type;
	};

	struct Symbol
	{
		size_t hash; // Yes, redundant - but failsafe
		Reference ref;
		bool is_resolved; // When returning from decode, this means "is defined"
	};

	typedef struct Value
	{
		union
		{
			fp_t fp;
			int_t integer;
		};

		enum Type
		{
			V_FLOAT,
			V_INTEGER,
			V_MAX
		} type;

		Value() :
		type (V_MAX)
		{
		}

		Value (fp_t src) :
		type (V_FLOAT)
		{
			fp = src;
		}

		Value (int_t src) :
		type (V_INTEGER)
		{
			integer = src;
		}

		template <typename T> void Get (T& dest) const
		{
			switch (type)
			{
			case V_FLOAT:
				dest = fp;
				break;

			case V_INTEGER:
				dest = integer;
				break;

			case V_MAX:
				__sasshole ("Uninitialised value");
				break;

			default:
				__sasshole ("Switch error");
				break;
			}
		}

		template <typename T> void Set (T src)
		{
			switch (type)
			{
			case V_FLOAT:
				fp = src;
				break;

			case V_INTEGER:
				integer = src;
				break;

			case V_MAX:
				__sasshole ("Uninitialised value");
				break;

			default:
				__sasshole ("Switch error");
				break;
			}
		}

		abiret_t GetABI() const
		{
			switch (type)
			{
			case V_INTEGER:
			{
				int_abi_t intermediate = integer;
				return *reinterpret_cast<abiret_t*> (&intermediate);
			}

			case V_FLOAT:
			{
				fp_abi_t intermediate = fp;
				return *reinterpret_cast<abiret_t*> (&intermediate);
			}

			case V_MAX:
				__sasshole ("Uninitialised value");
				break;

			default:
				__sasshole ("Switch error");
				break;
			}

			return 0; // for GCC not to complain
		}

		void SetFromABI (abiret_t value, Type new_type)
		{
			switch (type = new_type)
			{
			case V_INTEGER:
			{
				int_abi_t intermediate = *reinterpret_cast<int_abi_t*> (&value);
				integer = intermediate;
				break;
			}

			case V_FLOAT:
			{
				fp_abi_t intermediate = *reinterpret_cast<fp_abi_t*> (&value);
				fp = intermediate;
				break;
			}

			case V_MAX:
				__sasshole ("Uninitialised value");
				break;

			default:
				__sasshole ("Switch error");
				break;
			}
		}

		void Parse (const char* string)
		{
			char* endptr;

			switch (type)
			{
			case V_INTEGER:
			{
				errno = 0;
				integer = strtol (string, &endptr, 0); /* autodetermine base */

				__sverify (!errno && (endptr != string) && (*endptr == '\0'),
						   "Malformed integer value argument: \"%s\": strtol() says %s",
						   string, strerror (errno));

				break;
			}

			case V_FLOAT:
			{
				errno = 0;
				fp = strtof (string, &endptr);

				__sverify (!errno && (endptr != string) && (*endptr == '\0'),
						   "Malformed floating-point value argument: \"%s\": strtof() says %s",
						   string, strerror (errno));

				int classification = fpclassify (fp);
				__sverify (classification == FP_NORMAL || classification == FP_ZERO,
						   "Invalid floating-point value: \"%s\" -> %lg", string, fp);

				break;
			}

			case V_MAX:
				__sasshole ("Uninitialised value");
				break;

			default:
				__sasshole ("Switch error");
				break;
			}
		}
	} calc_t;
// 	typedef fp_t calc_t; // until we support multitype

	struct Command
	{
		union Argument
		{
			calc_t value;
			Reference ref;
		} arg;

		cid_t id;
		Value::Type type;
	};

	// This is something like TR1's std::unordered_map with manual hashing,
	// since we need to have direct access to hashes themselves.
	typedef std::map<size_t, std::pair<std::string, Symbol> > symbol_map;
	typedef symbol_map::value_type::second_type symbol_type;

	symbol_map::value_type PrepareSymbol (const char* label, Symbol sym, size_t* hash = 0);

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
		bool is_service_command;

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
		extern const char* ValueType_ids[Value::V_MAX + 1]; // debug value type IDs

		extern char debug_buffer[STATIC_LENGTH];

		void PrintReference (const Reference& ref);
		void PrintValue (const Value& val);
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

// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
