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

#if defined (TARGET_X64)
	typedef double fp_t; // main floating-point type
	typedef long int_t; // main integer type
#elif defined (TARGET_X86)
	typedef float fp_t;
	typedef long int_t;
#endif

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
		F_WAS_JUMP = 1, // Last instruction executed had changed the Program Counter
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
		S_BYTEPOOL,
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

	/*
	 * Possible reference types:
	 * - symbol				"variable"
	 * - symbol+offset		"variable+10"
	 * - symbol+indirect	"variable+(reference)"
	 * - absolute			"d:10"
	 * - absolute+offset	"d:10+20"
	 * - absolute+indirect	"d:10+(reference)"
	 * - indirect			"d:(reference)"
	 * - indirect+indirect	"d:(reference)+(reference)"
	 *
	 * Result:
	 * address  ::=  register | memory | string
	 *
	 * register ::= '$' {enum Register}
	 * memory   ::=  [ section ':' ] single [ '+' single ]
	 *
	 * single   ::= base | indirect
	 * base     ::= symbol | absolute
	 * indirect ::= '(' register | [ section ':' ] base ')'
	 *
	 * section  ::= {enum AddrType}
	 * symbol   ::= ['_' 'a'-'z' 'A'-'Z'] ['_' 'a'-'z' 'A'-'Z' '0'-'9' ]*
	 * absolute ::= [0-9]*
	 * string   ::= '\"' [^'\"']* '\"'
	 *
	 * Semantics:
	 * - There should not be more than one section specifier, including inherited from symbols.
	 */

	struct Reference
	{
		struct BaseRef
		{
			union
			{
				size_t	symbol_hash;
				size_t memory_address;
			};

			bool is_symbol;
		};

		struct IndirectRef
		{
			AddrType	section;
			BaseRef		target;
		};

		struct SingleRef
		{
			union
			{
				BaseRef		target;
				IndirectRef indirect;
			};

			bool is_indirect;
		};

		AddrType global_section;
		SingleRef components[2];

		/* one can iterate components with such loop:
		 * for (int i = 0; i <= reference.has_second_component; ++i)
		 */
		bool has_second_component;

		bool needs_linker_placement;
	};

	struct DirectReference
	{
		AddrType	section;
		size_t		address;
	};

	struct Symbol
	{

		size_t hash; // Yes, redundant - but failsafe
		Reference ref;
		bool is_resolved; // When returning from decode, this means "is defined"


		Symbol (const char* name) :
		hash (hasher_bsd_string (name)), ref(), is_resolved (0) {}

		Symbol (const char* name, const Reference& resolved_reference) :
		hash (hasher_bsd_string (name)), ref (resolved_reference), is_resolved (1) {}


		bool operator== (const Symbol& that) const { return hash == that.hash; }
		bool operator!= (const Symbol& that) const { return hash != that.hash; }
	};

	// This is something like TR1's std::unordered_map with manual hashing,
	// since we need to have direct access to hashes themselves.
	typedef std::map<size_t, std::pair<std::string, Symbol> > symbol_map;
	typedef symbol_map::value_type::second_type symbol_type;


	typedef struct Value
	{
		union
		{
			fp_t fp;
			int_t integer;
		};

		enum Type
		{
			V_INTEGER = 0,
			V_FLOAT,
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

		inline void Expect (Processor::Value::Type required_type, bool allow_uninitialised = 0) const;
		inline Type GenType (Type required_type) const
		{
			return (required_type == V_MAX) ? type : required_type;
		}

		// Verify type equality and assign contents of another value object to this.
		// Should not be used with registers, since they are untyped in JIT mode.
		inline void Assign (const Value& that)
		{
			__sassert (that.type != V_MAX, "Attempt to assign an uninitialised value");
			Expect (that.type);

			switch (GenType (that.type))
			{
				case V_FLOAT:
					fp = that.fp;
					break;

				case V_INTEGER:
					integer = that.integer;
					break;

				case V_MAX:
					break;

				default:
					__sasshole ("Switch error");
					break;
			}
		}

		// Verify type equality and assign the correct value to given reference.
		// Pass "V_MAX" as type to disable type checking.
		// API should use "allow_uninitialised" switch to read data from registers (since they are untyped in JIT mode).
		template <typename T> void Get (Type required_type, T& dest, bool allow_uninitialised = 0) const
		{
			Expect (required_type, allow_uninitialised);

			switch (GenType (required_type))
			{
			case V_FLOAT:
				dest = fp;
				break;

			case V_INTEGER:
				dest = integer;
				break;

			case V_MAX:
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
					int_abi_t tmp = integer;
					return reinterpret_cast<abiret_t&> (tmp);
				}

				case V_FLOAT:
				{
					fp_abi_t tmp = fp;
					fp_abi_t* ptmp = &tmp;
					return **reinterpret_cast<abiret_t**> (ptmp);
				}

				case V_MAX:
					__sasshole ("Uninitialised value is being read");
					break;

				default:
					__sasshole ("Switch error");
					break;
			}

			return 0;
		}

		void SetFromABI (abiret_t value)
		{
			abiret_t* pvalue = &value;

			switch (type)
			{
				case V_INTEGER:
					integer = **reinterpret_cast<int_abi_t**> (&pvalue);

				case V_FLOAT:
					fp = **reinterpret_cast<fp_abi_t**> (&pvalue);

				case V_MAX:
					__sasshole ("Uninitialised value is being set from ABI data");
					break;

				default:
					__sasshole ("Switch error");
					break;
			}
		}

		// Verify type equality and set correct value from given object.
		// Pass "V_MAX" as type to disable type checking.
		// API should use "allow_uninitialised" switch to write data to registers (since they are untyped in JIT mode).
		template <typename T> void Set (Type required_type, const T& src, bool allow_uninitialised = 0)
		{
			Expect (required_type, allow_uninitialised);

			switch (GenType (required_type))
			{
			case V_FLOAT:
				fp = src;
				break;

			case V_INTEGER:
				integer = src;
				break;

			case V_MAX:
			default:
				__sasshole ("Switch error");
				break;
			}
		}

		static int_t IntParse (const char* string)
		{
			char* endptr;
			unsigned char char_representation;
			long result;

			if ((string[0] == '\'') &&
				(char_representation = string[1]) &&
				(string[2] == '\'') &&
				!string[3])
			{
				result = static_cast<long> (char_representation);
			}

			else
			{
				errno = 0;
				result = strtol (string, &endptr, 0); /* base autodetermine */
				__sverify (!errno && (endptr != string) && (*endptr == '\0'),
						   "Malformed integer: \"%s\": non-ASCII and strtol() says \"%s\"",
						   string, strerror (errno));
			}

			return result;
		}

		static fp_t FPParse (const char* string)
		{
			char* endptr;
			fp_t result;
			int classification;

			errno = 0;
			result = strtof (string, &endptr);
			__sverify (!errno && (endptr != string) && (*endptr == '\0'),
					   "Malformed floating-point: \"%s\": strtof() says \"%s\"",
					   string, strerror (errno));

			classification = fpclassify (result);
			__sverify (classification == FP_NORMAL || classification == FP_ZERO,
					   "Invalid floating-point value: \"%s\" -> %lg", string, result);

			return result;
		}

		void Parse (const char* string)
		{
			switch (type)
			{
			case V_INTEGER:
				integer = IntParse (string);
				break;

			case V_FLOAT:
				fp = FPParse (string);
				break;

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

	class IExecutor;

	struct Command
	{
		union Argument
		{
			calc_t value;
			Reference ref;
		} arg;

		cid_t id;
		Value::Type type;

		void* cached_handle;
		IExecutor* cached_executor;

		Command() :
		arg ({}), id (0), type (Value::V_MAX), cached_handle (0), cached_executor (0) {}
	};

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

		bool is_service_command;

		// assigned by command set
		cid_t id;
		std::map<size_t, void*> execution_handles;
	};

	struct FileProperties
	{
		std::list< std::pair<FileSectionType, size_t> > file_description;
		FILE* file;

		void Reset()
		{
			if (file) rewind (file);
			file_description.clear();
			file_description.push_back (std::make_pair (SEC_MAX, 0)); // placeholder
		}

		FileProperties (const FileProperties&) = delete;
		FileProperties& operator= (const FileProperties&) = delete;

		FileProperties (FILE* target_file) :
		file (target_file)
		{
			Reset();
		}

		FileProperties (FileProperties&& that) :
		file_description (std::move (that.file_description)),
		file (that.file)
		{
			that.file = 0;
			that.file_description.clear();
		}

		FileProperties& operator= (FileProperties&& that)
		{
			if (this == &that)
				return *this;

			if (file) fclose (file);
			file_description.clear();

			file = that.file; that.file = 0;
			file_description = std::move (that.file_description);
			that.file_description.clear();

			return *this;
		}

		~FileProperties()
		{
			if (file) fclose (file);
		}
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
			DEC_NOTHING = 0,
			DEC_COMMAND,
			DEC_DATA
		} type;

		symbol_map mentioned_symbols;

		DecodeResult() : command(), type (DEC_NOTHING), mentioned_symbols() {}
	};

	class IReader;
	class IWriter;
	class IMMU;
	class ILinker;
	class IExecutor;
	class IBackend;
	class ICommandSet;
	class ILogic;
	class IModuleBase;

	calc_t ExecuteGate (void* address);

	inline void InsertSymbol (const Symbol& symbol, const char* name, symbol_map& target_map)
	{
		target_map.insert (std::make_pair (symbol.hash, std::make_pair (std::string (name), symbol)));
	}

	namespace ProcDebug
	{
		INTERPRETER_API extern const char* FileSectionType_ids[SEC_MAX]; // debug section IDs
		INTERPRETER_API extern const char* AddrType_ids[S_MAX]; // debug address type IDs
		INTERPRETER_API extern const char* ValueType_ids[Value::V_MAX + 1]; // debug value type IDs

		INTERPRETER_API extern char debug_buffer[STATIC_LENGTH];

		INTERPRETER_API void PrintReference (const Reference& ref, IMMU* mmu = 0);
		INTERPRETER_API void PrintReference (const Processor::DirectReference& ref);
		INTERPRETER_API void PrintValue (const Value& val);
		INTERPRETER_API void PrintArgument (ArgumentType arg_type,
											const Command::Argument& argument,
											IMMU* mmu = 0);
	}

	void Value::Expect (Processor::Value::Type required_type, bool allow_uninitialised) const
	{
		if (!allow_uninitialised)
			__sverify (type != V_MAX, "Cannot access uninitialised value");

		else
		{
			__sassert (required_type != V_MAX, "Type autodetermine requested while uninitialised values are allowed");

			if (type == V_MAX)
				return;

			__sverify (type == required_type,
					   "Cannot operate on non-matching types (expected \"%s\" instead of \"%s\")",
					   ProcDebug::ValueType_ids[required_type], ProcDebug::ValueType_ids[type]);
		}
	}
} // namespace Processor

#endif // _UTILITY_H

// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
