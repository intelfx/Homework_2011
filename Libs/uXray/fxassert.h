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
// --- Allocate and sprintf
// -----------------------------------------------------------------------------

const unsigned STATIC_LENGTH = 1024;

FXLIB_API bool fx_vasprintf (char** dest, const char* fmt, va_list args);
inline bool fx_asprintf (char** dest, const char* fmt, ...)
{
	va_list args;
	va_start (args, fmt);

	bool result = fx_vasprintf (dest, fmt, args);

	va_end (args);
	return result;
}


// -----------------------------------------------------------------------------
// ---- Masks
// -----------------------------------------------------------------------------

typedef unsigned long mask_t;

static const mask_t EVERYTHING = ~0x0;
inline mask_t MASK (int x)
{
	return x ? (1 << (x - 1)) : 0;
}

// -----------------------------------------------------------------------------
// ---- RTTI-enabled generic wrapper
// -----------------------------------------------------------------------------

template <typename T>
class rtti_enabled : public T
{
public:
	rtti_enabled (T && object);
	virtual ~rtti_enabled();
};

template <typename T>
rtti_enabled<T>::rtti_enabled (T && object) : T (std::move (object)) {}

template <typename T>
rtti_enabled<T>::~rtti_enabled() = default;

// -----------------------------------------------------------------------------
// ---- Object control
// -----------------------------------------------------------------------------

namespace Init
{
	// Inherit from this class to provide your version of CreateInstance()
	template <typename T>
	class FXLIB_API ControllerBase
	{
		T* instance_;
		bool is_available_;

	public:

		ControllerBase() : instance_ (0), is_available_ (1) {}
		~ControllerBase() { ForceDelete(); }

		T& operator() () // Fuck the world, forgot second pair of brackets
		{
			if (!is_available_)
				throw std::logic_error ("Request for global object after its destruction");

			if (!instance_) instance_ = new T;
			return *instance_;
		}

		void ForceDelete() { delete instance_; instance_ = 0; is_available_ = 0; }
		bool IsAvailable() { return is_available_; }
	};

	template <typename T>
	struct FXLIB_API Base
	{
		static ControllerBase<T> Instance;
	};

	template <typename T>
	ControllerBase<T> Base<T>::Instance;
}

// -----------------------------------------------------------------------------
// ---- The mainline debugging system
// -----------------------------------------------------------------------------

/*
 * WARNING
 * Very nontrivial nuance on using BaseClass or anything related in the debug system:
 * Make sure your classes that derive from BaseClass<> (uses LogBase) have
 * EXPLICIT visibility-default (non-inline and attributes set) ctor/dtor!
 *
 * The DeclareDescriptor/ImplementDescriptor macros does not have their own access specifiers
 * (like FXLIB_API) because they can be used in whatever library and from whatever library.
 * And it seems that generated BaseClass fields "inherit" their visibility from descendant.
 *
 * TODO verify everything on MSVC, since we've probably gone into "unspecified behavior".
 */

namespace Debug
{
	struct ObjectDescriptor_;
}

