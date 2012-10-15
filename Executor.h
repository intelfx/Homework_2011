#ifndef INTERPRETER_EXECUTOR_H
#define INTERPRETER_EXECUTOR_H

#include "build.h"

#include "Interfaces.h"

// -------------------------------------------------------------------------------------
// Library		Homework
// File			Executor.h
// Author		Ivan Shapovalov <intelfx100@gmail.com>
// Description	Floating-point interpreter plugin implementation.
// -------------------------------------------------------------------------------------

DeclareDescriptor( FloatExecutor, INTERPRETER_API, INTERPRETER_TE );

namespace ProcessorImplementation
{
using namespace Processor;

class INTERPRETER_API FloatExecutor : LogBase( FloatExecutor ), public IExecutor
{
	static const char* supported_mnemonics[];

	static const size_t temp_size = 4;
	fp_t temp[temp_size];

	bool need_to_analyze;

	inline void AttemptAnalyze();
	inline void TopArgument();
	inline void PopArguments( size_t count );
	inline void ReadArgument( Reference& ref, Value::Type type = Value::V_FLOAT );
	inline void WriteResult( Reference& ref );
	inline void WriteIntResult( Reference& ref );
	inline void PushResult();

protected:
	virtual void OnAttach();
	virtual Value::Type SupportedType() const;

public:
	virtual void ResetImplementations();
	virtual void Execute( void* handle, Command& command );
};

} // namespace ProcessorImplementation

#endif // INTERPRETER_EXECUTOR_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
