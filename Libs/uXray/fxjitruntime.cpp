#include "stdafx.h"
#include "fxjitruntime.h"

#if defined(TARGET_POSIX)
# include <signal.h>
# include <sys/mman.h>
# include <execinfo.h>
#else
# include <signal.h>
#endif

// -----------------------------------------------------------------------------
// Library		uXray
// File			fxjitruntime.cpp
// Author		intelfx
// Description	Memory and exception handling primitives for JIT applications
// -----------------------------------------------------------------------------

DeclareDescriptor (IExceptionRehandler);
DeclareDescriptor (IMemoryMapper);

ImplementDescriptor (IExceptionRehandler, "native exception handler", MOD_APPMODULE);
ImplementDescriptor (IMemoryMapper, "native memory allocator", MOD_APPMODULE);

ImplementDescriptor (NativeExecutionManager, "native system manager", MOD_APPMODULE);

class IExceptionRehandler : LogBase (IExceptionRehandler)
{
public:
	virtual bool IsSet() = 0;

	// Sets a custom signal handler
	virtual void SetHandlers (void(*handler)(int, const char*, char**)) = 0;

	// Clears custom signal handlers
	virtual void ResetHandlers() = 0;

	// Re-activates an existing signal handler
	virtual void ActivateHandlers() = 0;
};

class IMemoryMapper : LogBase (IMemoryMapper)
{
public:
	virtual void* Allocate (size_t length, bool is_writeable, bool is_executable) = 0;
	virtual void Deallocate (void* base, size_t length) = 0;
};

#if defined(TARGET_POSIX)
DeclareDescriptor (MmapImplementation);
ImplementDescriptor (MmapImplementation, "POSIX memory mapper", MOD_APPMODULE);

DeclareDescriptor (SignalHandler);
ImplementDescriptor (SignalHandler, "POSIX signal handler", MOD_APPMODULE);

static const size_t signal_trap_count = 5;
static int signal_trap_list[signal_trap_count] =
{
	SIGSEGV,
	SIGBUS,
	SIGFPE,
	SIGILL,
	SIGABRT
};

class MmapImplementation : LogBase (MmapImplementation), public IMemoryMapper
{
public:
	MmapImplementation()
	{
		static const size_t test_size = 0x100;
		void* test_alloc = mmap (0, test_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE, 0, 0);

		__assert (test_alloc, "POSIX memory allocation failed: mmap() says %s", strerror (errno));
		munmap (test_alloc, test_size);
	}

	virtual void* Allocate (size_t length, bool is_writeable, bool is_executable)
	{
		int flags = PROT_READ;

		if (is_writeable)
			flags |= PROT_WRITE;

		if (is_executable)
			flags |= PROT_EXEC;

		void* allocation = mmap (0, length, flags, MAP_PRIVATE, 0, 0);
		__assert (allocation, "POSIX memory allocation [%s%ssize %zu] failed: mmap() says %s",
				  is_writeable ? "writeable " : "", is_executable ? "executable " : "", strerror (errno));

		msg (E_INFO, E_DEBUG, "POSIX memory allocation [%s%ssize %zu]: mapping established at %p",
			 is_writeable ? "writeable " : "", is_executable ? "executable " : "", allocation);

		return allocation;
	}

	virtual void Deallocate (void* base, size_t length)
	{
		int status = munmap (base, length);
		__assert (!status, "POSIX memory deallocation [at %p size %zu] failed: mmap() says %s",
				  strerror (errno));

		msg (E_INFO, E_DEBUG, "POSIX memory deallocation [at %p size %zu]: unmapped",
			 base, length);
	}
};

class SignalHandler : LogBase (SignalHandler), public IExceptionRehandler
{
	static void RegisterAction (int signal, void(*handler)(int, siginfo_t*, void*))
	{
		struct sigaction action;
		memset (&action, 0, sizeof (action));

		action.sa_flags = SA_SIGINFO;

		if (handler)
			action.sa_sigaction = handler;

		else
			action.sa_handler = SIG_DFL;

		sigaction (signal, &action, 0);
	}

