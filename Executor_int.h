#ifndef _EXECUTOR_INT_H
#define _EXECUTOR_INT_H

// -----------------------------------------------------------------------------
// Library		Homework
// File			Executor_int.h
// Author		intelfx
// Description	Integer type virtual executor implementation
// -----------------------------------------------------------------------------

#include "build.h"
#include "Interfaces.h"

DeclareDescriptor (IntegerExecutor);

namespace ProcessorImplementation
{
	using namespace Processor;

	class IntegerExecutor : LogBase (IntegerExecutor), public IExecutor
	{
		static const char* supported_mnemonics[];

		static const size_t temp_size = 4;
		int_t temp[temp_size];

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

#endif // _EXECUTOR_INT_H