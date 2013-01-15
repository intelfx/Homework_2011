#ifndef INTERPRETER_X86BACKEND_SIB_H
#define INTERPRETER_X86BACKEND_SIB_H

#include "build.h"

#include "Defs.h"
#include "Registers.h"

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		SIB.h
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	x86 JIT backend: SIB opcode part definitions.
// -------------------------------------------------------------------------------------

namespace x86backend
{

enum class IndexRegs : unsigned char
{
	RAX  = 0,
	RCX  = 1,
	RDX  = 2,
	RBX  = 3,
	None = 4,
	RBP  = 5,
	RSI  = 6,
	RDI  = 7
};

enum class BaseRegs : unsigned char
{
	RAX       = 0,
	RCX       = 1,
	RDX       = 2,
	RBX       = 3,
	RSP       = 4,
	RBPOrNone = 5, // [RBP] base becomes zero when modrm:mod = 0
	RSI       = 6,
	RDI       = 7
};

struct SIB
{
	unsigned char base : 3;
	unsigned char index : 3;
	unsigned char scale : 2;
} PACKED;
static_assert( sizeof( SIB ) == 1, "SIB structure is not packed" );

struct SIBWrapper
{
	static inline unsigned char _ScaleToField( unsigned char scale )
	{
		s_cassert( scale >= 1, "Invalid scale: %hhu", scale );
		s_cassert( scale <= 8, "Invalid scale: %hhu", scale );
		s_cassert( scale == 1 || !( scale % 2 ), "Invalid scale: %hhu", scale );
		unsigned char ret = 0;
		while( scale > 1 ) {
			++ret;
			scale /= 2;
		}
		s_cassert( ret <= 3, "Got invalid scale field: %hhu", ret );
		return ret;
	}

	union {
		BaseRegs base;
		Reg64E base64e;
		unsigned char base_raw;
	};

	union {
		IndexRegs index;
		Reg64E index64e;
		unsigned char index_raw;
	};

	unsigned char scale;

	bool need_base_extension;
	bool need_index_extension;

	bool valid;

	SIBWrapper() :
	base_raw( 0 ),
	index_raw( 0 ),
	scale( 0 ),
	need_base_extension( false ),
	need_index_extension( false ),
	valid( false )
	{
	}

	SIBWrapper( BaseRegs base_reg, IndexRegs index_reg, unsigned char scale_factor = 1 ) :
	base( base_reg ),
	index( index_reg ),
	scale( _ScaleToField( scale_factor ) ),
	need_base_extension( false ),
	need_index_extension( false ),
	valid( true )
	{
	}

	SIBWrapper( Reg64E base_reg, IndexRegs index_reg, unsigned char scale_factor = 1 ) :
	base64e( base_reg ),
	index( index_reg ),
	scale( _ScaleToField( scale_factor ) ),
	need_base_extension( true ),
	need_index_extension( false ),
	valid( true )
	{
	}

	SIBWrapper( BaseRegs base_reg, Reg64E index_reg, unsigned char scale_factor = 1 ) :
	base( base_reg ),
	index64e( index_reg ),
	scale( _ScaleToField( scale_factor ) ),
	need_base_extension( false ),
	need_index_extension( true ),
	valid( true )
	{
	}

	SIBWrapper( Reg64E base_reg, Reg64E index_reg, unsigned char scale_factor = 1 ) :
	base64e( base_reg ),
	index64e( index_reg ),
	scale( _ScaleToField( scale_factor ) ),
	need_base_extension( true ),
	need_index_extension( true ),
	valid( true )
	{
	}
};

} // namespace x86backend

#endif // INTERPRETER_X86BACKEND_SIB_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
