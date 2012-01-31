#ifndef _EXECUTOR_H
#define _EXECUTOR_H

// -----------------------------------------------------------------------------
// Library		Homework
// File			Executor.h
// Author		intelfx
// Description	Main virtual executor implementation
// -----------------------------------------------------------------------------

#include "build.h"
#include "Interfaces.h"

DeclareDescriptor (FloatExecutor);

namespace ProcessorImplementation
{
	using namespace Processor;

	class INTERPRETER_API FloatExecutor : LogBase (FloatExecutor), public IExecutor
	{
		static const char* supported_mnemonics[];

		static const size_t temp_size = 4;
		fp_t temp[temp_size];

		bool need_to_analyze;

		inline void AttemptAnalyze	();
		inline void TopArgument		();
		inline void PopArguments	(size_t count);
		inline void ReadArgument	(Reference& ref);
		inline void WriteResult		(Reference& ref);
		inline void PushResult		();

	protected:
		virtual void OnAttach();
		virtual Value::Type SupportedType() const;

	public:
		virtual void ResetImplementations();
		virtual void Execute (void* handle, Command::Argument& argument);
	};
}

#endif // _EXECUTOR_H