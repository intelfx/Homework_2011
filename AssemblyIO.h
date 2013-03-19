#ifndef INTERPRETER_ASSEMBLYIO_H
#define INTERPRETER_ASSEMBLYIO_H

#include "build.h"

#include "Interfaces.h"
#include "Linker.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			AssemblyIO.h
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Assembly language plugin implementation.
// -------------------------------------------------------------------------------------

DeclareDescriptor( AsmHandler, INTERPRETER_API, INTERPRETER_TE )

namespace ProcessorImplementation
{
using namespace Processor;

class INTERPRETER_API AsmHandler : LogBase( AsmHandler ), public IReader, public IWriter
{
	FILE* reading_file_;
	FILE* writing_file_;

	unsigned current_line_num;
	Value::Type last_statement_type;
	DecodeResult decode_output;

	void ReadSingleStatement( char* input );
	void ReadSingleDeclaration( const char* decl_data );
	void ReadSingleCommand( const char* command, char* argument );

	void InternalWriteFile();
	char* PrepLine( char* read_buffer );
	void PrepReference( char* reference );
	char* ParseLabel( char* current_position );

	AddrType DecodeSectionType( char id );

	void                   ParseInsertString( char* arg );
	Reference              ParseFullReference( char* arg );
	Reference::BaseRef     ParseBaseReference( char* arg );
	Reference::SingleRef   ParseIndirectReference( char* arg );
	Reference::SingleRef   ParseSingleReference( char* arg, Processor::AddrType* global_section );
	Reference::BaseRef     ParseRegisterReference( char* arg );

protected:
	virtual bool _Verify() const;

public:
	AsmHandler();
	virtual ~AsmHandler();

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

#endif // INTERPRETER_ASSEMBLYIO_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
