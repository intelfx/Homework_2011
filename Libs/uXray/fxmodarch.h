#ifndef _FXPLUGARCH_H
#define _FXPLUGARCH_H

#include "build.h"

#include "fxassert.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxmodarch.h
// Author		intelfx
// Description	Plugin system
// -----------------------------------------------------------------------------

DeclareDescriptor(PluginSystem);
DeclareDescriptor(PluginEngine);
DeclareDescriptor(PluginSystem_Plugin);

// Cast to function pointer
template <typename fptr>
inline fptr Fcast (void* ptr)
{
	// Cast pointer-to-pointer-to-void to pointer-to-pointer-to-function.
	// Since they are all pointers to data, cast is apparently permitted by C99 and so forth.
	return *reinterpret_cast<fptr*> (&ptr);
}

// TODO lurk about PLT and windows' equivalent for transparent runtime linkage.

// The plugin engine - first abstraction layer.
// Provides system-independent low-level library operations.
class FXLIB_API PluginEngine : LogBase(PluginEngine)
{
public:
	PluginEngine() {}
	virtual ~PluginEngine();

	/* The function should return handle which allows lookup in main executable */
	virtual void* GetExecutableHandle() = 0;

	/* Must return only existing handles, NULL if not loaded */
	virtual void* ReopenLibrary (const char* filename) = 0;

	/* Successive calls for the same library should return the same handle */
	virtual void* LoadLibrary (const char* filename) = 0;

	/* Interface here may change: filename to unload is debatable */
	virtual void  FreeLibrary (const char* filename) = 0;

	virtual void* Lookup (const char* symbol, void* handle) = 0;
};

class FXLIB_API PluginSystem : LogBase(PluginSystem)
{
public:
	// The plugin object - second abstraction layer.
	// Handles load/unload and symbol lookup using the plugin engine.
	class Plugin : LogBase(PluginSystem_Plugin)
	{
		PluginEngine*	engine_; /* Engine used to create the plugin object */
		const char*		filename_; /* Not always filename, maybe soname or similar */
		void*			handle_;
		bool			is_primary_; /* Set to 1 if this object loaded its library, 0 if reopened */

	public:
		Plugin (const Plugin&)				= delete;
		Plugin& operator= (const Plugin&)	= delete;

		Plugin (const char* filename, PluginEngine* engine, bool reopen = 0);	// Load by-name
		Plugin();																// Load global handle
		~Plugin();

		Plugin (Plugin&& that);
		Plugin& operator= (Plugin&& that);

		bool IsValid() const
		{
			return (handle_);
		}

		const char* GetFilename() const
		{
			return filename_;
		}

		bool IsPrimary() const
		{
			return is_primary_;
		}

		bool operator== (const Plugin& that) const
		{
			return handle_ == that.handle_;
		}

		void* Lookup (const char* symbol);
	};

private:
	// Global instance of native plugin engine used by system.
	static PluginEngine* native_engine_;

	std::list<Plugin> plugins;

	// Inserts a newly created plugin into the list, checking for already inserted ones.
	std::list<Plugin>::iterator CheckInsertPluginObject (Plugin&& plugin);

	// Retrieve iterator for the requested plugin, maybe inserting it.
	std::list<Plugin>::iterator CheckGetPluginObject (Plugin&& plugin);

	// Remove plugin object which is equal to given
	void CheckRemovePluginObject (Plugin&& plugin);

public:

	PluginSystem();
	~PluginSystem();

	// Native engine is the exceptional case (yes, now it's the only case ;)
	static PluginEngine* GetNativeEngine();
	static void SetNativeEngine (PluginEngine* engine);

	Plugin* LoadPlugin (const char* filename); // Returns handle
	Plugin* ReferencePlugin (const char* filename); // Returns handle (to existing)

	void RemovePlugin (const char* filename); // Unload a library
	void RemovePlugin (Plugin* handle); // Unload a library (by handle)

	void* LookupSequential (const char* symbol); /* Lookup in all registered libraries */
	void* LookupGlobal (const char* symbol); /* Lookup in global space (implementation-defined!) */

	template <typename T>
	T* AttemptInitPlugin (Plugin* handle, const char* init_func); // Get symbol by name and call it as T*(*)(void)

	template <typename T>
	T* LoadInitPlugin (const char* filename, const char* init_func); // Load library, get symbol and call it as T*(*)(void)
};

template <typename T>
T* PluginSystem::AttemptInitPlugin (PluginSystem::Plugin* handle, const char* init_func)
{
	__assert (handle, "NULL handle");
	__assert (init_func, "NULL init function name pointer");

	void* symbol = handle ->Lookup (init_func);
	if (!symbol)
	{
		msg (E_WARNING, E_DEBUGLIB, "API init attempt: Unsupported API: \"%s\"", init_func);
		return 0;
	}

	T* interface = Fcast <T*(*)(void)> (symbol) ();
	if (!interface)
	{
		msg (E_CRITICAL, E_VERBOSE, "Plugin API call failed: \"%s\"", init_func);
	}

	return interface;
}

template <typename T>
T* PluginSystem::LoadInitPlugin (const char* filename, const char* init_func)
{
	__assert (filename, "NULL filename");
	__assert (init_func, "NULL init function name pointer");

	Plugin* handle = LoadPlugin (filename);
	void* symbol = handle ->Lookup (init_func);
	if (!symbol)
	{
		RemovePlugin (handle);
		__verify (0, "Invalid plugin API: [\"%s\"] \"%s\"", filename, init_func);
	}

	T* interface = Fcast<T*(*)(void)> (symbol) ();
	if (!interface)
	{
		RemovePlugin (handle);
		__verify (0, "Plugin API call failed: [\"%s\"] \"%s\"", filename, init_func);
	}

	return interface;
}



#endif // _FXPLUGARCH_H
