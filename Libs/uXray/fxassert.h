#ifndef _FXASSERT_H_
#define _FXASSERT_H_

#include "build.h"

#include "debugmacros.h"

/*
 * Per rectum ad astra.
 */

// -----------------------------------------------------------------------------
// Library		FXLib
// File			fxassert.h
// Author		intelfx
// Description	Log/debug system and reworked assertion
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// ---- RTTI-enabled generic wrapper
// -----------------------------------------------------------------------------

template <typename T>
class rtti_enabled : public T
{
public:
	rtti_enabled (T&& object);
	virtual ~rtti_enabled();
};

template <typename T>
rtti_enabled<T>::rtti_enabled (T&& object) : T (std::move (object)) {}

template <typename T>
rtti_enabled<T>::~rtti_enabled() = default;

// -----------------------------------------------------------------------------
// ---- Object control
// -----------------------------------------------------------------------------

namespace Init
{
	template <typename T>
	class FXLIB_API Controller
	{
		T* instance_;
		bool is_available_;

	public:

		Controller() : instance_ (0), is_available_ (1) {}
		~Controller() { ForceDelete(); }

		T& operator() () // Fuck the world, forgot second pair of brackets
		{
			if (!is_available_)
			{
				char message [STATIC_LENGTH];
				snprintf (message, STATIC_LENGTH, "Request for global object after its destruction [typeid %s]",
						  typeid (T).name());

				throw std::logic_error (message);
			}

			if (!instance_) instance_ = new T;
			return *instance_;
		}

		void ForceDelete() { delete instance_; instance_ = 0; is_available_ = 0; }
		bool IsAvailable() { return is_available_; }
	};

	template <typename T>
	struct FXLIB_API Base
	{
		static Controller<T> Instance;
	};

	template <typename T>
	Controller<T> Base<T>::Instance;
}

// -----------------------------------------------------------------------------
// ---- The mainline debugging system
// -----------------------------------------------------------------------------

/*
 * WARNING
 * Make sure your classes that derive from VerifierWrapper<> (uses LogBase) have
 * EXPLICIT visibility-default (non-inline and attributes set) ctor/dtor!
 *
 * The DeclareDescriptor/ImplementDescriptor macros does not have their own access specifiers
 * (like FXLIB_API) because they can be used in whatever library and from whatever library.
 * And it seems that generated VerifierWrapper fields "inherit" their visibility from descendant.
 *
 * TODO verify everything on MSVC, since we've probably gone into "unspecified behavior".
 */

namespace Debug
{




	// ---------------------------------------
	// Enumerations
	// ---------------------------------------

	// Specifies logged event's type for the logger to select targets and verbosity levels.
	enum EventTypeIndex_
	{
		E_UNDEFINED_TYPE = 0,
		E_INFO,				// Normal information message.
		E_WARNING,			// Non-critical (recoverable) error message.
		E_CRITICAL,			// Critical (unrecoverable) error message. Application usually cannot continue.

		// These types have special meaning and must be handled differently. See Debug::LogEngine.

		E_OBJCREATION,		// Message about object creation.
		E_OBJDESTRUCTION,	// Message about object destruction.
		E_EXCEPTION,		// Message is spawned on exception throw.
		E_FUCKING_EPIC_SHIT	// "Impossible" message for reasons like invalid code flow.
	};

	// Specifies logged event's desired verbosity level.
	enum EventLevelIndex_
	{
		E_UNDEFINED_VERBOSITY = 0,
		E_USER,			// User-readable and critical messages.
		E_VERBOSE,		// Verbose user-readable messages.
		E_DEBUG		 	// Debugging messages.
	};

	// Specifies this object's type for the logger to select right log targets and verbosity levels.
	enum ModuleType_
	{
		MOD_INTERNAL,	// An internal module/object (like a data structure or node)
		MOD_APPMODULE,	// This is an application module (like an IO subsystem)
		MOD_OBJECT		// This is an object (like an entity, surface or rendering context)
	};

	enum ExceptionType_
	{
		EX_BUG,			// Internal application error (bug)
		EX_INPUT		// Incorrect input (user's fault)
	};

