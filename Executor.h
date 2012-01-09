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

namespace ProcessorImplementation
{
	using namespace Processor;

	class Executor : public IExecutor
	{
		static const char* supported_mnemonics[];

		static const size_t temp_size = 4;
		fp_t temp[temp_size];

		struct Fl
		{
			bool negative;
			bool zero;
			bool analyze_always;
			bool invalid_fp;
		} temp_flags;

		inline void AttemptAnalyze	();
		inline void TopArgument		();
		inline void PopArguments	(size_t count);
		inline void ReadArgument	(Reference& ref);
		inline void WriteResult		(Reference& ref);
		inline void PushResult		();
		inline void RetrieveFlags	();

	protected:
		virtual void OnAttach();
		virtual Value::Type SupportedType() const;

	public:
		virtual void ResetImplementations();
		virtual void Execute (void* handle, Command::Argument& argument);
	};
}

#endif // _EXECUTOR_H