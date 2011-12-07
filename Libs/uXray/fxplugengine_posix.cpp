#include "stdafx.h"
#include "fxmodarch.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxplugengine_posix.cpp
// Author		intelfx
// Description	Plugin system backend for POSIX-compliant systems (libdl)
// -----------------------------------------------------------------------------

#ifdef TARGET_POSIX
#include <dlfcn.h>

class FXLIB_API POSIXPluginEngine : public PluginEngine
{
	POSIXPluginEngine (const POSIXPluginEngine&)			= delete;
	POSIXPluginEngine& operator= (const POSIXPluginEngine&)	= delete;

	POSIXPluginEngine();
	virtual ~POSIXPluginEngine();

public:
	static POSIXPluginEngine instance;

	virtual void* GetExecutableHandle();

	virtual void* ReopenLibrary (const char* filename);
	virtual void* LoadLibrary (const char* filename);
	virtual void  FreeLibrary (const char* filename);
	virtual void* Lookup (const char* symbol, void* handle);
};

POSIXPluginEngine POSIXPluginEngine::instance;

POSIXPluginEngine::POSIXPluginEngine()
{
	PluginSystem::SetNativeEngine (this);
}

POSIXPluginEngine::~POSIXPluginEngine()
{
}

void* POSIXPluginEngine::GetExecutableHandle()
{
	dlerror();

	msg (E_INFO, E_DEBUG, "Accessing main executable via POSIX dynamic linker");
	void* handle = dlopen (0, RTLD_NOLOAD);

	__assert (handle, "Unable to retrieve main executable handle - \"%s\"", dlerror());
	return handle;
}

void* POSIXPluginEngine::ReopenLibrary (const char* filename)
{
	dlerror();

	msg (E_INFO, E_DEBUG, "Retrieving handle to shared object \"%s\"", filename);
	void* handle = dlopen (filename, RTLD_NOLOAD);

	if (!handle)
		msg (E_WARNING, E_DEBUG, "Shared object \"%s\" was not loaded", filename);

	return handle;
}

void* POSIXPluginEngine::LoadLibrary (const char* filename)
{
	dlerror();

	msg (E_INFO, E_DEBUG, "Loading shared object \"%s\"", filename);
	void* handle = dlopen (filename, RTLD_LAZY | RTLD_GLOBAL);

	__verify (handle, "Could not load shared object \"%s\" - %s", filename, dlerror());
	return handle;
}

void POSIXPluginEngine::FreeLibrary (const char* filename)
{
	msg (E_INFO, E_DEBUG, "Unloading shared object \"%s\"", filename);

	void* handle = dlopen (filename, RTLD_LAZY | RTLD_NOLOAD);
	__verify (handle, "Shared object \"%s\" was not loaded prior to unloading", filename);

	dlclose (handle);
}

void* POSIXPluginEngine::Lookup (const char* symbol, void* handle)
{
	dlerror();

	msg (E_INFO, E_DEBUG, "Looking up symbol \"%s\"", symbol);
	void* address = dlsym (handle, symbol);

	if (const char* error = dlerror())
	{
		msg (E_WARNING, E_DEBUG, "Could not look up \"%s\" - %s", symbol, error);
		address = 0;
	}

	return address;
}

#endif // TARGET_POSIX