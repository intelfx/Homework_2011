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

namespace x86backend
{
struct ModRMWrapper;
class Insn;
}

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
		size_t next_insn; // next virtual insn wrt. the one to which the reference shall belong
		size_t what; // virtual insn being referenced
	};

	enum class BinaryFunction : abiret_t
	{
		BF_RESOLVEREFERENCE
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

	// A big and dirty HACK
	bool IsCmd( Command& cmd, const char* mnemonic )
	{
		const CommandTraits* traits = proc_->CommandSet()->DecodeCommand( mnemonic );
		return traits && ( traits->id == cmd.id );
	}

	void RecordNextInsnOffset()
	{
		current_image_->insn_offsets.push_back( current_image_->data.size() );
	}

	void CompileCommand( Command& cmd );
	bool CompileCommand_Control( Command& cmd );
	bool CompileCommand_Arithmetic( Command& cmd );
	bool CompileCommand_Conditionals( Command& cmd );
	bool CompileCommand_System( Command& cmd );
	void CompilePrologue();
	void CompileStoreFlags();
	void CompileRestoreFlags();
	void CompileIntegerCompare();
	void CompileFPCompare();
	void CompileBinaryGateCall( BinaryFunction function, abiret_t argument );

	// Compiles a stub for a jump/call instruction.
	// Actually, this is a version of CompileReferenceResolution() but for jumps
	// as they need directly specified disp32 instead of using modr/m with mod == 00b and r/m == 101b.
	void CompileControlTransferInstruction( const Reference& ref,
	                                        const x86backend::Insn& jmp_rel32_opcode,
	                                        const x86backend::Insn& jmp_modrm_opcode );

	// Compiles a conditional control transfer instruction sequence.
	// The problem is that CompileControlTransferInstruction() may resolve a jump indirectly
	// and x86 Jcc does not have an indirect form.
	// Thus, it will be needed to emit an inverse trampoline jump in such a situation:
	//
	// jncc _next
	// jmp [dest]
	// _next:
	//
	// instead of
	//
	// jcc [dest] -- which does not exist.
	//
	void CompileConditionalControlTransferInstruction( const Reference& ref,
	                                                   const x86backend::Insn& jcc_rel32_opcode,
	                                                   const x86backend::Insn& jncc_rel8_opcode,
	                                                   const x86backend::Insn& jmp_modrm_opcode );

	// Compiles code needed to resolve a reference
	// and returns a modr/m byte which points to the required data.
	x86backend::ModRMWrapper CompileReferenceResolution( const Reference& ref );
	x86backend::ModRMWrapper CompileReferenceResolution( const DirectReference& dref );

	// Resolves a reference at runtime and returns its address.
	// For S_FRAME or S_FRAME_BACK, returns offset to RBP.
	abiret_t RuntimeReferenceResolution( NativeImage* image, const DirectReference& dref );

	// Translates virtual stack frame access into a native offset to RBP.
	// is_reverse: whether the reference is S_FRAME_BACK.
	int32_t TranslateStackFrame( bool is_reverse, size_t address );

	abiret_t InternalBinaryGateFunction( NativeImage* image,
	                                     BinaryFunction function,
	                                     abiret_t argument );

	static abiret_t BinaryGateFunction( x86Backend* backend,
	                                    NativeImage* image,
	                                    BinaryFunction function,
	                                    abiret_t argument );

protected:
	virtual bool _Verify() const;

public:
	x86Backend();
	virtual ~x86Backend();
	virtual void CompileBuffer( size_t chk );
	virtual abi_native_fn_t GetImage( size_t chk );
	virtual bool ImageIsOK( size_t chk );
};

} // namespace ProcessorImplementation

#endif // INTERPRETER_X86BACKEND_H
