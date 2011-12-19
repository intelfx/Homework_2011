#ifndef _CSET_ORIGINAL_H
#define _CSET_ORIGINAL_H

#include "build.h"
#include "Interfaces.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			CommandSet_original.h
// Author		intelfx
// Description	Original (Processor mkI) command set provider.
// -----------------------------------------------------------------------------

namespace ProcessorImplementation
{
	using namespace Processor;

	class CommandSet_mkI : public ICommandSet
	{
		std::vector<CommandTraits> by_id;
		std::map<std::string, CommandTraits*> by_mnemonic;

		struct ICD
		{
			const char* name;
			const char* description;
			ArgumentType arg_type;
			bool exec_at_decode;
		};

		static const ICD initial_commands[];

	public:
		CommandSet_mkI();
		virtual ~CommandSet_mkI();

		virtual void ResetCommandSet();

		virtual void AddCommand (CommandTraits&& command);
		virtual void AddCommandImplementation (const char* mnemonic, size_t module, void* handle);

		virtual const CommandTraits& DecodeCommand (const char* mnemonic) const;
		virtual const CommandTraits& DecodeCommand (unsigned char id) const;

		virtual void* GetExecutionHandle (unsigned char id, size_t module);
	};
}

#endif // _CSET_ORIGINAL_H