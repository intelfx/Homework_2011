#ifndef INTERPRETER_X86BACKEND_INSN_H
#define INTERPRETER_X86BACKEND_INSN_H

#include "build.h"

#include "Defs.h"
#include "Registers.h"
#include "REX.h"
#include "ModRM.h"

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		Insn.h
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	x86 JIT backend: instruction emitter class definition.
// -------------------------------------------------------------------------------------

namespace x86backend
{

class Insn
{
	static const size_t PREFIXES_COUNT = 4;
	static const size_t OPERANDS_COUNT = 3;

	unsigned char prefix_[PREFIXES_COUNT];

	REX rex_;
	unsigned char opcode_;
	ModRM modrm_;

	OperandType operands_[OPERANDS_COUNT];

	// Opcode generation parameters.
	struct {
		AddressSize operand_size;
		bool is_default_64bit_opsize : 1;
		bool unconditionally_need_rex : 1;
		bool need_opcode_reg_extension : 1;
		bool need_modrm_reg_extension : 1;
		bool need_modrm_rm_extension : 1;
		bool need_sib_base_extension : 1;
		bool need_sib_index_extension : 1;
	} flags_;

	// Set prefixes according to flags.
	// Prefixes modified: REX, operand size override.
	void ProcessFlags() {
		switch( flags_.operand_size ) {
		case AddressSize::BYTE:
			/* shall be handled in the opcode itself; no-op */
			break;
		case AddressSize::WORD:
			prefix_[2] = static_cast<unsigned char>( Prefixes::SizeOverride::Operand );
			break;
		case AddressSize::DWORD:
			/* no-op */
			break;
		case AddressSize::QWORD:
			if( !flags_.is_default_64bit_opsize ) {
				rex_.w = true;
			}
			break;
		}

		if( flags_.need_opcode_reg_extension ) {
			s_cassert( !rex_.b, "Cannot set opcode reg field extension: REX.B field already used" );
			rex_.b = true;
		}
		if( flags_.need_modrm_reg_extension ) {
			s_cassert( !rex_.r, "Cannot set ModR/M reg field extension: REX.R field already used" );
			rex_.r = true;
		}
		if( flags_.need_modrm_rm_extension ) {
			s_cassert( !rex_.b, "Cannot set ModR/M r/m field extension: REX.B field already used" );
			rex_.b = true;
		}
		if( flags_.need_sib_base_extension ) {
			s_cassert( !rex_.b, "Cannot set SIB base field extension: REX.B field already used" );
			rex_.b = true;
		}
		if( flags_.need_sib_index_extension ) {
			s_cassert( !rex_.x, "Cannot set SIB index field extension: REX.X field already used" );
			rex_.x = true;
		}

		if( flags_.unconditionally_need_rex || rex_.IsSet() ) {
			rex_.Enable();
		}
	}

public:

	Insn() {
		reinterpret_cast<uint32_t&>( prefix_ ) = 0;
		reinterpret_cast<unsigned char&>( rex_ ) = 0;
		opcode_ = 0;
		reinterpret_cast<unsigned char&>( modrm_ ) = 0;
// 		reinterpret_cast<unsigned char&>( sib_ ) = 0;
	}

	void SetPrefix( Prefixes::GeneralPurpose p ) {
		prefix_[0] = static_cast<unsigned char>( p );
	}

	void SetPrefix( Prefixes::SegmentOverride p ) {
		prefix_[1] = static_cast<unsigned char>( p );
	}

	void SetPrefix( Prefixes::Special p ) {
		prefix_[2] = static_cast<unsigned char>( p );
	}

	void SetOperandSize( AddressSize s )
	{
		flags_.operand_size = s;
	}

	void SetOpcode( unsigned char opcode )
	{
		opcode_ = opcode;
	}

	void SetOpcodeExtension( unsigned char opcode_ext )
	{
		s_cassert( !modrm_.reg, "Cannot set opcode extension: reg field already taken" );
		modrm_.reg = 0x7 & opcode_ext;
	}

	void SetOperand( size_t index, OperandType type )
	{
		s_cassert( index < OPERANDS_COUNT, "Invalid operand index: %zu", index );
		operands_[index] = type;
	}

	void SetRegister( RegisterWrapper reg )
	{
		s_cassert( !modrm_.reg, "Cannot set register: reg field already taken" );
		modrm_.reg = 0x7 & reg.raw;

		if( operands_[0] == OperandType::Register ) {
			SetOperandSize( reg.operand_size );
		}
		flags_.need_modrm_reg_extension = reg.need_extension;
	}

	void SetOpcodeRegister( RegisterWrapper reg )
	{
		s_cassert( !( 0x7 & opcode_ ), "Cannot set register in opcode: lower bits of opcode are non-zero" );
		opcode_ &= ( 0x7 & reg.raw );

		if( operands_[0] == OperandType::OpcodeRegister ) {
			SetOperandSize( reg.operand_size );
		}
		flags_.need_opcode_reg_extension = reg.need_extension;
	}

	void SetRM( ModRMWrapper rm )
	{
		s_cassert( !modrm_.rm, "Cannot set r/m: r/m field already taken" );
		modrm_.rm = 0x7 & rm.raw;
		modrm_.mod = static_cast<unsigned char>( rm.mod );

		if( operands_[0] == OperandType::RegMem ) {
			SetOperandSize( AddressSize::QWORD );
		}
		flags_.need_modrm_rm_extension = rm.need_extension;
	}
};

} // namespace x86backend

#endif // INTERPRETER_X86BACKEND_INSN_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
