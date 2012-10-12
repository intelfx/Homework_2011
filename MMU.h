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

class INTERPRETER_API MMU : public Processor::IMMU
{
	struct InternalContextBuffer {
		std::vector<calc_t> data;
		std::vector<Command> commands;
		char* bytepool_data;
		size_t bytepool_length;

		symbol_map sym_table;

		calc_t registers[R_MAX];
	};

	std::vector<calc_t> integer_stack, fp_stack, *current_stack, *frame_stack;
	Value::Type current_stack_type, frame_stack_type;

	std::vector<Context> context_stack;
	std::vector<InternalContextBuffer> buffers;
	Context context;

	mutable char dump_data_registers[STATIC_LENGTH];
	mutable char dump_data_context[STATIC_LENGTH];
	mutable char dump_data_stacks[STATIC_LENGTH];

	InternalContextBuffer& CurrentBuffer();
	const InternalContextBuffer& CurrentBuffer() const;

	void InternalDumpCtx( const Context& w_context ) const;
	void LogDumpCtx( const Context& w_context ) const;

	void ClearStacks();
	void InternalWrStackPointer( std::vector<calc_t>** ptr, Value::Type type );

	void CheckStackOperation() const {
		cassert( current_stack, "Stack operations disabled or stack not selected" );
	}

	void CheckFrameOperation() const {
		cassert( frame_stack, "Frame operations disabled or frame stack not selected" );
		cassert( context.frame <= frame_stack->size(), "Invalid stack frame at %zu (T: %zu)",
		         context.frame, frame_stack->size() );
	}

	void CheckStackAddress( size_t offset ) const {
		CheckStackOperation();
		cassert( offset < current_stack->size(), "Invalid offset to stack top: %zu (T: %zu)",
		         offset, current_stack->size() );
	}

	void CheckFrameAddress( long offset ) const {
		CheckFrameOperation();
		cassert( -offset <= static_cast<long>( context.frame ), "Invalid offset to frame: %ld (F: %zu)",
		         offset, context.frame );
		cassert( context.frame + offset < frame_stack->size(), "Invalid offset to frame: %ld (F: %zu T: %zu)",
		         offset, context.frame, frame_stack->size() );
	}

	void AllocBytes( size_t destination_size );

protected:
	virtual bool _Verify() const;
	virtual void OnAttach();
	virtual void OnDetach();

public:
	MMU();

	virtual Context&		GetContext();
	virtual void			DumpContext( const char** ctx, const char** regs, const char** stacks ) const;

	virtual void			SelectStack( Value::Type type );
	virtual void			QueryActiveStack( Value::Type* type, size_t* top );
	virtual void			AlterStackTop( short int offset );

	virtual calc_t&			AStackFrame( int offset );
	virtual calc_t&			AStackTop( size_t offset );
	virtual calc_t&			ARegister( Register reg_id );
	virtual Command&		ACommand( size_t ip );
	virtual calc_t&			AData( size_t addr );
	virtual symbol_type&	ASymbol( size_t hash );
	virtual char*			ABytepool( size_t offset );

	virtual void			QueryLimits( size_t limits[SEC_MAX] ) const; // Query current section limits to the array
	virtual void			ReadSection( MemorySectionType section, void* image, size_t count ); // Read section image (maybe partial)
	virtual void			WriteSection( MemorySectionType section, void* image ) const; // Write section to image
	virtual void			ReadSymbolImage( symbol_map && symbols ); // Read symbol map image
	virtual void			WriteSymbolImage( symbol_map& symbols ) const; // Write symbol map to image

	virtual void			PasteFromContext( size_t ctx_id ); // Paste the specified context over the current one

	virtual void VerifyReference( const DirectReference& ref ) const;

	virtual void ResetBuffers( size_t ctx_id );
	virtual void ResetEverything();

	virtual void SaveContext();
	virtual void ClearContext();
	virtual void RestoreContext();

	virtual void NextContextBuffer();
	virtual void AllocContextBuffer();
};

} // namespace ProcessorImplementation

#endif // INTERPRETER_MMU_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
