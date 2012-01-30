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
	virtual void RegisterHandler (void(*handler)(int, const char*, char**)) = 0;
	virtual void DefaultHandlers() = 0;
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

static const size_t signal_trap_count = 4;
static int signal_trap_list[signal_trap_count] =
{
	SIGSEGV,
	SIGBUS,
	SIGFPE,
	SIGILL
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
	static struct sigaction ConstructAction (void(*handler)(int, siginfo_t*, void*))
	{
		struct sigaction action;
		memset (&action, 0, sizeof (action));

		action.sa_flags = SA_SIGINFO;

		if (handler)
			action.sa_sigaction = handler;

		else
			action.sa_handler = SIG_DFL;

		return action;
	}

	static char message_buffer[STATIC_LENGTH];
	static void* backtrace_buffer[STATIC_LENGTH];
	static char** symbolic_backtrace;
	static void(*user_handler)(int, const char*, char**);
	static bool handlers_set;

	static void _handler (int sig, siginfo_t* info, void* context)
	{
		int count = backtrace (backtrace_buffer, STATIC_LENGTH);
		symbolic_backtrace = backtrace_symbols (backtrace_buffer, count);

		if (info ->si_errno)
			snprintf (message_buffer, STATIC_LENGTH, "POSIX signal %d (%s) at %p: last_errno %d (%s)",
					sig, strsignal (sig), info ->si_addr, info ->si_errno, strerror (info ->si_errno));

		else
			snprintf (message_buffer, STATIC_LENGTH, "POSIX signal %d (%s) at %p",
					  sig, strsignal (sig), info ->si_addr);

		user_handler (count, message_buffer, symbolic_backtrace);
	}

public:

	virtual void DefaultHandlers()
	{
		struct sigaction default_action = ConstructAction (0);

		for (size_t i = 0; i < signal_trap_count; ++i)
		{
			sigaction (signal_trap_list[i], &default_action, 0);
			msg (E_INFO, E_DEBUG, "POSIX signal handler reset for %d \"%s\"",
				 signal_trap_list[i], strsignal (signal_trap_list[i]));
		}

		handlers_set = 0;
		user_handler = 0;

		msg (E_WARNING, E_DEBUG, "POSIX signal handlers reset");
	}

	virtual bool IsSet()
	{
		return handlers_set;
	}

	virtual void RegisterHandler (void(*handler)(int, const char*, char**))
	{
		struct sigaction custom_action = ConstructAction (&_handler);
		for (size_t i = 0; i < signal_trap_count; ++i)
		{
			sigaction (signal_trap_list[i], &custom_action, 0);
			msg (E_INFO, E_DEBUG, "POSIX signal handler set for %d \"%s\"",
				 signal_trap_list[i], strsignal (signal_trap_list[i]));
		}

		handlers_set = 1;
		user_handler = handler;

		stack_t new_stack;
		new_stack.ss_flags = SS_DISABLE;
		sigaltstack (&new_stack, 0);

		msg (E_WARNING, E_DEBUG, "POSIX signal handlers set and stack configured");
	}
};

char SignalHandler::message_buffer[STATIC_LENGTH];
void* SignalHandler::backtrace_buffer[STATIC_LENGTH];
char** SignalHandler::symbolic_backtrace = 0;
void(*SignalHandler::user_handler)(int, const char*, char**) = 0;
bool SignalHandler::handlers_set = 0;
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
		exceptions ->DefaultHandlers();

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

	ReentrantEnableEH();

	try
	{
		SafeExecute (reinterpret_cast<void*> (&selftest_function), 0);
	}

	catch (NativeException& e)
	{
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

	exceptions ->RegisterHandler (&ExceptionHandler);
	msg (E_INFO, E_VERBOSE, "Exception handling enabled, C++ exceptions will be thrown");
}

void NativeExecutionManager::DisableExceptionHandling()
{
	__assert (exceptions, "No exception handling interface is available");
	if (!exceptions ->IsSet())
		return;

	exceptions ->DefaultHandlers();
	msg (E_INFO, E_VERBOSE, "Exception handling disabled");
}

int NativeExecutionManager::SafeExecute (void* address, void* argument)
{
	if (!exceptions)
	{
		msg (E_WARNING, E_USER, "No exception handler registered - unsafe execution");
		return Fcast<int(*)(void*)> (address) (argument);
	}

	bool exceptions_were_set = exceptions ->IsSet();
	EnableExceptionHandling();

	// ----
	int result = Fcast<int(*)(void*)> (address) (argument);
	// ----

	if (!exceptions_were_set)
		DisableExceptionHandling();

	return result;
}

void NativeExecutionManager::ExceptionHandler (int backtrace_count, const char* message, char** backtrace)
{
// 	throw std::logic_error (message);
	throw NativeException (message, backtrace, backtrace_count);
}

NativeException::NativeException (const char* information, char** backtr, int count) :
Exception (THIS_PLACE, _specific_dbg_info, Debug::EX_INPUT, "<signal>", information, 0),
backtrace_ (backtr),
b_count_ (count)
{
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

