#include "stdafx.h"
#include "fxassert.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxassert.cpp
// Author		intelfx
// Description	Log/debug system and reworked assertion
// -----------------------------------------------------------------------------

static const Debug::ObjectDescriptor_ _global_dbg_info = {Debug::GLOBAL_OID, "global scope", Debug::MOD_APPMODULE,
														  Debug::E_UNDEFINED_VERBOSITY, Debug::E_UNDEFINED_TYPE, 0
														 };

const Debug::ObjectDescriptor_ Debug::_default_dbg_info = {Debug::GLOBAL_OID, "undefined object", Debug::MOD_INTERNAL,
														   Debug::E_UNDEFINED_VERBOSITY, Debug::E_UNDEFINED_TYPE,
														   MASK (Debug::OF_FATALVERIFY) | MASK (Debug::OF_USEVERIFY)
														  };

const Debug::ObjectDescriptor_* _specific_dbg_info = &_global_dbg_info;

FXLIB_API bool fx_vasprintf (char** dest, const char* fmt, va_list args)
{
#ifdef _GNU_SOURCE
	int vasprintf_result = vasprintf (dest, fmt, args);

	if (vasprintf_result != -1)
		return 1;
#endif

	// Fallback to another implementation when vasprintf is either unavailable or failed.

	// Try to determine length via vsnprintf with NULL pointer and size, as by POSIX/C99.
	// Otherwise, if runtime does not conform, use static method.
	int length = vsnprintf (0, 0, fmt, args);
	if (length < 1)
		length = STATIC_LENGTH;

	// Allocate the memory.
	char* staticmem = reinterpret_cast<char*> (calloc (length + 1, 1));
	if (!staticmem)
	{
		// We can't do much if calloc() fails.
		*dest = 0;
		return 0;
	}

	// We won't check if our length is sufficient, we'll just cut off what's excess.
	int vsnprintf_result = vsnprintf (staticmem, length, fmt, args);

	if (vsnprintf_result != -1)
		return 1;

	// Everything fails.
	return 0;
}

int dosilentthrow (Debug::ObjectParameters object,
				   Debug::SourceDescriptor place,
				   size_t error_code)
{
	throw Debug::SilentException (object, place, error_code);
	return 1;
}


FXLIB_API int dothrow (Debug::ObjectParameters object,
                       Debug::SourceDescriptor place,
					   Debug::ExceptionType_ ex_type,
					   const char* expr,
					   const char* fmt, ...)
{
	va_list args;
	va_start (args, fmt);

	Debug::Exception exception (place, object, ex_type, expr, fmt, args);

	va_end (args);
	throw std::move (exception);
	return 1;
}

FXLIB_API int seterror (Debug::ObjectParameters object, const char* fmt, ...)
{
	va_list lst;
	va_start (lst, fmt);

	if (object.error_string)
		free (object.error_string);

	fx_vasprintf (&object.error_string, fmt, lst);

	va_end (lst);
	return 1;
}


FXLIB_API int call_log (Debug::ObjectParameters object,
						Debug::SourceDescriptor place,
						Debug::EventTypeIndex_ event_type,
						Debug::EventLevelIndex_ event_level,
						const char* fmt, ...)
{
	if (!Debug::System::Instance.IsAvailable())
		return 0;

	if (object.object_status == Debug::OS_MOVED)
		return 0;

	__sassert (event_type != Debug::E_UNDEFINED_TYPE, "Message has undefined type");
	__sassert (event_level != Debug::E_UNDEFINED_VERBOSITY, "Message has undefined verbosity");

	Debug::EventDescriptor_ event;
	event.event_type = event_type;
	event.event_level = event_level;
	event.message_format_string = fmt;

	va_start (event.message_args, fmt);

	Debug::System::Instance().DoLogging (event, place, object);

	va_end (event.message_args);
	return 1;
}

