#ifndef _ASM_IO_H
#define _ASM_IO_H

#include "build.h"
#include "Interfaces.h"
#include "Linker.h"

DeclareDescriptor(AsmHandler);

namespace ProcessorImplementation
{
	using namespace Processor;

	class AsmHandler : LogBase(AsmHandler), public Processor::IReader, public Processor::IWriter
	{
        FILE* writing_file_;
        char read_buffer[line_length], command[line_length], arg[line_length];

        std::pair<bool, DecodeResult>	ReadSingleStatement (size_t line_num);
		Reference						ReadSingleReference (symbol_map& symbols);

        void InternalWriteFile();
		char* PrepLine();

    protected:
        virtual bool _Verify() const;

    public:
		AsmHandler();

        virtual void RdReset (FileProperties* prop);
        virtual FileProperties RdSetup (FILE* file);

        virtual size_t NextSection (Processor::FileProperties* prop, Processor::FileSectionType* type, size_t* count, size_t*, bool = 0);

        virtual size_t ReadNextElement (FileProperties* prop, void* destination, size_t max);
        virtual size_t ReadSectionImage (FileProperties* prop, void* destination, size_t max);
        virtual size_t ReadStream (FileProperties* prop, DecodeResult* destination);

		virtual void WrSetup (FILE* file);
		virtual void WrReset();

        virtual void Write (size_t ctx_id);
	};
}

#endif // _ASM_IO_H