	enum ObjectState_
	{
		OS_UNCHECKED = 0, // Verification has not been performed
		OS_MOVED, // Object is moved and must not provide logs anymore
		OS_BAD, // Verification failed
		OS_OK // Verification passed
	};

	enum ObjectFlags_
	{
		OF_FATALVERIFY = 1, // Whether to fail on bad object status immediately [ false - internal assertion on _Verify() is disabled ]
		OF_USEVERIFY, // Whether to use verification method Verify_() [ false - _Verify() is replaced by "return true" ]
		OF_USECHECK // Whether to enable verification interface CheckObject() [ false - CheckObject() does not call _Verify() ]
	};




	// ---------------------------------------
	// Predefinitions
	// ---------------------------------------

	class FXLIB_API LogEngine;
	class FXLIB_API ABIEngine;
	class FXLIB_API VerifierBase;




	// ---------------------------------------
	// Structures
	// ---------------------------------------

	// User-passed: event classification and message
	struct EventDescriptor_
	{
		EventTypeIndex_ event_type;
		EventLevelIndex_ event_level;
		const char* message_format_string;
		mutable va_list message_args; // mutable, since v*printf() does not accept "const va_list"

		EventDescriptor_ (EventTypeIndex_ type, EventLevelIndex_ level, const char* fmt) :
		event_type (type),
		event_level (level),
		message_format_string (fmt)
		{
		}

		bool IsCritical() const
		{
			return (event_type == E_CRITICAL) ||
				   (event_type == E_EXCEPTION) ||
				   (event_type == E_FUCKING_EPIC_SHIT);
		}
	};
	typedef const EventDescriptor_& EventDescriptor;


	// This is created and maintained by debug system
	struct TargetDescriptor_
	{
		const char* target_name; // Name of write destination (file, stream etc.)
		void* target_descriptor;
		LogEngine* target_engine;

		mask_t target_typemask;
		mask_t target_levelmask;

		TargetDescriptor_ (const char* name, mask_t typemask, mask_t levelmask) :
		target_name (name),
		target_descriptor (0),
		target_engine (0),
		target_typemask (typemask),
		target_levelmask (levelmask)
		{
		}

		TargetDescriptor_() :
		target_name (0),
		target_descriptor (0),
		target_engine (0),
		target_typemask (EVERYTHING),
		target_levelmask (EVERYTHING)
		{
		}

		bool isOK() const
		{
			return (target_descriptor && target_engine);
		}

		bool AcceptsEvent (EventDescriptor event) const
		{
			return (target_typemask & MASK (event.event_type)) &&
				   (target_levelmask & MASK (event.event_level));
		}

		void Close();
	};
	typedef const TargetDescriptor_& TargetDescriptor;

	// DEPRECATED wrapper for the constructor
	inline TargetDescriptor_ CreateTarget (const char* name, mask_t typemask, mask_t levelmask)
	{ return TargetDescriptor_ (name, typemask, levelmask); }


	// Static object information
	struct FXLIB_API ObjectDescriptor_
	{
		static const size_t GLOBAL_OID = -1;
		static size_t GetOID (const char* name) { return std::_Hash_impl::hash (name, strlen (name)); }

		size_t object_id; // hash of the name

		const char* object_name;
		ModuleType_ object_type;

		mutable EventLevelIndex_ maximum_accepted_level; /* everything higher is discarded - filters too verbose messages */
		mutable EventTypeIndex_ minimum_accepted_type; /* everything lower is discarded - filters non-critical messages */
		mutable mask_t type_flags;


		static const ObjectDescriptor_ default_object;
		static const ObjectDescriptor_ global_scope_object;



		// Creates a specific object descriptor
		ObjectDescriptor_ (const char* name, ModuleType_ type, const char* descriptor_name) :
		object_id (GetOID (descriptor_name)),
		object_name (name),
		object_type (type),
		maximum_accepted_level (E_UNDEFINED_VERBOSITY),
		minimum_accepted_type (E_UNDEFINED_TYPE),
		type_flags (default_object.type_flags)
		{
		}

		// Creates a default object descriptor
		ObjectDescriptor_ (size_t default_flags) :
		object_id (GLOBAL_OID),
		object_name ("undefined object"),
		object_type (MOD_INTERNAL),
		maximum_accepted_level (E_UNDEFINED_VERBOSITY),
		minimum_accepted_type (E_UNDEFINED_TYPE),
		type_flags (default_flags)
		{
		}

