#ifndef _ASM_IO_H
#define _ASM_IO_H

#include "build.h"
#include "Interfaces.h"
#include "Linker.h"

DeclareDescriptor (AsmHandler);

namespace ProcessorImplementation
{
	using namespace Processor;

	class AsmHandler : LogBase (AsmHandler), public Processor::IReader, public Processor::IWriter
	{
		FILE* writing_file_;

		calc_t			ParseValue (const char* input);
		Reference		ParseReference (Processor::symbol_map& symbols, const char* arg);

		void	ReadSingleStatement (size_t line_num, char* input, Processor::DecodeResult& output);
		bool			ReadSingleDeclaration (const char* input, Processor::DecodeResult& output);
		bool			ReadSingleCommand (size_t line_num, const char* input, Processor::DecodeResult& output);

		void InternalWriteFile();
		char* PrepLine (char* read_buffer);
		char* ParseLabel (char* current_position);

	protected:
		virtual bool _Verify() const;

	public:
		AsmHandler();

		virtual void RdReset (FileProperties* prop);
		virtual FileProperties RdSetup (FILE* file);

		virtual size_t NextSection (Processor::FileProperties* prop, Processor::FileSectionType* type, size_t*, size_t*);

		virtual size_t ReadNextElement (FileProperties* prop, void* destination, size_t max);
		virtual size_t ReadSectionImage (FileProperties* prop, void* destination, size_t max);
		virtual size_t ReadStream (FileProperties* prop, DecodeResult* destination);

		virtual void WrSetup (FILE* file);
		virtual void WrReset();

		virtual void Write (size_t ctx_id);
	};
}

#endif // _ASM_IO_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
