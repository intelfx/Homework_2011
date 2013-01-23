#ifndef INTERPRETER_EXECUTOR_SERVICE_H
#define INTERPRETER_EXECUTOR_SERVICE_H

#include "build.h"

#include "Interfaces.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			Executor_service.h
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Untyped interpreter plugin implementation.
// -------------------------------------------------------------------------------------

DeclareDescriptor( ServiceExecutor, INTERPRETER_API, INTERPRETER_TE )

namespace ProcessorImplementation
{
using namespace Processor;

class INTERPRETER_API ServiceExecutor : LogBase( ServiceExecutor ), public IExecutor
{
	static const char* supported_mnemonics[];

protected:
	virtual void OnAttach();
	virtual Value::Type SupportedType() const;

public:
	virtual void Execute( void* handle, Command& command );
	virtual void ResetImplementations();
};

} // namespace ProcessorImplementation

#endif // INTERPRETER_EXECUTOR_SERVICE_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