	static char message_buffer[STATIC_LENGTH];
	static void* backtrace_buffer[STATIC_LENGTH];
	static char** symbolic_backtrace;
	static void(*user_handler)(int, const char*, char**);

	static void internal_handler (int sig, siginfo_t* info, void* context)
	{
		int count = backtrace (backtrace_buffer, STATIC_LENGTH);
		symbolic_backtrace = backtrace_symbols (backtrace_buffer, count);

		if (info ->si_errno)
			snprintf (message_buffer, STATIC_LENGTH, "POSIX signal %d (%s) at %p: last_errno %d (%s)",
					sig, strsignal (sig), info ->si_addr, info ->si_errno, strerror (info ->si_errno));

		else
			snprintf (message_buffer, STATIC_LENGTH, "POSIX signal %d (%s) at %p",
					  sig, strsignal (sig), info ->si_addr);

		RegisterAction (sig, 0); // clear user handler for caught signal to eliminate recursive terminate()

		user_handler (count, message_buffer, symbolic_backtrace);
	}

public:

	virtual void ActivateHandlers()
	{
		for (size_t i = 0; i < signal_trap_count; ++i)
		{
			RegisterAction (signal_trap_list[i], internal_handler);
			msg (E_INFO, E_DEBUG, "POSIX signal handler updated for %d \"%s\"",
				 signal_trap_list[i], strsignal (signal_trap_list[i]));
		}

		msg (E_WARNING, E_DEBUG, "POSIX signal handlers updated");

	}

	virtual bool IsSet()
	{
		return user_handler ? 1 : 0;
	}

	virtual void SetHandlers (void(*handler)(int, const char*, char**))
	{
		user_handler = handler;
		ActivateHandlers();

		stack_t new_stack;
		new_stack.ss_flags = SS_DISABLE;
		sigaltstack (&new_stack, 0);
	}

	virtual void ResetHandlers()
	{
		user_handler = 0;
		ActivateHandlers();
	}
};

char SignalHandler::message_buffer[STATIC_LENGTH];
void* SignalHandler::backtrace_buffer[STATIC_LENGTH];
char** SignalHandler::symbolic_backtrace = 0;
void(*SignalHandler::user_handler)(int, const char*, char**) = 0;
#endif

NativeExecutionManager::NativeExecutionManager() :
#if defined(TARGET_POSIX)
exceptions (new SignalHandler),
memory (new MmapImplementation),
#else
exceptions (0),
memory (0),
#endif
default_eh_state (0),
reentrant_eh_count (0)
{
}

void NativeExecutionManager::ReentrantEnableEH()
{
	__assert (exceptions, "No exception handling interface is available");

	if (!reentrant_eh_count)
		default_eh_state = exceptions ->IsSet();

	EnableExceptionHandling();
	++reentrant_eh_count;
}

void NativeExecutionManager::ReentrantDisableEH()
{
	__assert (exceptions, "No exception handling interface is available");

	if (!reentrant_eh_count)
		return;

	--reentrant_eh_count;

	if (!reentrant_eh_count)
	{
		if (default_eh_state)
			EnableExceptionHandling();

		else
			DisableExceptionHandling();

		default_eh_state = 0;
	}
}

NativeExecutionManager::~NativeExecutionManager()
{
	if (exceptions)
		exceptions ->ResetHandlers();

	delete exceptions;
	delete memory;
}

static int selftest_function (int* ptr)
{
	return *ptr;
}

int NativeExecutionManager::EHSelftest()
{
	msg (E_WARNING, E_VERBOSE, "Exception handler self-tests");

	if (!exceptions)
	{
		msg (E_WARNING, E_VERBOSE, "Exception handler is not available");
		return 0;
	}

	try
	{
		ReentrantEnableEH();
		SafeExecute (reinterpret_cast<void*> (&selftest_function), 0);
		ReentrantDisableEH();
	}

	catch (NativeException& e)
	{
		e.Handle();
		msg (E_INFO, E_VERBOSE, "Native exception caught: %s", e.what());
	}

	catch (...)
	{
		msg (E_CRITICAL, E_USER, "Unknown exception caught");
		return 0;
	}

	msg (E_WARNING, E_VERBOSE, "Segfault handler self-test OK");
	return 1;
}