FXLIB_API extern const Debug::ObjectDescriptor_* _specific_dbg_info;


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
		OF_FATALVERIFY = 1, // Whether we need to __assert() verification result
		OF_USEVERIFY = 2 // Whether to call Verify_() at all
	};


	// ---------------------------------------
	// Predefinitions
	// ---------------------------------------

	class FXLIB_API LogEngine;
	class FXLIB_API ABIEngine;
	class FXLIB_API _InsideBase;


	// ---------------------------------------
	// Structures
	// ---------------------------------------

	// This is created and maintained by debug system
	struct TargetDescriptor_
	{
		const char* target_name; // Name of write destination (file, stream etc.)
		void* target_descriptor;
		LogEngine* target_engine;

		mask_t target_typemask;
		mask_t target_levelmask;
	};
	typedef const TargetDescriptor_& TargetDescriptor;


	// User-passed: event classification and message
	struct EventDescriptor_
	{
		EventTypeIndex_ event_type;
		EventLevelIndex_ event_level;
		const char* message_format_string;
		mutable va_list message_args; // Yes, since v*printf() does not accept "const va_list"
	};
	typedef const EventDescriptor_& EventDescriptor;


	static const size_t GLOBAL_OID = -1; // object_id for global scope

	// Static object information
	struct ObjectDescriptor_
	{
		size_t object_id; // type_info::hash_code()

		// Name and type are redundant at first glance,
		// but we need them on non-dynamic logging (when no object is available)
		const char* object_name;
		ModuleType_ object_type;

		/*
		 * "mutable" is required here not so inevitably as in ObjectParameters_,
		 * but this seems to be the most fine way to place an all-modifiable parameter
		 * in otherwise fully constant descriptor structure.
		 */

		mutable EventLevelIndex_ maximum_accepted_level; /* everything higher is discarded - filters too verbose messages */
		mutable EventTypeIndex_ minimum_accepted_type; /* everything lower is discarded - filters non-critical messages */
		mutable size_t type_flags;

	};
	typedef const ObjectDescriptor_& ObjectDescriptor;

	// Changeable (dynamic) object data (incapsulates static descriptor)
	struct ObjectParameters_
	{
		const ObjectDescriptor_* object_descriptor;

		/*
		 * "mutable" is required here since error indicator data
		 * must be semantically available RW even on constant objects.
		 */

		mutable ObjectState_ object_status;
		mutable char* error_string;

		size_t flags;
		bool is_emergency_mode;
	};
	typedef const ObjectParameters_& ObjectParameters;


	// Macro-filled: event source position (file/line)
	struct SourceDescriptor_
	{
		const char* source_name;
		const char* function;
		unsigned source_line;
	};
	typedef const SourceDescriptor_& SourceDescriptor;


	struct LogAtom_
	{
		EventDescriptor event; // Event parameters (filled by user)
		SourceDescriptor place; // Source file data (filled by macro)
		ObjectParameters object; // Initiator object data (filled by macro from Debug::BaseClass)
		TargetDescriptor target; // Logging target (filled by Debug::System)
	};
	typedef const LogAtom_& LogAtom;

	struct DumpEntity_
	{
		char id[4];
		ObjectDescriptor object;
		TargetDescriptor target;
	};
	typedef const DumpEntity_& DumpEntity;

	FXLIB_API extern const ObjectDescriptor_ _default_dbg_info;

	// ---------------------------------------
	// C-style functions
	// ---------------------------------------

	inline TargetDescriptor_ CreateTarget (const char* name, mask_t typemask, mask_t levelmask)
	{
		TargetDescriptor_ target = {name, 0, 0, typemask, levelmask};
		return target;
	}

	inline bool TargetAcceptsEvent (EventDescriptor event, TargetDescriptor target)
	{
		return (MASK (event.event_type) & target.target_typemask) &&
			   (MASK (event.event_level) & target.target_levelmask);
	}

	inline bool CheckDynamicVerbosity (ObjectDescriptor object, EventDescriptor event)
	{
		bool allowed = 1;

		EventLevelIndex_ effective_maxlevel = object.maximum_accepted_level;

		if (effective_maxlevel == E_UNDEFINED_VERBOSITY)
			effective_maxlevel = _default_dbg_info.maximum_accepted_level;

		if (effective_maxlevel != E_UNDEFINED_VERBOSITY)
			allowed &= (event.event_level <= effective_maxlevel);

		EventTypeIndex_ effective_mintype = object.minimum_accepted_type;

		if (effective_mintype == E_UNDEFINED_TYPE)
			effective_mintype = _default_dbg_info.minimum_accepted_type;

		if (effective_mintype != E_UNDEFINED_TYPE)
			allowed &= (event.event_type >= effective_mintype);

		return allowed;
	}

	inline bool TargetIsOK (const TargetDescriptor_* target)
	{
		return target && (target ->target_descriptor && target ->target_engine);
	}

	inline SourceDescriptor_ CreatePlace (const char* filename, const char* function, unsigned line)
	{
		SourceDescriptor_ place = {filename, function, line};
		return place;
	}

	inline ObjectDescriptor_ CreateObject (const char* name, Debug::ModuleType_ type,
										   const std::type_info* info)
	{
		ObjectDescriptor_ object = {info ? info ->hash_code() : 0, name, type,
									/* these parameters aren't going to be used in real objects
									 * see _SetStaticDbgInfo()
									 *
									 * Expl.: until at least one object is created,
									 * static ObjectDescriptor_ in BaseClass<T> will be initialized by global one
									 * and will be possibly altered by type-wide API functions.
									 *
									 * On first object creation, name, type and ID are copied from this descriptor
									 * to static one in BaseClass<T>.
									 */
									E_UNDEFINED_VERBOSITY, E_UNDEFINED_TYPE, 0
								   };
		return object;
	}

	inline ObjectParameters_ CreateParameters (_InsideBase*, const ObjectDescriptor_* descriptor)
	{
		// descriptor (passed), initial status (OK), error string (NULL), errors are fatal (1), is emergency mode (0)
		ObjectParameters_ parameters = {descriptor, OS_OK, 0, descriptor ? descriptor ->type_flags : 0, 0};
		return parameters;
	}

	inline ObjectParameters_ CreateEmergencyObject (ObjectParameters object)
	{
		ObjectParameters_ emergency_object = object;
		emergency_object.is_emergency_mode = 1;
		return emergency_object;
	}

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

	// ---------------------------------------
	// These classes are mess. Don't touch!!!
	// ---------------------------------------

	// Internal virtual base for Debug::BaseClass.
	class FXLIB_API _InsideBase
	{
		// For assertions and logging facility. Dynamically filled at runtime.
		// Designed as aid from multiple inheritance.
		const ObjectDescriptor_* dbg_info_;

		// Specific object state.
		ObjectParameters_ dbg_params_;

		// This function must verify the object state and return 0 on any
		// error or inconsistency.
		virtual bool _Verify() const = 0;

		inline void _VerifyAndSetState (SourceDescriptor place) const; // defined after everything

	protected:
		// Should return the ctor logging fmt string.
		// Redefine if you want to say something specific for your module.
		// FORMAT parameters:
		// * Module name
		// * Object address
		virtual const char* GetCtorFmt_();

		// Should return the dtor logging fmt string.
		// Rules from GetCtorFmt_ apply.
		virtual const char* GetDtorFmt_();

		// Copies debug descriptor from "that" and sets its status to "moved"
		// to suppress destruction logging. Does not do the construction logging.
		void _MoveDynamicDbgInfo (const _InsideBase* that);

		// Checks and sets the debug descriptor on "first come" basis -
		// first fed descriptor is saved, others discarded.
		void _SetDynamicDbgInfo (const ObjectDescriptor_* info);

	public:
		_InsideBase(); // defined after everything in "fxassert.h"
		virtual ~_InsideBase();

		// Checks object for errors and returns its actual state.
		inline ObjectParameters _GetDynamicDbgInfo (SourceDescriptor place) const
		{
			_VerifyAndSetState (place);
			return dbg_params_;
		}

		// Returns whether object state is BAD.
		inline bool CheckObject (SourceDescriptor place) const
		{
			_VerifyAndSetState (place);

			return ! ( (dbg_params_.object_status == OS_BAD) &&
					   (dbg_params_.flags & MASK (OF_FATALVERIFY)));
		}

		// Returns whether object state is BAD.
		inline bool CheckObject() const
		{
			static const SourceDescriptor_ stat_desc = THIS_PLACE;
			return CheckObject (stat_desc);
		}

		inline void _SetFlag (ObjectFlags_ flag) { dbg_params_.flags |=  MASK (flag); }
		inline void _ClrFlag (ObjectFlags_ flag) { dbg_params_.flags &= ~MASK (flag); }
	};

	class FXLIB_API _InsideBase_DefaultVerify : public _InsideBase
	{
	protected:
		virtual bool _Verify() const { return 1; }
	};

	// We need InfoHolderType to be a functor returning "const ObjectDescriptor_*".
	template <typename InfoHolderType>
	class BaseClass : virtual public _InsideBase_DefaultVerify
	{
		static ObjectDescriptor_ type_dbginfo;

	public:
		static const ObjectDescriptor_* const _specific_dbg_info; /* API pointer */

		static void _SetStaticDbgInfo()
		{
			// Initialize the static data if it hasn't been done already (once per type).
			if (type_dbginfo.object_id == GLOBAL_OID)
			{
				const ObjectDescriptor_* canonical_dbginfo = InfoHolderType() ();

				type_dbginfo.object_id = canonical_dbginfo ->object_id;
				type_dbginfo.object_name = canonical_dbginfo ->object_name;
				type_dbginfo.object_type = canonical_dbginfo ->object_type;

				/*
				 * We do not assign type flags and debug settings since they may
				 * have been altered by static type-wide functions in Debug::API
				 */
			}
		}

	protected:

		// Constructor used to signal object move
		BaseClass (const _InsideBase* src)
		{
			_MoveDynamicDbgInfo (src);
		}

		BaseClass()
		{
			_SetStaticDbgInfo();
			_SetDynamicDbgInfo (_specific_dbg_info);
		}

		virtual ~BaseClass();
	};

	// ---------------------------------------
	// The mess ends here
	// ---------------------------------------


	// Library-wide exception class. Uses logging system to log throws.
	class FXLIB_API Exception : public std::exception
	{
		ExceptionType_ type_;
		const char* expression_;
		char* a_reason_, *a_message_;
		const char* reason_, *message_;

	public:
		Exception (const Exception&) = delete;
		Exception& operator= (const Exception&) = delete;

		// Move semantics
		Exception (Exception && that);

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

		SilentException (SilentException && that);
		SilentException (Debug::ObjectParameters object, Debug::SourceDescriptor place, size_t error_code);
		virtual ~SilentException() throw();

		virtual const char* what() const throw();
	};

	// The main user (programmer) interface to the debug & logging system.
	DeclareDescriptor (System);
	class FXLIB_API System : LogBase (System), public Init::Base<System>
	{
		TargetDescriptor_ default_target;

		enum DbgSystemState
		{
			S_UNINITIALIZED = 0,
			S_READY,
			S_FATALERROR
		} state;

		// Handles a logging error dependent on circumstances (criticalness, emergency mode)
		void HandleError (EventDescriptor event,
						  SourceDescriptor place,
						  ObjectParameters object,
						  const char* error_string) throw();

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


	inline void WriteOutAtom (LogAtom atom, bool emergency_mode)
	{
		if (emergency_mode)	atom.target.target_engine ->WriteLogEmergency (atom);
		else				atom.target.target_engine ->WriteLog (atom);
	}

	inline void CloseTarget (TargetDescriptor_* target)
	{
		target ->target_engine ->CloseTarget (target);
		target ->target_engine = 0;
		target ->target_descriptor = 0;
	}

	namespace API
	{
		template <typename T>
		inline const char* GetClassName (const T* object)
		{
			if (!object)
				return "NULL pointer";

			if (const _InsideBase* baseptr = dynamic_cast<const _InsideBase*> (object))
				return baseptr ->_GetDynamicDbgInfo (THIS_PLACE).object_descriptor ->object_name;

			return typeid (*object).name();
		}

		inline void SetObjectFlag (_InsideBase* obj, ObjectFlags_ flag) { obj ->_SetFlag (flag); }
		inline void ClrObjectFlag (_InsideBase* obj, ObjectFlags_ flag) { obj ->_ClrFlag (flag); }

		template <typename T> inline void SetStaticTypeVerbosity (EventLevelIndex_ max) { T::_specific_dbg_info ->maximum_accepted_level = max; }
		template <typename T> inline void SetStaticTypeEvtFilter (EventTypeIndex_ min)  { T::_specific_dbg_info ->minimum_accepted_type = min; }

		template<> inline void SetStaticTypeVerbosity<void> (EventLevelIndex_ max)      { ::_specific_dbg_info ->maximum_accepted_level = max; }
		template<> inline void SetStaticTypeEvtFilter<void> (EventTypeIndex_ min)       { ::_specific_dbg_info ->minimum_accepted_type = min; }

		template <typename T> inline void SetStaticTypeFlag (ObjectFlags_ flag)         { T::_specific_dbg_info ->type_flags |=  MASK (flag); }
		template <typename T> inline void ClrStaticTypeFlag (ObjectFlags_ flag)         { T::_specific_dbg_info ->type_flags &= ~MASK (flag); }

		inline void SetDefaultVerbosity (EventLevelIndex_ max) { _default_dbg_info.maximum_accepted_level = max; }
		inline void SetDefaultEvtFilter (EventTypeIndex_ min)  { _default_dbg_info.minimum_accepted_type = min; }
	}

	// ---------------------------------------
	// BaseClass<_information> implementation
	// ---------------------------------------

	template <typename InfoHolderType>
	ObjectDescriptor_ BaseClass<InfoHolderType>::type_dbginfo = _default_dbg_info;

	template <typename InfoHolderType>
	const ObjectDescriptor_* const BaseClass<InfoHolderType>::_specific_dbg_info = &type_dbginfo;

	template <typename InfoHolderType>
	BaseClass<InfoHolderType>::~BaseClass() = default;

} // namespace Debug

