#include "stdafx.h"
#include "x86Backend.h"

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		x86Backend.h
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	x86 JIT compiler backend.
// -------------------------------------------------------------------------------------

namespace ProcessorImplementation
{
using namespace Processor;

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
	verify_statement( current_image_, "No image is selected" );
	auto it = images_.find( current_chk_ );
	verify_statement( it != images_.end(), "Image not found for the current checksum %zx", current_chk_ );
	verify_statement( &it->second == current_image_, "Looked up image does not match selected (chk %zx)", current_chk_ );
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

	void* img = proc_->ExecutionManager().AllocateMemory( current_image_->data.size(), true, true, true );
	cassert( img, "Failed to allocate memory through native execution manager (size: %zu, readable, writable, executable)",
			 current_image_->data.size() );
	memcpy( img, current_image_->data, current_image_->data.size() );
	current_image_->mm.image = img;

	cassert( ImageIsOK( current_chk_ ), "Newly finalized image is not OK - inconsistency" );
}

void x86Backend::Clear()
{
	verify_method;

	Deallocate();
	current_image_->data.clear();
}

void x86Backend::CompileBuffer( size_t chk, abi_callback_fn_t callback )
{
	msg( E_INFO, E_DEBUG, "Compiling for checksum %zx", chk );
	Select( chk, true );
	Clear();
	current_image_->callback = callback;

	IMMU* mmu = proc_->MMU();
	ILogic* logic = proc_->LogicProvider();

	size_t section_limits[SEC_MAX];
	mmu->QueryLimits( section_limits );

	msg( E_INFO, E_DEBUG, "Emitting prologue" );
	CompilePrologue();

	for( size_t pc = 0; pc < section_limits[SEC_CODE_IMAGE]; ++pc ) {
		Command& cmd = mmu->ACommand( pc );
		msg( E_INFO, E_DEBUG, "Emitting native code for command [PC=%zu] \"%s\"",
			 pc, logic->DumpCommand( cmd ) );
		CompileCommand( cmd );
	}

	msg( E_INFO, E_VERBOSE, "Native code emission completed" );
	Finalize();
}

x86Backend::x86Backend()
{

}

x86Backend::~x86Backend()
{

}

} // namespace ProcessorImplementation
