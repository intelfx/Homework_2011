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

INTERPRETER_API std::string PrintReference( const Reference& ref, IMMU* mmu = 0 );
INTERPRETER_API std::string PrintReference( const DirectReference& ref );
INTERPRETER_API std::string PrintValue( const Value& val );

INTERPRETER_API std::string Print( MemorySectionType arg );
INTERPRETER_API std::string Print( AddrType arg );

} // namespace ProcDebug

class MemorySectionIdentifier
{
	MemorySectionType section_type_;
	Value::Type data_type_;

public:
	MemorySectionIdentifier( MemorySectionType section_type, Value::Type data_type = Value::V_MAX ) :
	section_type_( section_type ),
	data_type_( data_type )
	{
		s_cassert( section_type_ != SEC_MAX, "Invalid section type given" );
		if( section_type_ == SEC_STACK_IMAGE ) {
			s_cassert( data_type_ != Value::V_MAX, "Data type not specified with stack section" );
		} else {
			s_cassert( data_type_ == Value::V_MAX, "Data type specified (\"%s\") with non-stack section",
				       ProcDebug::Print( data_type_ ).c_str() );
		}
	}

	MemorySectionIdentifier( AddrType addr_type, Value::Type data_type = Value::V_MAX ) :
	MemorySectionIdentifier( TranslateSection( addr_type ), data_type )
	{
	}

	MemorySectionIdentifier() :
	section_type_( SEC_MAX ),
	data_type_( Value::V_MAX )
	{
	}

	size_t Index() const
	{
		s_cassert( isValid(), "Dereferencing an invalid identifier" );
		if( section_type_ == SEC_STACK_IMAGE ) {
			s_cassert( data_type_ != Value::V_MAX, "Data type not specified with stack section" );
		} else {
			s_cassert( data_type_ == Value::V_MAX, "Data type specified (\"%s\") with non-stack section",
				       ProcDebug::Print( data_type_ ).c_str() );
		}
		size_t idx = ( section_type_ == SEC_STACK_IMAGE ) ? ( section_type_ + data_type_ ) : section_type_;
		s_cassert( idx < SEC_COUNT, "Invalid section index generated (section \"%s\" data type \"%s\") == %zu",
				   ProcDebug::Print( section_type_ ).c_str(), ProcDebug::Print( data_type_ ).c_str(), idx );
		return idx;
	}

	bool isValid() const
	{
		return section_type_ != SEC_MAX;
	}

	operator bool() const { return isValid(); }

	MemorySectionType SectionType() const { return section_type_; }
	Value::Type DataType() const { return data_type_; }
};

class Offsets
{
	size_t offsets_[SEC_COUNT];

public:
	Offsets()
	{ memset( offsets_, 0, sizeof( *offsets_ ) * SEC_COUNT ); }

	Offsets( const size_t offsets[SEC_COUNT] )
	{ memcpy( offsets_, offsets, sizeof( *offsets_ ) * SEC_COUNT ); }

	Offsets( const Offsets& rhs ) : Offsets( rhs.offsets_ )
	{}

	size_t& at( const MemorySectionIdentifier& index )
	{ return offsets_[index.Index()]; }
	size_t  at( const MemorySectionIdentifier& index ) const
	{ return offsets_[index.Index()]; }

	size_t& operator[]( const MemorySectionIdentifier& index )
	{ return at( index ); }
	size_t operator[]( const MemorySectionIdentifier& index ) const
	{ return at( index ); }

	size_t& Code()                          { return at( SEC_CODE_IMAGE ); }
	size_t  Code() const                    { return at( SEC_CODE_IMAGE ); }
	size_t& Data()                          { return at( SEC_DATA_IMAGE ); }
	size_t  Data() const                    { return at( SEC_DATA_IMAGE ); }
	size_t& Bytepool()                      { return at( SEC_BYTEPOOL_IMAGE ); }
	size_t  Bytepool() const                { return at( SEC_BYTEPOOL_IMAGE ); }
	size_t& Stack( Value::Type type )       { return at( MemorySectionIdentifier( SEC_STACK_IMAGE, type ) ); }
	size_t  Stack( Value::Type type ) const { return at( MemorySectionIdentifier( SEC_STACK_IMAGE, type ) ); }

	const size_t* Raw() { return offsets_; }
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
		union {
			size_t	symbol_hash;
			size_t memory_address;
		};

		enum BaseRefType {
			BRT_SYMBOL,
			BRT_DEFINITION,
			BRT_MEMORY_REF
		} type;
	};

	struct SingleRef
	{
		BaseRef		target;
		AddrType	indirection_section; // If not S_NONE, then this is an indirect reference
	};

	AddrType global_section;
	SingleRef components[2];

	/* one can iterate components with such loop:
	 * for (int i = 0; i <= reference.has_second_component; ++i)
	 */
	bool has_second_component;
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

INTERPRETER_API std::string PrintArgument( ArgumentType arg_type,
                                           const Command::Argument& argument,
                                           IMMU* mmu = 0 );
} // namespace ProcDebug

struct Context
{
	mask_t flags;
	size_t ip;
	union
	{
		// the r/w is positioned before r/o to make the union (hence the struct) have the
		// default assignment operator/ctor.
		// TODO: make this really work and eliminate the assignment op below.
		ctx_t __buffer_rw;
		const ctx_t buffer;
	};
	size_t depth;
	size_t frame;

	Context& operator=( const Context& rhs )
	{
		flags = rhs.flags;
		ip = rhs.ip;
		__buffer_rw = rhs.buffer;
		depth = rhs.depth;
		frame = rhs.frame;
		return *this;
	}
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

calc_t ExecuteGate( abi_native_fn_t jit_compiled_function );

inline void InsertSymbol( const Symbol& symbol, const char* name, symbol_map& target_map )
{
	target_map.insert( std::make_pair( symbol.hash, std::make_pair( std::string( name ), symbol ) ) );
}

} // namespace Processor

#endif // INTERPRETER_UTILITY_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