		// Creates a global object descriptor
		ObjectDescriptor_() :
		object_id (GLOBAL_OID),
		object_name ("global scope"),
		object_type (MOD_APPMODULE),
		maximum_accepted_level (E_UNDEFINED_VERBOSITY),
		minimum_accepted_type (E_UNDEFINED_TYPE),
		type_flags (0)
		{
		}

		bool IsGlobal() const
		{
			return object_id == GLOBAL_OID;
		}

		bool AcceptsEvent (EventDescriptor event) const
		{
			bool allowed = 1;

			EventLevelIndex_ effective_maxlevel = maximum_accepted_level;

			if (effective_maxlevel == E_UNDEFINED_VERBOSITY)
				effective_maxlevel = default_object.maximum_accepted_level;

			if (effective_maxlevel != E_UNDEFINED_VERBOSITY)
				allowed &= (event.event_level <= effective_maxlevel);

			EventTypeIndex_ effective_mintype = minimum_accepted_type;

			if (effective_mintype == E_UNDEFINED_TYPE)
				effective_mintype = default_object.minimum_accepted_type;

			if (effective_mintype != E_UNDEFINED_TYPE)
				allowed &= (event.event_type >= effective_mintype);

			return allowed;
		}

	};
	typedef const ObjectDescriptor_& ObjectDescriptor;


	// Changeable (dynamic) object data (incapsulates static descriptor)
	struct ObjectParameters_
	{
		const ObjectDescriptor_* object_descriptor;

		mutable ObjectState_ object_status;
		mutable char* error_string;

		mask_t flags;
		bool is_emergency_mode;



		ObjectParameters_ (const ObjectDescriptor_* descriptor) :
		object_descriptor (descriptor),
		object_status (OS_OK),
		error_string (0),
		flags (descriptor ? descriptor ->type_flags : 0),
		is_emergency_mode (0)
		{
		}

		ObjectParameters_ ToEmergency() const
		{
			if (!is_emergency_mode)
			{
				ObjectParameters_ emergency_object = *this;
				emergency_object.is_emergency_mode = 1;
				return emergency_object;
			}

			return *this;
		}
	};
	typedef const ObjectParameters_& ObjectParameters;


	// Macro-filled: event source position (file/line)
	struct SourceDescriptor_
	{
		const char* source_name;
		const char* function;
		unsigned source_line;

		SourceDescriptor_ (const char* filename, const char* funcname, unsigned line) :
		source_name (filename),
		function (funcname),
		source_line (line)
		{
		}
	};
	typedef const SourceDescriptor_& SourceDescriptor;


	struct LogAtom_
	{
		EventDescriptor event; // Event parameters (filled by user)
		SourceDescriptor place; // Source file data (filled by macro)
		ObjectParameters object; // Initiator object data (filled by macro from Debug::VerifierWrapper)
		TargetDescriptor target; // Logging target (filled by Debug::System)

		LogAtom_ (EventDescriptor event_, SourceDescriptor place_, ObjectParameters object_, TargetDescriptor target_) :
		event (event_),
		place (place_),
		object (object_),
		target (target_)
		{
		}

		void WriteOut();
	};
	typedef const LogAtom_& LogAtom;

	struct DumpEntity_
	{
		char id[4];
		ObjectDescriptor object;
		TargetDescriptor target;
	};
	typedef const DumpEntity_& DumpEntity;




	// ---------------------------------------
	// Classes
	// ---------------------------------------

	// The interface for the logger back-end.
	// Takes care of writing data into the destination file in proper format.
	class FXLIB_API LogEngine
	{
	public:
		virtual ~LogEngine() {};

		virtual void RegisterTarget (TargetDescriptor_* target)  = 0;
		virtual void CloseTarget (TargetDescriptor_* target) = 0;

		virtual void WriteLog (LogAtom atom) = 0;
		virtual void WriteLogEmergency (LogAtom atom) throw() = 0; // Targetless write in case of failures.
	};

	// The interface for the debug system back-end.
	// Interacts with language ABI and platform's runtime linker
	// for introspection.
	class FXLIB_API ABIEngine
	{
	};

