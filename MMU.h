#ifndef _MMU_H
#define _MMU_H

// -----------------------------------------------------------------------------
// Library		Homework
// File			MMU.h
// Author		intelfx
// Description	Memory management unit.
// -----------------------------------------------------------------------------

#include "build.h"
#include "Interfaces.h"

namespace ProcessorImplementation
{
	using namespace Processor;
	class INTERPRETER_API MMU : public Processor::IMMU
	{
		struct InternalContextBuffer
		{
			std::vector<calc_t> data;
			std::vector<Command> commands;
			char* bytepool_data;
			size_t bytepool_length;

			symbol_map sym_table;

			calc_t registers[R_MAX];
		};

		std::vector<calc_t> main_stack, integer_stack, fp_stack, *current_stack, *frame_stack;
		Value::Type current_stack_type, frame_stack_type;

		std::vector<Context> context_stack;
		StaticAllocator<InternalContextBuffer, BUFFER_NUM> buffers;
		Context context;

		InternalContextBuffer& CurrentBuffer();
		const InternalContextBuffer& CurrentBuffer() const;

		void InternalDumpCtx (const Context& w_context) const;
		void ClearStacks();
		size_t InternalWriteStack (const std::vector<calc_t>* stack, Processor::calc_t* pointer) const;
		void InternalWrStackPointer (std::vector<calc_t>** ptr, Value::Type type);

	protected:
		virtual bool _Verify() const;
		virtual void OnAttach();

	public:
		MMU();
		virtual ~MMU();

		virtual Context&		GetContext	();
		virtual void			DumpContext	() const;

		virtual void			SelectStack	(Value::Type type);

		virtual calc_t&			AStackFrame	(int offset);
		virtual calc_t&			AStackTop	(size_t offset);
		virtual calc_t&			ARegister	(Register reg_id);
		virtual Command&		ACommand	(size_t ip);
		virtual calc_t&			AData		(size_t addr);
		virtual symbol_type&	ASymbol		(size_t hash);
		virtual char*			ABytepool	(size_t offset);

		virtual void			ReadStack	(calc_t* image, size_t size, bool selected_only);
		virtual void			ReadData	(calc_t* image, size_t size);
		virtual void			ReadText	(Command* image, size_t size);
		virtual void			ReadSyms	(void* image, size_t size);

		virtual void			InsertData	(calc_t value);
		virtual void			InsertText	(const Command& command);
		virtual void			InsertSyms	(symbol_map&& syms);

		virtual void			WriteStack	(Processor::calc_t* image, bool selected_only) const;
		virtual void			WriteData	(calc_t* image) const;
		virtual void 			WriteText	(Command* image) const;
		virtual void			WriteSyms	(void** image, size_t* bytes, size_t* count) const;

		virtual void			AllocBytes	(size_t base, size_t length);
		virtual void			SetBytes	(size_t base, size_t length, const char* data);

		virtual size_t			GetTextSize	() const;
		virtual size_t			GetDataSize	() const;
		virtual size_t			GetPoolSize () const;
		virtual size_t			GetStackTop	() const;

		virtual void			AlterStackTop (short int offset);

		virtual void VerifyReference (const DirectReference& ref) const;

		virtual void ResetBuffers (size_t ctx_id);
		virtual void ResetEverything();

		virtual void SaveContext();
		virtual void ClearContext();
		virtual void RestoreContext();

		virtual void NextContextBuffer();
		virtual void AllocContextBuffer();
	};
}

#endif // _MMU_H
// kate: indent-mode cstyle; replace-tabs off; indent-width 4; tab-width 4;