using Debug::ModuleType_;
using Debug::EventTypeIndex_;
using Debug::EventLevelIndex_;

// -----------------------------------------------------------------------------
// ---- Entry points
// -----------------------------------------------------------------------------

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

#define __silent(sta, code) ((sta) || dosilentthrow (_GetDynamicDbgInfo (THIS_PLACE), THIS_PLACE))
#define __ssilent(sta, code) ((sta) || dosilentthrow (Debug::CreateParameters (0, _specific_dbg_info))

#define __sassert(sta, fmt...) ((sta) || dothrow (Debug::CreateParameters (0, _specific_dbg_info), THIS_PLACE, Debug::EX_BUG, #sta, fmt))
#define __sverify(sta, fmt...) ((sta) || dothrow (Debug::CreateParameters (0, _specific_dbg_info), THIS_PLACE, Debug::EX_INPUT, #sta, fmt))
#define __sasshole(fmt...)				dothrow (Debug::CreateParameters (0, _specific_dbg_info), THIS_PLACE, Debug::EX_BUG, "code flow", fmt)

#define __assert(sta, fmt...) ((sta) || dothrow (_GetDynamicDbgInfo (THIS_PLACE), THIS_PLACE, Debug::EX_BUG, #sta, fmt))
#define __verify(sta, fmt...) ((sta) || dothrow (_GetDynamicDbgInfo (THIS_PLACE), THIS_PLACE, Debug::EX_INPUT, #sta, fmt))
#define __asshole(fmt...)				dothrow (_GetDynamicDbgInfo (THIS_PLACE), THIS_PLACE, Debug::EX_BUG, "code flow", fmt)

