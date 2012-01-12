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
		std::map<cid_t, CommandTraits> by_id;

		struct ICD
		{
			const char* name;
			const char* description;
			ArgumentType arg_type;
			bool is_service_command;
		};

		static const ICD initial_commands[];

		inline static cid_t get_id (const char* mnemonic)
		{
			return std::_Hash_impl::hash (mnemonic, strlen (mnemonic));
		}

	protected:
		virtual void OnAttach();

	public:
		CommandSet_mkI();
		virtual ~CommandSet_mkI();

		virtual void ResetCommandSet();

		virtual void AddCommand (CommandTraits&& command);
		virtual void AddCommandImplementation (const char* mnemonic, size_t module, void* handle);

		virtual const CommandTraits& DecodeCommand (const char* mnemonic) const;
		virtual const CommandTraits& DecodeCommand (cid_t id) const;

		virtual void* GetExecutionHandle (Processor::cid_t id, size_t module);
		virtual void* GetExecutionHandle (const CommandTraits& cmd, size_t module);
	};
}

#endif // _CSET_ORIGINAL_H