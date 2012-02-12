#ifndef _EXECUTOR_SERVICE_H
#define _EXECUTOR_SERVICE_H

#include "build.h"
#include "Interfaces.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			Executor_service.h
// Author		intelfx
// Description	Service (no stack access) virtual executor implementation
// -----------------------------------------------------------------------------

DeclareDescriptor (ServiceExecutor);

namespace ProcessorImplementation
{
	using namespace Processor;

	class INTERPRETER_API ServiceExecutor : LogBase(ServiceExecutor), public IExecutor
	{
		static const char* supported_mnemonics[];

	protected:
		virtual void OnAttach();
		virtual Value::Type SupportedType() const;

	public:
		virtual void Execute (void* handle, Command& command);
		virtual void ResetImplementations();
	};
}

#endif // _EXECUTOR_SERVICE_H