namespace Debug
{
SilentException::SilentException (SilentException&& that) :
error_code_ (that.error_code_),
a_code_ (that.a_code_)
{
	that.a_code_ = 0;
}

SilentException::SilentException (Debug::ObjectParameters object,
								  Debug::SourceDescriptor place,
								  size_t error_code) :
error_code_ (error_code),
a_code_ (0)
{
	fx_asprintf (&a_code_, "Silent exception [%s%s]: status %s, error code %ld",
	             (object.object_descriptor ->object_type == MOD_APPMODULE) ? "module " : "",
	             object.object_descriptor ->object_name,
	             (object.object_status == OS_BAD) ? "BAD" :
	             (object.object_status != OS_OK) ? "N/A" : "OK",
	             error_code);

	call_log (object, place, E_WARNING, E_VERBOSE, a_code_);
}

SilentException::~SilentException() throw()
{
	free (a_code_);
}

const char* SilentException::what() const throw()
{
	return a_code_;
}

_InsideBase::_InsideBase() :
dbg_info_ (0),
dbg_params_ (Debug::CreateParameters (this, dbg_info_)) // stub
{
}

Exception::Exception (SourceDescriptor place,
                      ObjectParameters object,
                      ExceptionType_ type,
                      const char* expression,
                      const char* fmt, va_list args) :
type_ (type),
expression_ (expression),
a_reason_ (0),
a_message_ (0),
reason_ (0),
message_ (0)
{
	// Construct (assemble) the user-supplied reason string.
	// We can't just fail the whole program on *asprintf fail.
	if (!fx_vasprintf (&a_reason_, fmt, args))
		reason_ = "Reason can't be constructed due to internal vasprintf failure";

	else
		reason_ = a_reason_;

	// Construct the whole message string (including both user-supplied reason and technical details).
	// We must select the appropriate format string according to the exception type.
	// First argument is reason, second is failed expression.
	const char* msg_fmt = 0;

	switch (type_)
	{
	case EX_BUG:
		msg_fmt = "Assertion failed on \"%2$s\": %1$s."; // Notice %*$ !!!
		break;

	case EX_INPUT:
		msg_fmt = "%1$s.";
		break;

	default:
		msg_fmt = 0;
		break;
	}

	if (!fx_asprintf (&a_message_, msg_fmt, reason_, expression_))
		message_ = "Message can't be constructed due to internal vasprintf failure";

	else
		message_ = a_message_;

	// Perform exception logging
	call_log (object, place,
			  E_EXCEPTION, E_USER,
			  message_);
}

Exception::Exception (Exception&& that) :
type_ (that.type_),
expression_ (that.expression_),
a_reason_ (that.a_reason_),
a_message_ (that.a_message_),
reason_ (that.reason_),
message_ (that.message_)
{
	that.expression_ = 0;
	that.a_reason_ = 0;
	that.a_message_ = 0;
	that.reason_ = 0;
	that.message_ = 0;
}

Exception::~Exception() throw()
{
	free (a_reason_);
	free (a_message_);
}

const char* Exception::what() const throw()
{
	return message_;
}

System::System() :
default_target (CreateTarget ("", EVERYTHING, EVERYTHING)),
state (S_UNINITIALIZED)
{
}

System::~System()
{
	state = S_UNINITIALIZED;

	// By destructor, the logger backend has probably gone off already.
	// TODO: make some detection when logger backend is still available or not
	// CloseTargets();
}

void System::CloseTargets()
{
	if (TargetIsOK (&default_target))
	{
		msg (E_INFO, E_VERBOSE, "Target is being closed (destination \"%s\")",
			 default_target.target_name);

		state = S_UNINITIALIZED; // Avoid biting our own tail in numerous situations.
		Debug::CloseTarget (&default_target);
	}
}

void System::SetTargetProperties (TargetDescriptor target, LogEngine* logger)
{
	CloseTargets();

	default_target = target;
	logger ->RegisterTarget (&default_target);

	if (state != S_READY)
	{
		state = S_READY;
		msg (E_INFO, E_VERBOSE, "Logger becomes operational");
	}

	msg (E_INFO, E_VERBOSE, "New target initialized using %s, destination \"%s\"",
		 API::GetClassName (logger),
		 target.target_name);
}

void System::DoLogging (EventDescriptor event,
						SourceDescriptor place,
						ObjectParameters object)
{
	bool is_emergency_mode = object.is_emergency_mode;
	bool is_critical_message = (event.event_type >= E_CRITICAL);
	bool is_message_allowed = CheckDynamicVerbosity (*object.object_descriptor, event);
	TargetDescriptor_* current_target = 0;

	if (!is_message_allowed)
		return;

	// In this state we will simply discard the message.
	if (state == S_UNINITIALIZED)
		return;

	// This is the special mode of debug system.
	// Critical messages are dumped to stderr, others are ignored.
	if (state == S_FATALERROR)
	{
		if (is_critical_message)
			vfprintf (stderr, event.message_format_string, event.message_args);

		return;
	}

	// In emergency state we will use default target.
	if (is_emergency_mode)
		current_target = &default_target;

	else
	{
		// Current implementation does not support multiple targets.
		if (!TargetAcceptsEvent (event, default_target)) return;
		current_target = &default_target;
	}

	// The following code will do error handling in case of debug system/target/logger failures.
	// I know that it's overkill for just one target.
	if (!TargetIsOK (current_target))
	{
		HandleError (event, place, object, "Invalid target");
		return;
	}

	// Finally construct and write the atom.
	try
	{
		LogAtom_ atom = {event, place, object, *current_target};

		// If there is an exception in WriteOutAtom, it won't cause endless recursion.
		state = S_UNINITIALIZED;
		WriteOutAtom (atom, is_emergency_mode);
		state = S_READY;
	}

	catch (std::exception e)
	{
		HandleError (event, place, object, e.what());
	}

	catch (...)
	{
		HandleError (event, place, object, "Unknown logger error");
	}
}

void System::HandleError (EventDescriptor event, SourceDescriptor place, ObjectParameters object,
						  const char* error_string) throw()
{
	bool is_emergency_mode = object.is_emergency_mode;
	bool is_critical_message = (event.event_type >= E_CRITICAL);

	// The worst-case situation: failed emergency write.
	// Write "last breath" error message
	if (is_emergency_mode)
	{
		fprintf (stderr, "\n\n"
				 "---- DEBUG SYSTEM FATAL ERROR ----\n"
				 "%s while in emergency mode.\n"
				 "The original message follows.\n\n", error_string);

		vfprintf (stderr, event.message_format_string, event.message_args);

		fprintf (stderr, "\n\n"
		         "---- THE LOGGING SYSTEM IS IN FATAL ERROR MODE ----\n"
				 "All critical messages will be reported to stderr from now on.\n"
				 "Press Enter.\n\n");

		state = S_FATALERROR;
		getchar();
	}

	else
	{
		// Non-critical messages are "lost".
		// Critical ones are reposted in emergency mode.
		if (is_critical_message)
			DoLogging (event, place, CreateEmergencyObject (object));
	}
}

void _InsideBase::_MoveDynamicDbgInfo (const Debug::_InsideBase* that)
{
	dbg_info_ = that ->dbg_info_;
	dbg_params_ = that ->dbg_params_;

	that ->dbg_params_.object_status = OS_MOVED;
}


void _InsideBase::_SetDynamicDbgInfo (const ObjectDescriptor_* info)
{
	// If pointer is already set, don't do anything.
	if (dbg_info_)
		return;

	// Else, set the dynamic pointer and do the logging.
	dbg_info_ = info;
	dbg_params_ = CreateParameters (this, dbg_info_);

	SourceDescriptor_ place = THIS_PLACE; // Place information is a kind of "stub"
	EventLevelIndex_ ctor_event_level;

	// Auto-determine verbosity of object creation message (based on common sense).
	switch (dbg_info_ ->object_type)
	{
	default:
	case MOD_INTERNAL:
		return; // Do not log them at all

	case MOD_APPMODULE:
		ctor_event_level = E_VERBOSE; // Module creation logging is "verbose" message
		break;

	case MOD_OBJECT:
		ctor_event_level = E_DEBUG; // Logging creation of objects is intended for debugging only
	};

	call_log (dbg_params_, place, // Collected data
	          E_OBJCREATION, ctor_event_level, // Generated data
	          GetCtorFmt_(), dbg_info_ ->object_name, this); // Fmt and arguments
}

_InsideBase::~_InsideBase()
{
	SourceDescriptor_ place = THIS_PLACE;
	EventLevelIndex_ dtor_event_level;

	switch (dbg_info_ ->object_type)
	{
		default:
		case MOD_INTERNAL:
			return;

		case MOD_APPMODULE:
			dtor_event_level = E_VERBOSE;
			break;

		case MOD_OBJECT:
			dtor_event_level = E_DEBUG;
	};

	call_log (dbg_params_, place,
	          E_OBJDESTRUCTION, dtor_event_level,
	          GetDtorFmt_(), dbg_info_ ->object_name, this);
}

const char* _InsideBase::GetCtorFmt_()
{
	switch (dbg_info_ ->object_type)
	{
		default:
		case MOD_INTERNAL:
			return 0;

		case MOD_OBJECT:
			return "Created a new %s";

		case MOD_APPMODULE:
			return "Instance of %s created @ 0x%08x";
	}
}

const char* _InsideBase::GetDtorFmt_()
{
	switch (dbg_info_ ->object_type)
	{
		default:
		case MOD_INTERNAL:
			return 0;

		case MOD_OBJECT:
			return "A %s has been removed";

		case MOD_APPMODULE:
			return "Instance of %s deleted @ 0x%08x";
	}
}

ImplementDescriptor(System, "logging system", MOD_INTERNAL, System);
} // namespace Debug

// kate: indent-mode cstyle; replace-tabs off; tab-width 4;
