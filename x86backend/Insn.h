#ifndef INTERPRETER_X86BACKEND_INSN_H
#define INTERPRETER_X86BACKEND_INSN_H

#include "build.h"

#include "Defs.h"
#include "Registers.h"
#include "REX.h"
#include "ModRM.h"
#include "SIB.h"

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

	unsigned char prefix_[PREFIXES_COUNT];

	REX rex_;
	unsigned char opcode_;
	ModRM modrm_;
	SIB sib_;

	std::vector<OperandType> operands_;

	union {
		int8_t displacement_8_;
		int16_t displacement_16_;
		int32_t displacement_32_;
	};

	llarray immediates_;

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
		bool used_modrm_reg : 1;
		bool used_modrm_rm : 1;
		bool used_opcode_reg : 1;
	} PACKED flags_;

	// Set prefixes according to flags.
	// Prefixes modified: REX, operand size override.
	void GeneratePrefixes() {
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

	void AddOperand( AddressSize size, OperandType type)
	{
		if( operands_.empty() ) {
			flags_.operand_size = size;
		}
		operands_.push_back( type );
	}

public:

	Insn()
	{
		reinterpret_cast<uint32_t&>( prefix_ ) = 0;
		reinterpret_cast<unsigned char&>( rex_ ) = 0;
		opcode_ = 0;
		reinterpret_cast<unsigned char&>( modrm_ ) = 0;
// 		reinterpret_cast<unsigned char&>( sib_ ) = 0;
	}

	Insn& SetIsDefault64Bit( bool arg = true )
	{
		flags_.is_default_64bit_opsize = arg;
		return *this;
	}

	Insn& SetNeedREX( bool arg = true )
	{
		flags_.unconditionally_need_rex = arg;
		return *this;
	}

	Insn& SetPrefix( Prefixes::GeneralPurpose p )
	{
		prefix_[0] = static_cast<unsigned char>( p );
		return *this;
	}

	Insn& SetPrefix( Prefixes::SegmentOverride p )
	{
		prefix_[1] = static_cast<unsigned char>( p );
		return *this;
	}

	Insn& SetPrefix( Prefixes::Special p )
	{
		prefix_[2] = static_cast<unsigned char>( p );
		return *this;
	}

	Insn& SetOperandSize( AddressSize s = AddressSize::DWORD /* the default one */ )
	{
		flags_.operand_size = s;
		return *this;
	}

	Insn& SetOpcode( unsigned char opcode )
	{
		opcode_ = opcode;
		return *this;
	}

	Insn& SetOpcodeExtension( unsigned char opcode_ext )
	{
		s_cassert( !flags_.used_modrm_reg, "Cannot set opcode extension: reg field already taken" );
		modrm_.reg = 0x7 & opcode_ext;

		flags_.used_modrm_reg = true;
		return *this;
	}

	Insn& AddRegister( RegisterWrapper reg )
	{
		if( flags_.used_modrm_reg && !flags_.used_modrm_rm ) {
			return AddRM( reg );
		}

		s_cassert( !flags_.used_modrm_reg, "Cannot set register: reg field already taken" );
		modrm_.reg = 0x7 & reg.raw;

		flags_.used_modrm_reg = true;
		flags_.need_modrm_reg_extension = reg.need_extension;
		AddOperand( reg.operand_size, OperandType::Register );
		return *this;
	}

	Insn& AddOpcodeRegister( RegisterWrapper reg )
	{
		s_cassert( !flags_.used_opcode_reg, "Cannot set register in opcode: lower bits of opcode already taken" );
		s_cassert( !( 0x7 & opcode_ ), "Cannot set register in opcode: lower bits of opcode are non-zero" );
		opcode_ |= ( 0x7 & reg.raw );

		flags_.used_opcode_reg = true;
		flags_.need_opcode_reg_extension = reg.need_extension;
		AddOperand( reg.operand_size, OperandType::OpcodeRegister );
		return *this;
	}

	Insn& AddRM( ModRMWrapper rm )
	{
		s_cassert( !modrm_.rm, "Cannot set r/m: r/m field already taken" );
		s_cassert( !modrm_.mod, "Cannot set r/m: mod field already taken" );
		modrm_.rm = 0x7 & rm.raw;
		modrm_.mod = rm.mod;

		flags_.used_modrm_rm = true;
		flags_.need_modrm_rm_extension = rm.need_extension;
		AddOperand( AddressSize::QWORD, OperandType::RegMem );
		return *this;
	}

	Insn& AddImmediate( ImmediateUnsignedWrapper imm )
	{
		switch( imm.size ) {
		case AddressSize::BYTE:
			immediates_.append( 1, &imm.i8 );
			break;

		case AddressSize::WORD:
			immediates_.append( 2, &imm.i16 );
			break;

		case AddressSize::DWORD:
			immediates_.append( 4, &imm.i32 );
			break;

		case AddressSize::QWORD:
			immediates_.append( 8, &imm.i64 );
			break;
		}
		AddOperand( imm.size, OperandType::Immediate );
		return *this;
	}

	Insn& SetDisplacement( ImmediateSignedWrapper disp )
	{
		s_cassert( disp.size != AddressSize::QWORD, "Displacement cannot be QWORD" );
		s_cassert( disp.size != AddressSize::WORD, "Displacement cannot be WORD" );
		displacement_32_ = disp.i32;
		return *this;
	}

	llarray Emit()
	{
		llarray ret;

		// Prefixes
		GeneratePrefixes();
		for( size_t i = 0; i < PREFIXES_COUNT; ++i ) {
			if( prefix_[i] ) {
				ret.append( 1, &prefix_[i] );
			}
		}
		if( rex_.Enabled() ) {
			ret.append( 1, &rex_ );
		}

		// Opcode
		ret.append( 1, &opcode_ );

		// ModR/M and displacement
		if( flags_.used_modrm_reg || flags_.used_modrm_rm ) {
			ret.append( 1, &modrm_ );

			if( modrm_.UsingSIB() ) {
				ret.append( 1, &sib_ );
			}

			switch( modrm_.UsingDisplacement() ) {
			case ModField::Direct:
			case ModField::NoShift:
				/* no-op */
				break;

			case ModField::Disp8:
				ret.append( sizeof( displacement_8_ ), &displacement_8_ );
				break;

			case ModField::Disp32:
				ret.append( sizeof( displacement_32_ ), &displacement_32_ );
				break;
			}
		}

		// Immediates
		ret.append( immediates_ );

		return ret;
	}
};

} // namespace x86backend

#endif // INTERPRETER_X86BACKEND_INSN_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
