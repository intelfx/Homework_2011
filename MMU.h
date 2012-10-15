#ifndef INTERPRETER_MMU_H
#define INTERPRETER_MMU_H

#include "build.h"

#include "Interfaces.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			MMU.h
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Default memory management unit plugin implementation.
// -------------------------------------------------------------------------------------

namespace ProcessorImplementation
{
using namespace Processor;

class INTERPRETER_API MMU : public IMMU
{
	struct InternalContextBuffer {
		std::vector<calc_t> data;
		std::vector<Command> commands;
		llarray bytepool;

		symbol_map sym_table;

		calc_t registers[R_MAX];
	};

	std::vector<calc_t> stacks_[Value::V_MAX];

	std::map<ctx_t, InternalContextBuffer> buffers_;
	std::map<ctx_t, InternalContextBuffer>::iterator current_buffer_;

	InternalContextBuffer& CurrentBuffer()
	{ cassert( current_buffer_ != buffers_.end(), "No context buffer is selected" ); return current_buffer_->second; }

	const InternalContextBuffer& CurrentBuffer() const
	{ cassert( current_buffer_ != buffers_.end(), "No context buffer is selected" ); return current_buffer_->second; }

	void InternalDumpCtx( const InternalContextBuffer* icb, std::string& registers, std::string& stacks ) const;

	void ClearStacks();

	void CheckFrameOperation( Value::Type frame_stack_type ) const
	{
		cassert( proc_->CurrentContext().frame <= stacks_[frame_stack_type].size(),
		         "Invalid stack frame at %zu (T: %zu)",
		         proc_->CurrentContext().frame, stacks_[frame_stack_type].size() );
	}

	void CheckStackAddress( Value::Type main_stack_type, size_t offset ) const
	{
		cassert( offset < stacks_[main_stack_type].size(),
		         "Invalid offset to stack top: %zu (T: %zu)",
		         offset, stacks_[main_stack_type].size() );
	}

	void CheckFrameAddress( Value::Type frame_stack_type, ssize_t offset ) const
	{
		CheckFrameOperation( frame_stack_type );

		ssize_t address = proc_->CurrentContext().frame + offset;
		cassert( address >= 0,
		         "Invalid parameter offset to frame: %zd (F: %zu)",
		         offset, proc_->CurrentContext().frame );
		cassert( static_cast<size_t>( address ) < stacks_[frame_stack_type].size(),
		         "Invalid local offset to frame: %zd (F: %zu T: %zu)",
		         offset, proc_->CurrentContext().frame, stacks_[frame_stack_type].size() );
	}

protected:
	virtual bool _Verify() const;
	virtual void OnAttach();
	virtual void OnDetach();

public:
	MMU();

	virtual void			DumpContext( std::string* regs, std::string* stacks ) const;

	virtual ctx_t			CurrentContextBuffer() const;
	virtual ctx_t			AllocateContextBuffer();
	virtual void			SelectContextBuffer( ctx_t id );
	virtual void			ReleaseContextBuffer( ctx_t id );
	virtual void			ResetContextBuffer( ctx_t id );

	virtual size_t			QueryStackTop( Value::Type type ) const;
	virtual void            SetStackTop( Value::Type type, ssize_t adjust );

	virtual calc_t&			AStackFrame( Value::Type type, ssize_t offset );
	virtual calc_t&			AStackTop( Value::Type type, size_t offset );
	virtual calc_t&			ARegister( Register reg_id );
	virtual Command&		ACommand( size_t ip );
	virtual calc_t&			AData( size_t addr );
	virtual symbol_type&	ASymbol( size_t hash );
	virtual char*			ABytepool( size_t offset );

	virtual Offsets			QuerySectionLimits() const;

	virtual llarray			DumpSection( MemorySectionIdentifier section, size_t address,
	                                     size_t count );
	virtual void			ModifySection( MemorySectionIdentifier section, size_t address,
	                                       const void* data, size_t count, bool insert = false );
	virtual void			AppendSection( MemorySectionIdentifier section,
	                                       const void* data, size_t count );

	virtual void			SetSymbolImage( symbol_map && symbols );
	virtual symbol_map		DumpSymbolImage() const;

	virtual void			VerifyReference( const DirectReference& ref,
	                                         Value::Type frame_stack_type ) const;

	virtual void			ShiftImages( const Offsets& offsets ); // Shift forth all sections by specified offset, filling space with empty data.
	virtual void			PasteFromContext( ctx_t id ); // Paste the specified context over the current one

	virtual void			ResetEverything();
};

} // namespace ProcessorImplementation

#endif // INTERPRETER_MMU_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