	// Internal virtual base for Debug::VerifierWrapper.
	class FXLIB_API VerifierBase
	{
		// Specific object state.
		ObjectParameters_ dbg_params_;

		// This function must verify the object state and return 0 on any
		// error or inconsistency.
		virtual bool _Verify() const = 0;

		// Calls _Verify() to get new object state.
		// May throw exception if "fatal" flag is set.
		void _UpdateState (SourceDescriptor place) const;

	protected:
		// Should return the ctor logging fmt string.
		// Redefine if you want to say something specific for your module.
		// FORMAT parameters:
		// * Module name
		// * Object address
		virtual const char* _GetCtorFmt();

		// Should return the dtor logging fmt string.
		// Rules from GetCtorFmt_ apply.
		virtual const char* _GetDtorFmt();

		// Copies debug descriptor from "that" and sets its status to "moved"
		// to suppress destruction logging. Does not do the construction logging.
		void _MoveDynamicDbgInfo (const VerifierBase* that)
		{
			dbg_params_ = that ->dbg_params_;
			that ->dbg_params_.object_status = OS_MOVED;
		}

		// Checks and sets the debug descriptor on "first come" basis -
		// first fed descriptor is saved, others discarded.
		void _SetDynamicDbgInfo (const ObjectDescriptor_* info);

		VerifierBase() :
		dbg_params_ (Debug::ObjectParameters_ (0)) // stub
		{
		}

		virtual ~VerifierBase();

	public:

		// Checks object for errors and returns its actual state.
		inline ObjectParameters _GetDynamicDbgInfo (SourceDescriptor place) const
		{
			_UpdateState (place);
			return dbg_params_;
		}

		inline ObjectDescriptor _GetStaticDbgInfo() const
		{
			return *dbg_params_.object_descriptor;
		}

		// Returns whether object state is BAD.
		inline bool _CheckObject (SourceDescriptor place) const
		{
			_UpdateState (place);

			return ! ( (dbg_params_.object_status == OS_BAD) &&
					   (dbg_params_.flags & MASK (OF_USECHECK)));
		}

		// Returns whether object state is BAD.
		inline bool CheckObject() const
		{
			static const SourceDescriptor_ stat_desc = THIS_PLACE;
			return _CheckObject (stat_desc);
		}

		inline void _SetFlag (ObjectFlags_ flag) { dbg_params_.flags |=  MASK (flag); }
		inline void _ClrFlag (ObjectFlags_ flag) { dbg_params_.flags &= ~MASK (flag); }
	};

	class FXLIB_API _VerifierBase_Deprecated : public VerifierBase
	{
	protected:
		virtual bool _Verify() const { return 1; }
	};

	// We need InfoHolderType to be a functor returning "const ObjectDescriptor_*".
	template <typename InfoHolderType>
	class VerifierWrapper : virtual public _VerifierBase_Deprecated
	{
	protected:
		static const ObjectDescriptor_* _specific_dbg_info; /* API pointer */

	private:
		static void _SetStaticDbgInfo()
		{
			// Initialize the static data if it hasn't been done already (once per type).
			if (_specific_dbg_info ->IsGlobal())
				_specific_dbg_info = InfoHolderType() ();
		}

	protected:

		// Constructor used to signal object move
		VerifierWrapper (const VerifierBase* src)
		{
			_MoveDynamicDbgInfo (src);
		}

		VerifierWrapper()
		{
			_SetStaticDbgInfo();
			_SetDynamicDbgInfo (_specific_dbg_info);
		}

		virtual ~VerifierWrapper();
	};

	// ---------------------------------------
	// The mess ends here
	// ---------------------------------------


	// Library-wide exception class. Uses logging system to log throws.
	class FXLIB_API Exception : public std::exception
	{
		ExceptionType_ type_;
		const char* expression_;
		char* a_reason_, *a_message_, *a_what_message_;
		const char* reason_, *message_, *what_message_;

	public:
		Exception (const Exception&) = delete;
		Exception& operator= (const Exception&) = delete;

		// Move semantics
		Exception (Exception&& that);

		Exception (Debug::SourceDescriptor place,
				   Debug::ObjectParameters object,
				   Debug::ExceptionType_ type,
				   const char* expression,
				   const char* fmt,
				   va_list args);
		virtual ~Exception() throw();

