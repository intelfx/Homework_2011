#ifndef INTERPRETER_X86BACKEND_H
#define INTERPRETER_X86BACKEND_H

#include "build.h"

#include "Interfaces.h"
#include "x86backend/Interfaces.h"
#include <uXray/fxcontainers.h>

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		x86Backend.h
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	x86 JIT compiler backend.
// -------------------------------------------------------------------------------------

namespace ProcessorImplementation
{
using namespace Processor;

class INTERPRETER_API x86Backend : public IBackend, public x86backend::IEmissionDestination
{
	struct ReferencePatch
	{
		enum ReferenceType
		{
			RT_ABSOLUTE,
			RT_TO_INSTRUCTIONPOINTER // to RIP of the next insn executed, 32-bit signed offset
		} type;
		off_t where; // offset in the native buffer where to write the reference
		size_t where_insn; // virtual insn to which the reference shall belong to
		size_t what; // virtual insn being referenced
	};

	struct NativeImage
	{
		llarray data; // primary working buffer
		std::vector<off_t> insn_offsets; // offsets of the native instructions (per-cmd) in buffer
		std::vector<ReferencePatch> references;
		struct // mapped executable region
		{
			void* image;
			size_t length;
		} mm;
		abi_callback_fn_t callback; // processor API callback function
	};
	std::map<size_t, NativeImage> images_;
	size_t current_chk_;
	NativeImage* current_image_;

	void Select( size_t chk, bool create = false );
	void Finalize();
	void Deallocate();
	void Clear();

	virtual llarray& Target();
	virtual void AddCodeReference( size_t insn, bool relative );

	void RecordNextInsnOffset()
	{
		current_image_->insn_offsets.push_back( current_image_->data.size() );
	}

	void CompileCommand( Command& cmd );
	void CompilePrologue();

protected:
	virtual bool _Verify() const;

public:
	x86Backend();
	virtual ~x86Backend();
	virtual void CompileBuffer( size_t chk, abi_callback_fn_t callback );
	virtual abi_native_fn_t GetImage( size_t chk );
	virtual bool ImageIsOK( size_t chk );
};

} // namespace ProcessorImplementation

#endif // INTERPRETER_X86BACKEND_H
