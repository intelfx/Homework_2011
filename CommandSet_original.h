#ifndef INTERPRETER_COMMANDSET_ORIGINAL_H
#define INTERPRETER_COMMANDSET_ORIGINAL_H

#include "build.h"

#include "Interfaces.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			CommandSet_original.h
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Original (Processor mkI) command set plugin implementation.
// -------------------------------------------------------------------------------------

namespace ProcessorImplementation
{
using namespace Processor;

class INTERPRETER_API CommandSet_mkI : public ICommandSet
{
	std::map<cid_t, CommandTraits> by_id;

	struct InternalCommandDescriptor {
		const char* name;
		const char* description;
		ArgumentType arg_type;
		bool is_service_command;
	};

	static const InternalCommandDescriptor initial_commands[];

	inline static cid_t get_id( const char* mnemonic ) {
		return crc32_runtime( mnemonic );
	}

protected:
	virtual void OnAttach();

public:
	virtual void ResetCommandSet();

	virtual void AddCommand( CommandTraits && command );
	virtual void AddCommandImplementation( const char* mnemonic, size_t module, void* handle );

	virtual const CommandTraits* DecodeCommand( const char* mnemonic ) const;
	virtual const CommandTraits* DecodeCommand( cid_t id ) const;

	virtual void* GetExecutionHandle( const CommandTraits& cmd, size_t module );
};

} // namespace ProcessorImplementation

#endif // INTERPRETER_COMMANDSET_ORIGINAL_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
