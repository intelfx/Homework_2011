#ifndef _FXJITRUNTIME_H
#define _FXJITRUNTIME_H

#include "build.h"

#include "fxassert.h"

// -----------------------------------------------------------------------------
// Library		uXray
// File			fxjitruntime.h
// Author		intelfx
// Description	Memory and exception handling primitives for JIT applications
// -----------------------------------------------------------------------------

class IExceptionRehandler;
class IMemoryMapper;

DeclareDescriptor (NativeExecutionManager);

class FXLIB_API NativeExecutionManager : LogBase (NativeExecutionManager)
{
	IExceptionRehandler* exceptions;
	IMemoryMapper* memory;

	static void ExceptionHandler (int, const char*, char**);

public:
	NativeExecutionManager();
	~NativeExecutionManager();

	bool ExceptionHandlingAvailable() const;
	bool MemoryOperationsAvailable() const;

	// Describes required application behavior in case of "unrecoverable" error
	enum FaultBehavior
	{
		/* native OS error handling, usually termination */
		FB_DEFAULT = 0,

		/* exception should be generated instead of termination */
		FB_CUSTOM_EXCEPTION = 1,

		/* debug system crash dump should be generated */
		FB_GENERATE_DEBUG_CRASHDUMP = 2,

		/* native backtrace/crash dump should be generated */
		FB_GENERATE_NATIVE_CRASHDUMP = 3
	};

	// Sets application behavior in case of _handled_ native faults (segfaults, access violations etc).
	// Accepts mask of FaultBehavior entries.
	void SetFaultBehavior (mask_t behavior = MASK (FB_DEFAULT));

	// Enables custom handlers for some common native exceptions in order to provide
	// custom behavior set using SetFaultBehavior().
	void EnableExceptionHandling();

	// Disables all set custom handlers, returning exception handling to the platform.
	void DisableExceptionHandling();

	void* AllocateMemory (size_t length, bool is_readable, bool is_writeable, bool is_executable);
	void DeallocateMemory (void* address, size_t length);

	// Safely (temporarily installing all the handlers)
	// executes code within the given address, passing argument to it
	// and returning its return value.
	// Throws a C++ exception in case of a native exception.
	int SafeExecute (void* address, void* argument);
};

class FXLIB_API NativeException : public Debug::Exception
{
	char** backtrace_;
	int b_count_;

public:
	NativeException (const NativeException&) = delete;
	NativeException& operator= (const NativeException&) = delete;

	NativeException (const char* information, char** backtr, int count);
	virtual ~NativeException() throw();

	NativeException (NativeException&& that);

	char** backtrace() const throw()
	{
		return backtrace_;
	}

	int backtrace_length() const throw()
	{
		return b_count_;
	}
};

#endif // _FXJITRUNTIME_H