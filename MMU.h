#ifndef _MMU_H
#define _MMU_H

// -----------------------------------------------------------------------------
// Library		Antided
// File			MMU.h
// Author		intelfx
// Description	Memory management unit.
// -----------------------------------------------------------------------------

#include "build.h"
#include "Interfaces.h"

namespace ProcessorImplementation
{
	using namespace Processor;
	class MMU : public Processor::IMMU
	{

		struct InternalContextBuffer
		{
			std::vector<calc_t> data;
			std::vector<DecodedCommand> commands;
			symbol_map sym_table;

			calc_t registers[R_MAX];
		};

		std::vector<calc_t> main_stack;
		std::vector<Context> context_stack;
		StaticAllocator<InternalContextBuffer, BUFFER_NUM> buffers;
		Context context;

		InternalContextBuffer& CurrentBuffer();

		void DumpContext (Context& w_context);

	protected:
		virtual bool Verify_() const;

	public:
		virtual calc_t&			AStackFrame	(int offset);
		virtual calc_t&			AStackTop	(size_t offset);
		virtual calc_t&			ARegister	(Register reg_id);
		virtual DecodedCommand&	ACommand	(size_t ip);
		virtual calc_t&			AData		(size_t addr);
		virtual symbol_type&	ASymbol		(size_t hash);

		virtual void			ReadStack	(calc_t* image, size_t size);
		virtual void			ReadData	(calc_t* image, size_t size);
		virtual void			ReadText	(DecodedCommand* image, size_t size);
		virtual void			ReadSyms	(symbol_map* image);

		virtual size_t			GetTextSize	();
		virtual size_t			GetDataSize	();
		virtual size_t			GetStackTop	();

		virtual Context&		GetContext	();

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