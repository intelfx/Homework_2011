#ifndef INTERPRETER_VALUE_H
#define INTERPRETER_VALUE_H

#include "build.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			Value.h
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Interpreter's uniform value class implementation.
// -------------------------------------------------------------------------------------

namespace Processor
{

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
		type( V_MAX )
	{
	}

	Value( fp_t src ) :
		type( V_FLOAT )
	{
		fp = src;
	}

	Value( int_t src ) :
		type( V_INTEGER )
	{
		integer = src;
	}

	inline void Expect( Processor::Value::Type required_type, bool allow_uninitialised = 0 ) const;
	inline Type GenType( Type required_type ) const
	{
		return ( required_type == V_MAX ) ? type : required_type;
	}

	// Verify type equality and assign contents of another value object to this.
	// Should not be used with registers, since they are untyped in JIT mode.
	inline void Assign( const Value& that ) {
		s_cassert( that.type != V_MAX, "Attempt to assign an uninitialised value" );
		Expect( that.type );

		switch( GenType( that.type ) ) {
		case V_FLOAT:
			fp = that.fp;
			break;

		case V_INTEGER:
			integer = that.integer;
			break;

		case V_MAX:
			break;

		default:
			s_casshole( "Switch error" );
			break;
		}
	}

	// Verify type equality and assign the correct value to given reference.
	// Pass "V_MAX" as type to disable type checking.
	// API should use "allow_uninitialised" switch to read data from registers (since they are untyped in JIT mode).
	template <typename T> void Get( Type required_type, T& dest, bool allow_uninitialised = 0 ) const
	{
		Expect( required_type, allow_uninitialised );

		switch( GenType( required_type ) ) {
		case V_FLOAT:
			dest = fp;
			break;

		case V_INTEGER:
			dest = integer;
			break;

		case V_MAX:
		default:
			s_casshole( "Switch error" );
			break;
		}
	}

	abiret_t GetABI() const
	{
		switch( type ) {
		case V_INTEGER: {
			int_abi_t tmp = integer;
			return reinterpret_cast<abiret_t&>( tmp );
		}

		case V_FLOAT: {
			fp_abi_t tmp = fp;
			fp_abi_t* ptmp = &tmp;
			return **reinterpret_cast<abiret_t**>( ptmp );
		}

		case V_MAX:
			s_casshole( "Uninitialised value is being read" );
			break;

		default:
			s_casshole( "Switch error" );
			break;
		}

		return 0;
	}

	void SetFromABI( abiret_t value )
	{
		abiret_t* pvalue = &value;

		switch( type ) {
		case V_INTEGER:
			integer = **reinterpret_cast<int_abi_t**>( &pvalue );
			break;

		case V_FLOAT:
			fp = **reinterpret_cast<fp_abi_t**>( &pvalue );
			break;

		case V_MAX:
			s_casshole( "Uninitialised value is being set from ABI data" );
			break;

		default:
			s_casshole( "Switch error" );
			break;
		}
	}

	// Verify type equality and set correct value from given object.
	// Pass "V_MAX" as type to disable type checking.
	// API should use "allow_uninitialised" switch to write data to registers (since they are untyped in JIT mode).
	template <typename T> void Set( Type required_type, const T& src, bool allow_uninitialised = 0 )
	{
		Expect( required_type, allow_uninitialised );

		switch( GenType( required_type ) ) {
		case V_FLOAT:
			fp = src;
			break;

		case V_INTEGER:
			integer = src;
			break;

		case V_MAX:
		default:
			s_casshole( "Switch error" );
			break;
		}
	}

	static int_t IntParse( const char* string )
	{
		char* endptr;
		unsigned char char_representation;
		long result;

		if( ( string[0] == '\'' ) &&
		    ( char_representation = string[1] ) &&
		    ( string[2] == '\'' ) &&
		    !string[3] ) {
			result = static_cast<long>( char_representation );
		}

		else {
			errno = 0;
			result = strtol( string, &endptr, 0 ); /* base autodetermine */
			s_cverify( !errno && ( endptr != string ) && ( *endptr == '\0' ),
			           "Malformed integer: \"%s\": non-ASCII and strtol() says \"%s\"",
			           string, strerror( errno ) );
		}

		return result;
	}

	static fp_t FPParse( const char* string )
	{
		char* endptr;
		fp_t result;
		int classification;

		errno = 0;
		result = strtof( string, &endptr );
		s_cverify( !errno && ( endptr != string ) && ( *endptr == '\0' ),
		           "Malformed floating-point: \"%s\": strtof() says \"%s\"",
		           string, strerror( errno ) );

		classification = fpclassify( result );
		s_cverify( classification == FP_NORMAL || classification == FP_ZERO,
		           "Invalid floating-point value: \"%s\" -> %lg", string, result );

		return result;
	}

	void Parse( const char* string )
	{
		switch( type ) {
		case V_INTEGER:
			integer = IntParse( string );
			break;

		case V_FLOAT:
			fp = FPParse( string );
			break;

		case V_MAX:
			s_casshole( "Uninitialised value" );
			break;

		default:
			s_casshole( "Switch error" );
			break;
		}
	}
} calc_t;

namespace ProcDebug
{

INTERPRETER_API std::string Print( Value::Type arg );

} // namespace ProcDebug

void Value::Expect( Processor::Value::Type required_type, bool allow_uninitialised ) const
{
	if( !allow_uninitialised )
		s_cverify( type != V_MAX, "Cannot access uninitialised value" );

	else {
		s_cassert( required_type != V_MAX, "Type autodetermine requested while uninitialised values are allowed" );

		if( type == V_MAX )
			return;

		s_cverify( type == required_type,
		           "Cannot operate on non-matching types (expected \"%s\" instead of \"%s\")",
		           ProcDebug::Print( required_type ).c_str(), ProcDebug::Print( type ).c_str() );
	}
}

} // namespace Processor


#endif // INTERPRETER_VALUE_H
