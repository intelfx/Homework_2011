#ifndef INTERPRETER_DEFS_H
#define INTERPRETER_DEFS_H

#include "build.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			Defs.h
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Primary static definitions.
// -------------------------------------------------------------------------------------

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

// Main processing types

#if defined (TARGET_X64)
typedef double fp_t; // main floating-point type
typedef long int_t; // main integer type
#elif defined (TARGET_X86)
typedef float fp_t;
typedef long int_t;
#endif

static_assert( sizeof( fp_t ) == sizeof( int_t ),
               "FP data type size does not equal integer data type size" );

// ABI types

/*
 * ABI conversion scheme:
 *  first conversion is a valid C cast (with precision loss)
 *  second conversion is type punning to integer type (same size, but exact copying)
 *  fp_t-> fp_abi_t-> abiret_t
 *  int_t-> int_abi_t-> abiret_t
 */

typedef float fp_abi_t; // ABI intermediate floating-point type
typedef unsigned int_abi_t; // ABI intermediate integer type

typedef unsigned abiret_t; // final ABI interaction type


static_assert( sizeof( fp_abi_t ) == sizeof( abiret_t ),
               "ABI data type size does not equal FP intermediate data type size" );

static_assert( sizeof( int_abi_t ) == sizeof( abiret_t ),
               "ABI data type size does not equal integer intermediate data type size" );

// API types

typedef unsigned short cid_t; // command identifier type
typedef unsigned long ctx_t; // context identifier type

// Possible plugin types

class IReader;
class IWriter;
class IMMU;
class ILinker;
class IExecutor;
class IBackend;
class ICommandSet;
class ILogic;
class IModuleBase;

// Utilitary constants

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

enum ArgumentType
{
	A_NONE = 0,
	A_VALUE,
	A_REFERENCE
};

const Register indirect_addressing_register = R_F;

enum AddrType
{
	S_NONE = 0,
	S_CODE,
	S_DATA,
	S_REGISTER,
	S_BYTEPOOL,
	S_FRAME,
	S_FRAME_BACK, // Parameters to function
	S_MAX
};

enum FileType
{
	FT_BINARY,
	FT_STREAM,
	FT_NON_UNIFORM
};

enum MemorySectionType
{
	SEC_CODE_IMAGE = 1,
	SEC_DATA_IMAGE,
	SEC_BYTEPOOL_IMAGE,
	SEC_SYMBOL_MAP,
	SEC_STACK_IMAGE,
	SEC_MAX
};

// Utilitary structures pre-declarations
class MemorySectionIdentifier;
class Offsets;
struct Reference;
struct DirectReference;
struct Symbol;
struct Command;
struct Context;
struct CommandTraits;
struct DecodeResult;

} // namespace Processor

#endif // INTERPRETER_DEFS_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