void* NativeExecutionManager::AllocateMemory (size_t length, bool, bool is_writeable, bool is_executable)
{
	__assert (memory, "No memory mapper interface is available");
	void* allocated = memory ->Allocate (length, is_writeable, is_executable);

	msg (E_INFO, E_VERBOSE, "Allocated %zu bytes", length);
	return allocated;
}

void NativeExecutionManager::DeallocateMemory (void* address, size_t length)
{
	__assert (memory, "No memory mapper interface is available");
	memory ->Deallocate (address, length);
}

bool NativeExecutionManager::MemoryOperationsAvailable() const
{
	return !!memory;
}

bool NativeExecutionManager::ExceptionHandlingAvailable() const
{
	return !!exceptions;
}

void NativeExecutionManager::EnableExceptionHandling()
{
	__assert (exceptions, "No exception handling interface is available");
	if (exceptions ->IsSet())
		return;

	exceptions ->SetHandlers (&EmitNativeException);
	msg (E_INFO, E_VERBOSE, "Exception handling enabled, C++ exceptions will be thrown");
}

void NativeExecutionManager::DisableExceptionHandling()
{
	__assert (exceptions, "No exception handling interface is available");
	if (!exceptions ->IsSet())
		return;

	exceptions ->ResetHandlers();
	msg (E_INFO, E_VERBOSE, "Exception handling disabled");
}

void NativeExecutionManager::RestoreExceptionHandling()
{
	__assert (exceptions, "No exception handling interface is available");

	exceptions ->ActivateHandlers();
	msg (E_INFO, E_VERBOSE, "Exception handling restored");
}

int NativeExecutionManager::SafeExecute (void* address, void* argument)
{
	if (!exceptions)
	{
		msg (E_WARNING, E_USER, "No exception handler registered - unsafe execution");
		return Fcast<int(*)(void*)> (address) (argument);
	}

	ReentrantEnableEH();

	// ----
	int result = Fcast<int(*)(void*)> (address) (argument);
	// ----

	ReentrantDisableEH();

	return result;
}

void NativeExecutionManager::EmitNativeException (int backtrace_count, const char* message, char** backtrace)
{
// 	throw std::logic_error (message);
	throw NativeException (THIS_PLACE, message, backtrace, backtrace_count);
}

NativeException::NativeException (Debug::SourceDescriptor src, const char* information, char** backtrace, int backtrace_count) :
Exception (src, _specific_dbg_info, Debug::EX_INPUT, "<signal>", information, 0),
backtrace_ (backtrace),
b_count_ (backtrace_count),
handled_ (0)
{
}

void NativeException::DumpBacktrace() const throw()
{
	fprintf (stderr, "---- Dumping crash backtrace\t----\n\n");

	int shown_count = (b_count_ <= 10) ? b_count_ : 10;
	for (int i = 0; i < shown_count; ++i)
	{
		char* backtrace_rpath = 0;
		if (backtrace_rpath = strrchr (backtrace_[i], '/'))
			++backtrace_rpath;

		else
			backtrace_rpath = backtrace_[i];

		fprintf (stderr, "* Call %d: %s\n", i, backtrace_rpath);
	}
	fprintf (stderr, "\n---- Backtrace dump complete\t----\n\n");
}

const char* NativeException::what() const throw()
{
	if (!handled_)
	{
		fprintf (stderr, "\n---- UNHANDLED NATIVE EXCEPTION ----\n");
		DumpBacktrace();
	}

	return Debug::Exception::what();
}

NativeException::~NativeException() throw()
{
	free (backtrace_);
}

NativeException::NativeException (NativeException&& that) :
Exception (std::move (that)),
backtrace_ (that.backtrace_),
b_count_ (that.b_count_)
{
	that.backtrace_ = 0;
	that.b_count_ = 0;
}

