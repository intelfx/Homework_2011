#include "stdafx.h"
#include "BytecodeIO.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			BytecodeIO.cpp
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Bytecode plugin implementation.
// -------------------------------------------------------------------------------------

ImplementDescriptor( BytecodeHandler, "bytecode reader/writer", MOD_APPMODULE );

namespace {
	const uint32_t file_signature = *reinterpret_cast<const uint32_t*>( "BCDE" );
	const uint32_t section_signature = *reinterpret_cast<const uint32_t*>( "SEC_" );
}

namespace ProcessorImplementation
{
using namespace Processor;

BytecodeHandler::BytecodeHandler() :
	reading_file_( nullptr ),
	writing_file_( nullptr ),
	count_sections_read_( 0 )
{
	mem_init( current_file_ );
	mem_init( current_section_ );
}

BytecodeHandler::~BytecodeHandler()
{
	if( reading_file_ )
		RdReset();

	if( writing_file_ )
		WrReset();
}

bool BytecodeHandler::_Verify() const
{
	if( writing_file_ )
		verify_statement( !ferror( writing_file_ ), "Error in writing stream" );

	if( reading_file_ )
		verify_statement( !ferror( reading_file_ ), "Error in reading stream" );

	return 1;
}

void BytecodeHandler::ReadFileInfo()
{
	fread( &current_file_, sizeof( current_file_ ), 1, reading_file_ );
	cassert( current_file_.signature == file_signature, "Invalid file signature: %08x", current_file_.signature );
	msg( E_INFO, E_DEBUG, "%hhu sections in file", current_file_.section_count );
	count_sections_read_ = 0;
}

void BytecodeHandler::ReadSectionInfo()
{
	fread( &current_section_, sizeof( current_section_ ), 1, reading_file_ );
	cassert( current_section_.signature == section_signature,
	         "Invalid section signature: %08x",
	         current_section_.signature );
	cassert( current_section_.section_type < SEC_MAX,
	         "Invalid section type #: %u",
	         current_section_.section_type );
	msg( E_INFO, E_DEBUG, "New section: %u bytes, %u entries",
		 current_section_.size_bytes, current_section_.size_entries );
	++count_sections_read_;
}


FileType BytecodeHandler::RdSetup( FILE* file )
{
	reading_file_ = file;
	verify_method;

	rewind( reading_file_ );
	ReadFileInfo();

	return FT_BINARY;
}

void BytecodeHandler::RdReset()
{
	verify_method;

	if( reading_file_ ) {
		msg( E_INFO, E_DEBUG, "Resetting reader" );

		fclose( reading_file_ );
		reading_file_ = nullptr;
		count_sections_read_ = 0;

		mem_init( current_file_ );
		mem_init( current_section_ );
	}
}

void BytecodeHandler::WrReset()
{
	verify_method;

	if( writing_file_ ) {
		msg( E_INFO, E_DEBUG, "Resetting writing file" );

		fclose( writing_file_ );
		writing_file_ = nullptr;
	}
}

void BytecodeHandler::WrSetup( FILE* file )
{
	writing_file_ = file;
	verify_method;

	msg( E_INFO, E_DEBUG, "Writer set up" );
}

void BytecodeHandler::Write( ctx_t id )
{
	verify_method;
	cassert( writing_file_, "Writer has not been set up" );

	msg( E_INFO, E_VERBOSE, "Beginning ASM write of context %zu", id );

	proc_->LogicProvider()->SwitchToContextBuffer( id );
	InternalWriteFile();
	proc_->LogicProvider()->RestoreCurrentContext();
}

void BytecodeHandler::InternalWriteFile()
{
	// We write code, data, bytepool and symbols
	static const size_t writing_section_count = 4;
	static const MemorySectionType writing_sections[writing_section_count] =
	{
		SEC_CODE_IMAGE,
		SEC_DATA_IMAGE,
		SEC_BYTEPOOL_IMAGE,
		SEC_SYMBOL_MAP
	};

	FileHeader hdr;
	hdr.signature = file_signature;
	hdr.section_count = writing_section_count;
	fwrite( &hdr, sizeof( hdr ), 1, writing_file_ );

	Offsets limits = proc_->MMU()->QuerySectionLimits();

	for( size_t i = 0; i < writing_section_count; ++i ) {
		if( writing_sections[i] == SEC_SYMBOL_MAP ) {
			WriteSymbols( proc_->MMU()->DumpSymbolImage() );
		} else {
			MemorySectionIdentifier id( writing_sections[i] );
			llarray section_data = proc_->MMU()->DumpSection( id, 0, limits.at( id ) );
			PutSection( id.SectionType(), section_data, limits.at( id ) );
		}
	}
}

std::pair< MemorySectionIdentifier, size_t > BytecodeHandler::NextSection()
{
	verify_method;

	if( count_sections_read_ >= current_file_.section_count ) {
		return std::make_pair( MemorySectionIdentifier(), size_t( 0 ) );
	} else {
		ReadSectionInfo();
		return std::make_pair( MemorySectionIdentifier( current_section_.section_type ),
							   size_t( current_section_.size_entries ) );
	}
}

llarray BytecodeHandler::ReadSectionImage()
{
	llarray ret;
	ret.resize( current_section_.size_bytes );
	fread( ret, 1, current_section_.size_bytes, reading_file_ );
	return ret;
}

void BytecodeHandler::PutSection( MemorySectionType type, const llarray& data, size_t entities_count )
{
	SectionHeader hdr;
	hdr.signature = section_signature;
	hdr.section_type = type;
	hdr.size_bytes = data.size();
	hdr.size_entries = entities_count;
	fwrite( &hdr, sizeof( hdr ), 1, writing_file_ );
	fwrite( data, 1, data.size(), writing_file_ );
}

void BytecodeHandler::WriteSymbols( const symbol_map& symbols )
{
	verify_method;

	llarray temp;
	for( const symbol_map::value_type& symbol_record: symbols ) {
		// Write name
		const std::string& name = symbol_record.second.first;
		const Symbol& symbol = symbol_record.second.second;
		temp.append( name.size(), name.c_str() );
		temp.append( 1, "\0" );

		char is_resolved = 0;
		if( symbol.is_resolved ) {
			is_resolved = 1;
		}

		temp.append( 1, &is_resolved );
		if( is_resolved ) {
			temp.append( sizeof( symbol.ref ), &symbol.ref );
		}
	}

	PutSection( Processor::SEC_SYMBOL_MAP, temp, symbols.size() );
}

symbol_map BytecodeHandler::ReadSymbols()
{
	verify_method;

	llarray temp;
	symbol_map ret;

	temp.resize( current_section_.size_bytes );
	fread( temp, 1, current_section_.size_bytes, reading_file_ );

	char* ptr = temp;
	for( size_t i = 0; i < current_section_.size_entries; ++i ) {
		char* name = ptr;
		size_t len = strlen( name );
		ptr += len + 1;

		char is_resolved = *ptr++;
		if( is_resolved ) {
			Reference* ref = reinterpret_cast<Reference*>( ptr );
			ptr += sizeof( Reference );

			Symbol sym( name, *ref );
			InsertSymbol( sym, name, ret );
		} else {
			Symbol sym( name );
			InsertSymbol( sym, name, ret );
		}
	}

	cassert( ptr - current_section_.size_bytes == temp,
			 "Failed to deserialize symbols - did not read the whole section" );
	return ret;
}

DecodeResult* BytecodeHandler::ReadStream()
{
	casshole( "Not implemented" );
}

} // namespace ProcessorImplementation
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