#define smsg(type, level, ...) call_log (Debug::CreateParameters (0, _specific_dbg_info), THIS_PLACE, EventTypeIndex_::type, EventLevelIndex_::level, __VA_ARGS__)
#define msg(type, level, ...) call_log (_GetDynamicDbgInfo (THIS_PLACE), THIS_PLACE, EventTypeIndex_::type, EventLevelIndex_::level, __VA_ARGS__)

#define verify_statement(sta, fmt...) if (!(sta)) { seterror (_GetDynamicDbgInfo (THIS_PLACE), fmt); return 0; }
#define verify_method __verify (CheckObject (THIS_PLACE), "In-method object verification failed")
#define verify_foreign(x) __verify ((x).CheckObject(), "External object verification failed")

#define sverify_statement(sta, fmt...) if (!(sta)) { smsg (E_CRITICAL, E_USER, fmt); return 0; }

#define move_ctor BaseClass (&that)
#define move_op Debug::_InsideBase::_MoveDynamicDbgInfo (&that)


// -----------------------------------------------------------------------------
// ---- Verifier implementation
// -----------------------------------------------------------------------------

#ifndef NDEBUG

inline void Debug::_InsideBase::_VerifyAndSetState (Debug::SourceDescriptor place) const
{
	// eliminate successive _Verify() calls in case of bad object to reduce log clutter
	if ((dbg_params_.flags & MASK (OF_USEVERIFY)) && (dbg_params_.object_status != Debug::OS_BAD))
	{
		dbg_params_.object_status = Debug::OS_BAD; // eliminate recursive calls from verify_statement statements

		if (this ->_Verify())
			dbg_params_.object_status = Debug::OS_OK;

		else
			call_log (dbg_params_, place, Debug::E_CRITICAL, Debug::E_USER,
					  "Verification error: %s", dbg_params_.error_string);
	}
	dbg_params_.object_status = OS_UNCHECKED;
}

#else

inline void Debug::_InsideBase::_VerifyAndSetState (Debug::SourceDescriptor) const
{
	dbg_params_.object_status = OS_UNCHECKED;
}

#endif // NDEBUG

#endif // _FXASSERT_H_

// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;