		virtual const char* what() const throw();
	};

	class FXLIB_API SilentException : public std::exception
	{
		size_t error_code_;
		char* a_code_;

	public:
		SilentException (const SilentException&) = delete;
		SilentException& operator= (const SilentException&) = delete;

		SilentException (SilentException&& that);
		SilentException (Debug::ObjectParameters object, Debug::SourceDescriptor place, size_t error_code);
		virtual ~SilentException() throw();

		virtual const char* what() const throw();
	};

	// The main user (programmer) interface to the debug & logging system.
	DeclareDescriptor (System);
	class FXLIB_API System : LogBase (System), public Init::Base<System>
	{
		TargetDescriptor_ default_target;
		TargetDescriptor_ emergency_target;

		enum DbgSystemState
		{
			S_UNINITIALIZED = 0,
			S_READY,
			S_FATAL_ERROR
		} state;

		// Handles a logging error dependent on circumstances (criticalness, emergency mode)
		void HandleError (EventDescriptor event,
						  SourceDescriptor place,
						  ObjectParameters object,
						  const char* error_string) throw();

		void FatalErrorWrite (EventDescriptor event, SourceDescriptor place, ObjectParameters object);

	public:
		System();
		~System();

		void DoLogging (EventDescriptor event,
						SourceDescriptor place,
						ObjectParameters object);

		void SetTargetProperties (TargetDescriptor target,
								  LogEngine* logger); // Adds default target

		void CloseTargets(); // Removes all targets and puts system into uninitialized state
	};

	namespace API
	{
		template <typename T>
		inline const char* GetClassName (const T* object)
		{
			if (!object)
				return "NULL pointer";

			if (const VerifierBase* baseptr = dynamic_cast<const VerifierBase*> (object))
				return baseptr ->_GetStaticDbgInfo().object_name;

			return typeid (*object).name();
		}

		inline size_t GetObjectID (const VerifierBase* object)
		{
			if (!object)
				return ObjectDescriptor_::GLOBAL_OID;

			return object ->_GetStaticDbgInfo().object_id;
		}

		inline void SetObjectFlag (VerifierBase* obj, ObjectFlags_ flag) { obj ->_SetFlag (flag); }
		inline void ClrObjectFlag (VerifierBase* obj, ObjectFlags_ flag) { obj ->_ClrFlag (flag); }

		inline void SetDefaultVerbosity (EventLevelIndex_ max) { ObjectDescriptor_::default_object.maximum_accepted_level = max; }
		inline void SetDefaultEvtFilter (EventTypeIndex_ min)  { ObjectDescriptor_::default_object.minimum_accepted_type = min; }

		FXLIB_API const ObjectDescriptor_* RegisterMetaType (Debug::ObjectDescriptor_ type);

		FXLIB_API void SetTypewideVerbosity (const char* desc_name, EventLevelIndex_ max);
		FXLIB_API void SetTypewideEvtFilter (const char* desc_name, EventTypeIndex_ min);
		FXLIB_API void SetTypewideFlag (const char* desc_name, ObjectFlags_ flag);
		FXLIB_API void ClrTypewideFlag (const char* desc_name, ObjectFlags_ flag);
	}

	// ---------------------------------------
	// VerifierWrapper<_information> implementation
	// ---------------------------------------

	template <typename InfoHolderType>
	const ObjectDescriptor_* VerifierWrapper<InfoHolderType>::_specific_dbg_info = &ObjectDescriptor_::default_object;

	template <typename InfoHolderType>
	VerifierWrapper<InfoHolderType>::~VerifierWrapper() = default;

} // namespace Debug

using Debug::ModuleType_;
using Debug::EventTypeIndex_;
using Debug::EventLevelIndex_;

// -----------------------------------------------------------------------------
// ---- Entry points
// -----------------------------------------------------------------------------

// Debuggging information for the global scope (initialized with Debug::ObjectDescriptor_::global_scope_object)
FXLIB_API extern const Debug::ObjectDescriptor_* _specific_dbg_info;

// Silent exception entry point
FXLIB_API int dosilentthrow (Debug::ObjectParameters object,
							 size_t error_code);

