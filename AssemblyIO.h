#ifndef _ASM_IO_H
#define _ASM_IO_H

#include "build.h"
#include "Interfaces.h"
#include "Linker.h"

DeclareDescriptor (AsmHandler);

namespace ProcessorImplementation
{
using namespace Processor;

class INTERPRETER_API AsmHandler : virtual LogBase (AsmHandler), public Processor::IReader, public Processor::IWriter
{
	FILE* reading_file_;
	FILE* writing_file_;

	unsigned current_line_num;
	Value::Type last_statement_type;
	DecodeResult decode_output;

	void			ReadSingleStatement (char* input);
	void			ReadSingleDeclaration (const char* decl_data);
	void			ReadSingleCommand (const char* command, char* argument);

	void InternalWriteFile();
	char* PrepLine (char* read_buffer);
	char* ParseLabel (char* current_position);

	AddrType DecodeSectionType (char id);

	void					ParseInsertString		(char* arg);

	Reference				ParseFullReference		(char* arg);
	Reference::BaseRef		ParseBaseReference		(char* arg);
	Reference::IndirectRef	ParseIndirectReference	(char* arg);
	Reference::SingleRef	ParseSingleReference	(char* arg);
	Reference::SingleRef	ParseRegisterReference	(char* arg);

protected:
	virtual bool _Verify() const;

public:
	AsmHandler();
	virtual ~AsmHandler();

	virtual void RdReset();
	virtual FileType RdSetup (FILE* file);

	virtual size_t NextSection (Processor::MemorySectionType*, size_t*, size_t*);

	virtual void ReadSectionImage (void* destination);
	virtual DecodeResult* ReadStream();
	virtual void ReadSymbols (Processor::symbol_map&);

	virtual void WrSetup (FILE* file);
	virtual void WrReset();

	virtual void Write (size_t ctx_id);
};
}

#endif // _ASM_IO_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
