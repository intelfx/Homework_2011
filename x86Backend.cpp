#include "stdafx.h"
#include "x86Backend.h"

#include "x86backend/Insn.h"

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		x86Backend.h
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	x86 JIT compiler backend.
// -------------------------------------------------------------------------------------

namespace ProcessorImplementation
{
using namespace Processor;
using namespace x86backend;

bool x86Backend::ImageIsOK( size_t chk )
{
	msg( E_INFO, E_DEBUG, "Verifying image for checksum %zx", chk );

	auto it = images_.find( chk );
	if( it == images_.end() ) {
		msg( E_WARNING, E_DEBUG, "Image for checksum %zx does not exist", chk );
		return false;
	}
	if( !it->second.mm.length ) {
		msg( E_WARNING, E_DEBUG, "Image for checksum %zx is empty", chk );
		return false;
	}
	if( !it->second.mm.image ) {
		msg( E_WARNING, E_DEBUG, "Image for checksum %zx is not finalized", chk );
		return false;
	}
	if( it->second.data.size() != it->second.mm.length ) {
		msg( E_WARNING, E_DEBUG, "Image for checksum %zx is out of sync", chk );
		return false;
	}
	return true;
}

bool x86Backend::_Verify() const
{
// 	verify_statement( current_image_, "No image is selected" );
// 	auto it = images_.find( current_chk_ );
// 	verify_statement( it != images_.end(), "Image not found for the current checksum %zx", current_chk_ );
// 	verify_statement( &it->second == current_image_, "Looked up image does not match selected (chk %zx)", current_chk_ );
	return true;
}

abi_native_fn_t x86Backend::GetImage( size_t chk )
{
	cassert( ImageIsOK( chk ), "Image for checksum %zx is not good/ready", chk );
	return reinterpret_cast<abi_native_fn_t>( images_.find( chk )->second.mm.image );
}

void x86Backend::Select( size_t chk, bool create )
{
	auto it = images_.find( chk );
	if( it == images_.end() ) {
		cassert( create, "Selecting inexistent image chk %zx when create == false", chk );
		it = images_.insert( std::make_pair( chk, NativeImage() ) ).first;
	}
	current_chk_ = chk;
	current_image_ = &it->second;

	verify_method;
}

void x86Backend::Deallocate()
{
	verify_method;

	if( current_image_->mm.image ) {
		proc_->ExecutionManager().DeallocateMemory( current_image_->mm.image, current_image_->mm.length );
		current_image_->mm.image = 0;
	}
}

void x86Backend::Finalize()
{
	verify_method;

	Deallocate();

	char* img = reinterpret_cast<char*>( proc_->ExecutionManager().AllocateMemory( current_image_->data.size(), true, true, true ) );
	cassert( img, "Failed to allocate memory through native execution manager (size: %zu, readable, writable, executable)",
			 current_image_->data.size() );
	memcpy( img, current_image_->data, current_image_->data.size() );

	// Patch in the references
	for( const ReferencePatch& ref: current_image_->references ) {
		void* where = img + ref.where;
		char* target_insn = img + current_image_->insn_offsets.at( ref.what );
		switch( ref.type ) {
		case ReferencePatch::RT_ABSOLUTE:
			*reinterpret_cast<void**>( where ) = target_insn;
			break;
		case ReferencePatch::RT_TO_INSTRUCTIONPOINTER: {
			char* next_insn = img + current_image_->insn_offsets.at( ref.next_insn );
			ptrdiff_t offset_to_target_insn = target_insn - next_insn;
			*reinterpret_cast<int32_t*>( where ) = offset_to_target_insn;
			break;
		}
		}
	}

	current_image_->mm.image = img;
	current_image_->mm.length = current_image_->data.size();

	cassert( ImageIsOK( current_chk_ ), "Newly finalized image is not OK - inconsistency" );
}

void x86Backend::Clear()
{
	verify_method;

	Deallocate();
	current_image_->data.clear();
}

llarray& x86Backend::Target()
{
	verify_method;

	return current_image_->data;
}

ModRMWrapper x86Backend::CompileReferenceResolution( const DirectReference& dref )
{
	void* data_ptr = nullptr;

	switch( dref.section ) {
	case S_MAX:
	case S_NONE:
		casshole( "Invalid direct reference type while compiling reference access" );
		break;

	case S_CODE:
		return ModRMWrapper( IndirectNoShift::Displacement32 ).SetDisplacementToInsn( dref.address );

	case S_DATA:
		data_ptr = &proc_->MMU()->AData( dref.address ).integer;
		break;

	case S_REGISTER:
		data_ptr = &proc_->MMU()->ARegister( static_cast<Register>( dref.address ) ).integer;
		break;

	case S_BYTEPOOL:
		data_ptr = proc_->MMU()->ABytepool( dref.address );
		break;

	case S_FRAME:
	case S_FRAME_BACK: {
		bool frame_is_reverse = ( dref.section == S_FRAME_BACK );
		int32_t offset_to_native_frame = TranslateStackFrame( frame_is_reverse, dref.address );
		return ModRMWrapper( IndirectShiftDisplacement32::RBP ).SetDisplacement( offset_to_native_frame );
	}
	}

	// Load the data_ptr into some temporary register and return a modr/m referencing the register.
	Insn()
		.AddOpcode( 0xB8 )
		.AddOpcodeRegister( Reg64::RCX )
		.AddImmediate( data_ptr )
		.Emit( this );
	return ModRMWrapper( IndirectNoShift::RCX );
}

abiret_t x86Backend::RuntimeReferenceResolution( NativeImage* image, const DirectReference& dref )
{
	union {
		abiret_t retval;
		void* address;
		int32_t offset_to_native_frame;
	} result;

	switch( dref.section ) {
	case S_MAX:
	case S_NONE:
		casshole( "Invalid direct reference type while runtime-resolving reference access" );
		break;

	case S_CODE:
		result.address = reinterpret_cast<char*>( image->mm.image ) + image->insn_offsets.at( dref.address );
		break;

	case S_DATA:
		result.address = &proc_->MMU()->AData( dref.address ).integer;
		break;

	case S_REGISTER:
		result.address = &proc_->MMU()->ARegister( static_cast<Register>( dref.address ) ).integer;
		break;

	case S_BYTEPOOL:
		result.address = proc_->MMU()->ABytepool( dref.address );
		break;

	case S_FRAME:
	case S_FRAME_BACK: {
		bool frame_is_reverse = ( dref.section == S_FRAME_BACK );
		result.offset_to_native_frame = TranslateStackFrame( frame_is_reverse, dref.address );
	}
	}

	return result.retval;
}

ModRMWrapper x86Backend::CompileReferenceResolution( const Reference& ref )
{
	verify_method;

	// Do partial resolution of the given reference.
	bool ref_resolved_statically = true;
	DirectReference dref = proc_->Linker()->Resolve( ref, &ref_resolved_statically );

	if( ref_resolved_statically ) {
		// If the reference has been statically resolved, emit a static modr/m.
		return CompileReferenceResolution( dref );
	} else {
		// If not, emit a call to get the address into RCX and emit modr/m referencing RCX.
		// The only exception is S_FRAME/S_FRAME_BACK, where we need to emit modr/m+sib which uses native stack frame.
		CompileBinaryGateCall( BinaryFunction::BF_RESOLVEREFERENCE, reinterpret_cast<abiret_t>( &ref ) );

		switch( dref.section ) {
		case S_MAX:
		case S_NONE:
			casshole( "Invalid direct reference type while compiling reference access" );
			break;

		case S_CODE:
		case S_DATA:
		case S_REGISTER:
		case S_BYTEPOOL:
			return ModRMWrapper( IndirectNoShift::RCX );

		case S_FRAME:
		case S_FRAME_BACK: {
			return ModRMWrapper( BaseRegs::RCX, IndexRegs::RBP, 1, ModField::NoShift );
		}
		}
	}
}

void x86Backend::CompileBinaryGateCall( x86Backend::BinaryFunction function, abiret_t argument )
{
	/*
	 * 1. Save the registers: rax, r11
	 * 2. Pass the parameters: backend, image, function, argument
	 * 3. Call the function
	 * 4. Move the result into rcx
	 * 5. Restore the registers: r11, rax
	 */

	// push rax
	Insn()
		.AddOpcode( 0x50 )
		.AddOpcodeRegister( Reg64::RAX )
		.SetIsDefault64Bit()
		.Emit( this );

	// push r11
	Insn()
		.AddOpcode( 0x50 )
		.AddOpcodeRegister( Reg64E::R11 )
		.SetIsDefault64Bit()
		.Emit( this );

	// align the stack on 16-bit boundary
	// see http://stackoverflow.com/a/9600102/857932

	// push rsp
	Insn()
		.AddOpcode( 0x50 )
		.AddOpcodeRegister( Reg64::RSP )
		.SetIsDefault64Bit()
		.Emit( this );

	// push qword [rsp]
	Insn()
		.AddOpcode( 0xFF )
		.SetOpcodeExtension( 0x6 )
		.AddRM( ModRMWrapper( BaseRegs::RSP, IndexRegs::None, 1, ModField::NoShift ) )
		.SetIsDefault64Bit()
		.Emit( this );

	// and rsp, -16 (0xffff ffff ffff fff0)
	Insn()
		.AddOpcode( 0x83 )
		.SetOpcodeExtension( 0x4 )
		.AddRM( RegisterWrapper( Reg64::RSP ) )
		.AddImmediate<int8_t>( -0x10 )
		.Emit( this );

	// mov rdi, {backend}
	Insn()
		.AddOpcode( 0xB8 )
		.AddOpcodeRegister( Reg64::RDI )
		.AddImmediate( this )
		.Emit( this );

	// mov rsi, {image}
	Insn()
		.AddOpcode( 0xB8 )
		.AddOpcodeRegister( Reg64::RSI )
		.AddImmediate( current_image_ )
		.Emit( this );

	// mov rdx, {function}
	Insn()
		.AddOpcode( 0xB8 )
		.AddOpcodeRegister( Reg64::RDX )
		.AddImmediate( function )
		.Emit( this );

	// mov rcx, {argument}
	Insn()
		.AddOpcode( 0xB8 )
		.AddOpcodeRegister( Reg64::RCX )
		.AddImmediate( argument )
		.Emit( this );

	// mov rax, {address}
	Insn()
		.AddOpcode( 0xB8 )
		.AddOpcodeRegister( Reg64::RAX )
		.AddImmediate( &BinaryGateFunction )
		.Emit( this );

	// call rax
	Insn()
		.AddOpcode( 0xFF )
		.SetOpcodeExtension( 0x2 )
		.AddRM( ModRMWrapper( Reg64::RAX ) )
		.SetIsDefault64Bit()
		.Emit( this );

	// mov rcx, rax
	Insn()
		.AddOpcode( 0x8B )
		.AddRegister( Reg64::RCX )
		.AddRegister( Reg64::RAX )
		.Emit( this );

	// recover rsp after alignment
	// see http://stackoverflow.com/a/9600102/857932

	// mov rsp, qword [rsp]+8
	Insn()
		.AddOpcode( 0x8B )
		.AddRegister( Reg64::RSP )
		.AddRM( ModRMWrapper( BaseRegs::RSP, IndexRegs::None, 1, ModField::Disp8 ).SetDisplacement( int8_t( 8 ) ) )
		.Emit( this );

	// pop r11
	Insn()
		.AddOpcode( 0x58 )
		.AddOpcodeRegister( Reg64E::R11 )
		.SetIsDefault64Bit()
		.Emit( this );

	// pop rax
	Insn()
		.AddOpcode( 0x58 )
		.AddOpcodeRegister( Reg64::RAX )
		.SetIsDefault64Bit()
		.Emit( this );
}

void x86Backend::CompileControlTransferInstruction( const Reference& ref,
                                                    const Insn& jmp_rel32_opcode,
                                                    const Insn& jmp_modrm_opcode )
{
	Insn jump_insn;
	bool ref_resolved_statically = true;
	DirectReference dref = proc_->Linker()->Resolve( ref, &ref_resolved_statically );
	if( ref_resolved_statically ) {
		jump_insn = jmp_rel32_opcode;
		jump_insn.AddDisplacement( size_t( dref.address ) );
	} else {
		CompileBinaryGateCall( BinaryFunction::BF_RESOLVEREFERENCE, reinterpret_cast<abiret_t>( &ref ) );

		jump_insn = jmp_modrm_opcode;
		jump_insn.AddRM( ModRMWrapper( IndirectNoShift::RCX ) );
	}
	jump_insn.Emit( this );
}

void x86Backend::CompileBuffer( size_t chk )
{
	msg( E_INFO, E_DEBUG, "Compiling for checksum %zx", chk );
	Select( chk, true );
	Clear();

	IMMU* mmu = proc_->MMU();
	ILogic* logic = proc_->LogicProvider();

	Offsets limits = mmu->QuerySectionLimits();

	msg( E_INFO, E_DEBUG, "Emitting prologue" );
	CompilePrologue();

	// Reserve one entry for the final record (will be needed for the finalization/ref. resolve code)
	current_image_->insn_offsets.reserve( limits.Code() + 1 );

	for( size_t pc = 0; pc < limits.Code(); ++pc ) {
		RecordNextInsnOffset();
		Command& cmd = mmu->ACommand( pc );
		msg( E_INFO, E_DEBUG, "Emitting native code for command [PC=%zu OFFSET=%zu] \"%s\"",
			 pc, current_image_->insn_offsets.back(), logic->DumpCommand( cmd ).c_str() );
		CompileCommand( cmd );
	}
	RecordNextInsnOffset();

	msg( E_INFO, E_VERBOSE, "Native code emission completed" );
	Finalize();
}

abiret_t x86Backend::InternalBinaryGateFunction( NativeImage* image,
                                                 BinaryFunction function,
                                                 abiret_t argument )
{
	switch( function ) {
	case BinaryFunction::BF_RESOLVEREFERENCE: {
		// the argument is the reference
		Reference* ref = reinterpret_cast<Reference*>( argument );
		DirectReference dref = proc_->Linker()->Resolve( *ref );
		return RuntimeReferenceResolution( image, dref );
	}
	}
}

abiret_t x86Backend::BinaryGateFunction( x86Backend* backend,
										 x86Backend::NativeImage* image,
										 x86Backend::BinaryFunction function,
										 abiret_t argument )
{
	return backend->InternalBinaryGateFunction( image, function, argument );
}

void x86Backend::AddCodeReference( size_t insn, bool relative )
{
	verify_method;

	static const uint32_t disp32_stub = 0xbaadf00d;
	static const uint64_t disp64_stub = 0xdeadc0de00bf00bf;

	ReferencePatch ref;
	ref.what = insn;
	ref.next_insn = current_image_->insn_offsets.size();
	ref.where = Target().size();

	if( relative ) {
		ref.type = ReferencePatch::RT_TO_INSTRUCTIONPOINTER;
		Target().append( sizeof( disp32_stub ), &disp32_stub );
	} else {
		ref.type = ReferencePatch::RT_ABSOLUTE;
		Target().append( sizeof( disp64_stub ), &disp64_stub );
	}
	current_image_->references.push_back( ref );
}

int32_t x86Backend::TranslateStackFrame( bool is_reverse, size_t address )
{
	int32_t dest;
	if( is_reverse ) {
		dest = 1 + address;
	} else {
		dest = -address;
	}
	dest *= 8;
	return dest;
}

x86Backend::x86Backend()
{

}

x86Backend::~x86Backend()
{

}

} // namespace ProcessorImplementation
