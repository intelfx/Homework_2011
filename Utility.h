#ifndef INTERPRETER_UTILITY_H
#define INTERPRETER_UTILITY_H

#include "build.h"

#include "Value.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			Utility.h
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Utilitary structures and definitions.
// -------------------------------------------------------------------------------------

namespace Processor
{

static_assert( SEC_MAX - SEC_STACK_IMAGE == 1,
               "Invalid section indices disposition: SEC_STACK_IMAGE shall be the last entry" );
static const size_t SEC_COUNT = SEC_STACK_IMAGE + Value::V_MAX;

namespace ProcDebug
{

INTERPRETER_API extern const char* FileSectionType_ids[SEC_MAX]; // debug section IDs
INTERPRETER_API extern const char* AddrType_ids[S_MAX]; // debug address type IDs

INTERPRETER_API extern char debug_buffer[STATIC_LENGTH];

INTERPRETER_API void PrintReference( const Reference& ref, IMMU* mmu = 0 );
INTERPRETER_API void PrintReference( const Processor::DirectReference& ref );
INTERPRETER_API void PrintValue( const Value& val );

} // namespace ProcDebug

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
		union {
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
		union {
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


	Symbol( const char* name ) :
		hash( crc32_runtime( name ) ), ref(), is_resolved( 0 ) {}

	Symbol( const char* name, const Reference& resolved_reference ) :
		hash( crc32_runtime( name ) ), ref( resolved_reference ), is_resolved( 1 ) {}


	bool operator== ( const Symbol& that ) const { return hash == that.hash; }
	bool operator!= ( const Symbol& that ) const { return hash != that.hash; }
};

// This is something like TR1's std::unordered_map with manual hashing,
// since we need to have direct access to hashes themselves.
typedef std::map<size_t, std::pair<std::string, Symbol> > symbol_map;
typedef symbol_map::value_type::second_type symbol_type;


struct Command
{
	union Argument
	{
		calc_t value;
		Reference ref;
	} arg;

	/*
	 * The virtual command opcode in current command set.
	 */
	cid_t id;

	/*
	 * Type of the command is which stack it is operating with.
	 * V_MAX represents "no stack ops".
	 */
	Value::Type type;

	/*
	 * Executor module for command is selected coherent with command type and service flag.
	 * Together with handle, it represents actual command opcode.
	 */
	IExecutor* cached_executor;
	void* cached_handle;

	Command() :
		arg( {} ), id( 0 ), type( Value::V_MAX ), cached_executor( 0 ), cached_handle( 0 ) {}
};

namespace ProcDebug
{

INTERPRETER_API void PrintArgument( ArgumentType arg_type,
                                    const Command::Argument& argument,
                                    IMMU* mmu = 0 );
} // namespace ProcDebug

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

	CommandTraits( const char* mnem, const char* desc, ArgumentType arg, bool is_service ) :
	mnemonic( mnem ),
	description( desc ),
	arg_type( arg ),
	is_service_command( is_service ),
	id( 0 ),
	execution_handles()
	{
	}
};

struct DecodeResult
{
	std::vector<Command> commands;
	std::vector<calc_t> data;
	std::vector<char> bytepool;

	symbol_map mentioned_symbols;

	void Clear()
	{
		commands.clear();
		data.clear();
		bytepool.clear();

		mentioned_symbols.clear();
	}
};

calc_t ExecuteGate( void* address );

inline void InsertSymbol( const Symbol& symbol, const char* name, symbol_map& target_map )
{
	target_map.insert( std::make_pair( symbol.hash, std::make_pair( std::string( name ), symbol ) ) );
}

} // namespace Processor

#endif // INTERPRETER_UTILITY_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