// Exception entry point
FXLIB_API int dothrow (Debug::ObjectParameters object,
					   Debug::SourceDescriptor place,
					   Debug::ExceptionType_ ex_type,
					   const char* expr,
					   const char* fmt, ...);

// Logger entry point
FXLIB_API int call_log (Debug::ObjectParameters object,
						Debug::SourceDescriptor place,
						Debug::EventTypeIndex_ event_type,
						Debug::EventLevelIndex_ event_level,
						const char* fmt, ...);

// Verification failure handler
FXLIB_API int seterror (Debug::ObjectParameters object,
						const char* fmt, ...);

// -----------------------------------------------------------------------------
// ---- Assertions
// -----------------------------------------------------------------------------
// __assert is for verifying system-dependent conditions (as pointers),
// __verify is for checking user-dependent conditions (as input file names and expressions).

#ifdef NDEBUG
# define _CKLEVEL_(level) if (EventLevelIndex_::level > EventLevelIndex_::E_VERBOSE) break;
#else
# define _CKLEVEL_(level)
#endif

#define __silent(sta, code)  do { if ((sta)) break; Debug::SourceDescriptor_ pl = THIS_PLACE; dosilentthrow (_GetDynamicDbgInfo (pl), code); } while (0)
#define __ssilent(sta, code) do { if ((sta)) break; dosilentthrow (Debug::ObjectParameters_ (_specific_dbg_info), code); } while (0)

#define __sassert(sta, fmt...) do { if ((sta)) break; dothrow (Debug::ObjectParameters_ (_specific_dbg_info), THIS_PLACE, Debug::EX_BUG, #sta, fmt); } while (0)
#define __sverify(sta, fmt...) do { if ((sta)) break; dothrow (Debug::ObjectParameters_ (_specific_dbg_info), THIS_PLACE, Debug::EX_INPUT, #sta, fmt); } while (0)
#define __sasshole(fmt...)     do {                   dothrow (Debug::ObjectParameters_ (_specific_dbg_info), THIS_PLACE, Debug::EX_INPUT, "<none>", fmt); } while (0)

#define __assert(sta, fmt...) do { if ((sta)) break; Debug::SourceDescriptor_ pl = THIS_PLACE; dothrow (_GetDynamicDbgInfo (pl), pl, Debug::EX_BUG, #sta, fmt); } while (0)
#define __verify(sta, fmt...) do { if ((sta)) break; Debug::SourceDescriptor_ pl = THIS_PLACE; dothrow (_GetDynamicDbgInfo (pl), pl, Debug::EX_INPUT, #sta, fmt); } while (0)
#define __asshole(fmt...)     do {                   Debug::SourceDescriptor_ pl = THIS_PLACE; dothrow (_GetDynamicDbgInfo (pl), pl, Debug::EX_INPUT, "<none>", fmt); } while (0)

#define smsg(type, level, ...) do { _CKLEVEL_(level); Debug::ObjectParameters_ pm (_specific_dbg_info); call_log (pm,                      THIS_PLACE, EventTypeIndex_::type, EventLevelIndex_::level, __VA_ARGS__); } while (0)
#define msg(type, level, ...)  do { _CKLEVEL_(level); Debug::SourceDescriptor_ pl = THIS_PLACE;         call_log (_GetDynamicDbgInfo (pl), pl,         EventTypeIndex_::type, EventLevelIndex_::level, __VA_ARGS__); } while (0)

#define verify_statement(sta, fmt...) if (!(sta)) { seterror (_GetDynamicDbgInfo (THIS_PLACE), fmt); return 0; }

#ifndef NDEBUG
# define verify_method __verify (_CheckObject (THIS_PLACE), "In-method object verification failed")
#else
# define verify_method
#endif

#define verify_foreign(x) __verify ((x)._CheckObject (THIS_PLACE), "External object verification failed")

#define sverify_statement(sta, fmt...) if (!(sta)) { smsg (E_CRITICAL, E_USER, fmt); return 0; }

#define move_ctor VerifierWrapper (&that)
#define move_op Debug::VerifierBase::_MoveDynamicDbgInfo (&that)


// -----------------------------------------------------------------------------
// ---- Verifier implementation
// -----------------------------------------------------------------------------

#endif // _FXASSERT_H_

// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;


