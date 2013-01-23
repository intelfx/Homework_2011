#ifndef INTERPRETER_BYTECODEIO_H
#define INTERPRETER_BYTECODEIO_H

#include "build.h"

#include "Interfaces.h"
#include "Linker.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			BytecodeIO.h
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Bytecode plugin implementation.
// -------------------------------------------------------------------------------------

DeclareDescriptor( BytecodeHandler, INTERPRETER_API, INTERPRETER_TE )

namespace ProcessorImplementation
{
using namespace Processor;

class INTERPRETER_API BytecodeHandler : LogBase( BytecodeHandler ), public IReader, public IWriter
{
	struct FileHeader
	{
		uint32_t signature;
		uint8_t section_count;
	} PACKED;

	struct SectionHeader
	{
		uint32_t signature;
		uint32_t size_bytes;
		uint32_t size_entries;
		Processor::MemorySectionType section_type : 8;
	} PACKED;

	FILE* reading_file_;
	FILE* writing_file_;

	FileHeader current_file_;
	SectionHeader current_section_;
	uint8_t count_sections_read_;

	void ReadFileInfo();
	void ReadSectionInfo();

	void InternalWriteFile();

	void WriteSymbols( const symbol_map& symbols );

	void PutSection( Processor::MemorySectionType type, const llarray& data, size_t entities_count );

protected:
	virtual bool _Verify() const;

public:
	BytecodeHandler();
	virtual ~BytecodeHandler();

	virtual void RdReset();
	virtual FileType RdSetup( FILE* file );

	virtual std::pair<MemorySectionIdentifier, size_t> NextSection();

	virtual llarray ReadSectionImage();
	virtual DecodeResult* ReadStream();
	virtual symbol_map ReadSymbols();

	virtual void WrSetup( FILE* file );
	virtual void WrReset();

	virtual void Write( ctx_t id );
};

} // namespace ProcessorImplementation

#endif // INTERPRETER_BYTECODEIO_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
