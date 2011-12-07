#include "stdafx.h"
#include "fxmodarch.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxmodarch.cpp
// Author		intelfx
// Description	Plugin system
// -----------------------------------------------------------------------------

PluginEngine::~PluginEngine()
{
}

PluginSystem::PluginSystem()
{
}

PluginSystem::~PluginSystem()
{
}

PluginEngine* PluginSystem::GetNativeEngine()
{
	__sassert (native_engine_, "Native engine is not registered - plugin system is probably not initialized yet!");
	return native_engine_;
}

void PluginSystem::SetNativeEngine (PluginEngine* engine)
{
	// This is gotta be quiet because it is called from global ctors.
	if (!native_engine_)
		native_engine_ = engine; // First come - first served, others fuck off
}

PluginSystem::Plugin::Plugin (const char* filename, PluginEngine* engine, bool reopen_only /* = 0 */) :
engine_		(engine),
filename_	(filename),
handle_		(engine_ ->ReopenLibrary (filename)),
is_primary_	(!handle_ && !reopen_only) // If reopen returned 0, then we're supposed to load (if allowed).
{
	if (is_primary_)
	{
		handle_ = engine_ ->LoadLibrary (filename);
		__assert (handle_, "Unable to load \"%s\" somewhy... Check upper log for engine messages",
		          filename);
	}
}

PluginSystem::Plugin::Plugin() :
engine_		(PluginSystem::GetNativeEngine()),
filename_	(0),
handle_		(engine_ ->GetExecutableHandle()),
is_primary_	(0)
{
	__assert (handle_, "Unable to load main handle somewhy... Check upper log for engine messages");
}


PluginSystem::Plugin::~Plugin()
{
	if (handle_ && is_primary_)
		engine_ ->FreeLibrary (filename_);
}

PluginSystem::Plugin::Plugin (PluginSystem::Plugin&& that) : move_ctor,
engine_		(that.engine_),
filename_	(that.filename_),
handle_		(that.handle_),
is_primary_	(that.is_primary_)
{
	that.engine_	= 0;
	that.filename_	= 0;
	that.handle_	= 0;
	that.is_primary_= 0;
}

PluginSystem::Plugin& PluginSystem::Plugin::operator= (PluginSystem::Plugin&& that)
{
	if (is_primary_ && handle_)
		engine_ ->FreeLibrary (filename_);

	filename_		= that.filename_;
	handle_			= that.handle_;
	is_primary_		= that.is_primary_;

	move_op;

	that.filename_	= 0;
	that.handle_	= 0;
	that.is_primary_= 0;

	return *this;
}

void* PluginSystem::Plugin::Lookup (const char* symbol)
{
	msg (E_INFO, E_DEBUG, "Looking up symbol \"%s\" in library \"%s\"", symbol, filename_);
	__assert (symbol, "NULL symbol name for lookup");
	return engine_ ->Lookup (symbol, handle_);
}

std::list<PluginSystem::Plugin>::iterator PluginSystem::CheckInsertPluginObject (PluginSystem::Plugin&& plugin)
{
	msg (E_INFO, E_DEBUG, "Inserting plugin \"%s\"", plugin.GetFilename());
	msg (E_INFO, E_DEBUG, "The given plugin object is of type %s",
		 plugin.IsPrimary() ? "primary" : "reference");

	for (auto i = plugins.begin(); i != plugins.end(); ++i)
		if (*i == plugin)
		{
			// If we have it already, the plugin object must not be primary.
			__assert (!plugin.IsPrimary(), "Found duplicate for a primary type plugin");

			msg (E_WARNING, E_VERBOSE,
				 "Plugin has not been inserted - already loaded as \"%s\"", i ->GetFilename());

			return i;
		}

	// If we need to insert the plugin object, it must be primary.
	__assert (plugin.IsPrimary(), "Got reference type object when there is no primary objects");

	plugins.push_front (std::move (plugin));
	auto inserted_plugin = plugins.begin();
	return inserted_plugin;
}

std::list<PluginSystem::Plugin>::iterator PluginSystem::CheckGetPluginObject (PluginSystem::Plugin&& plugin)
{
	msg (E_INFO, E_DEBUG, "Searching primary for plugin \"%s\"", plugin.GetFilename());
	msg (E_INFO, E_DEBUG, "The given plugin object is of type %s",
		 plugin.IsPrimary() ? "primary" : "reference");

	for (auto i = plugins.begin(); i != plugins.end(); ++i)
		if (*i == plugin)
		{
			// If we have it already, the plugin object must not be primary.
			__assert (!plugin.IsPrimary(), "Found duplicate for a primary type plugin");

			msg (E_INFO, E_DEBUG, "Plugin found as \"%s\"", i ->GetFilename());
			return i;
		}

	if (plugin.IsPrimary())
	{
		msg (E_WARNING, E_VERBOSE, "Requested plugin was not loaded - inserting new primary object \"%s\"",
			plugin.GetFilename());

		return CheckInsertPluginObject (std::move (plugin));
	}

	else
	{
		msg (E_WARNING, E_VERBOSE, "Requested plugin was not loaded, not inserting non-primary object");

		return plugins.end();
	}
}

void PluginSystem::CheckRemovePluginObject (PluginSystem::Plugin&& plugin)
{
	msg (E_INFO, E_DEBUG, "Checking and removing plugin \"%s\"", plugin.GetFilename());
	msg (E_INFO, E_DEBUG, "The given plugin object is of type %s",
		   plugin.IsPrimary() ? "primary" : "reference");

	for (auto i = plugins.begin(); i != plugins.end(); ++i)
		if (*i == plugin)
		{
			// If we have it already, the given plugin object must not be primary.
			__assert (!plugin.IsPrimary(), "Found duplicate for a primary type plugin");

			msg (E_INFO, E_VERBOSE, "Plugin loaded as \"%s\" is being removed", i ->GetFilename());

			plugins.erase (i);
			return;
		}

	// Not asserting primariness of object since you have every right to give "force-reopened" object here.
	msg (E_WARNING, E_VERBOSE, "Plugin \"%s\" was not inserted prior to removing!",
	     plugin.GetFilename());
}

PluginSystem::Plugin* PluginSystem::LoadPlugin (const char* filename)
{
	msg (E_INFO, E_VERBOSE, "Loading plugin from name \"%s\"", filename);
	return CheckInsertPluginObject (Plugin (filename, GetNativeEngine())).operator->(); // wtf
}

PluginSystem::Plugin* PluginSystem::ReferencePlugin (const char* filename)
{
	msg (E_INFO, E_DEBUG, "Returning existing handle to plugin \"%s\"", filename);
	auto ref_iter = CheckGetPluginObject (Plugin (filename, GetNativeEngine(), 1));

	if (ref_iter == plugins.end())
		return 0;

	else
	{
		__assert (ref_iter ->IsPrimary(), "Must not return non-primary objects");
		__assert (ref_iter ->IsValid(), "Must not return invalid objects");
		return ref_iter.operator->(); // wtf
	}
}

void PluginSystem::RemovePlugin (const char* filename)
{
	msg (E_INFO, E_VERBOSE, "Removing plugin with name \"%s\"", filename);

	// We're not going to load a library just to unload it, so force-reopen
	CheckRemovePluginObject (Plugin (filename, GetNativeEngine(), 1));
}

void PluginSystem::RemovePlugin (PluginSystem::Plugin* handle)
{
	msg (E_INFO, E_VERBOSE, "Removing plugin with name \"%s\" (by handle)", handle ->GetFilename());
	CheckRemovePluginObject (std::move (*handle));
}

void* PluginSystem::LookupGlobal (const char* symbol)
{
	msg (E_INFO, E_DEBUG, "Globally looking for symbol \"%s\"", symbol);

	Plugin global;
	void* address = global.Lookup (symbol);
	if (!address)
		msg (E_WARNING, E_DEBUG, "We got NULL results for symbol \"%s\"", symbol);

	return address;
}

void* PluginSystem::LookupSequential (const char* symbol)
{
	msg (E_INFO, E_DEBUG, "Sequentially looking for symbol \"%s\"", symbol);

	for (Plugin& plugin: plugins)
	{
		__assert (plugin.IsPrimary(), "In-list plugin object \"%s\" is not primary!", plugin.GetFilename());

		if (void* address = plugin.Lookup (symbol))
		{
			msg (E_INFO, E_DEBUG, "Found symbol \"%s\" in plugin \"%s\"", symbol, plugin.GetFilename());
			return address;
		}
	}

	msg (E_WARNING, E_DEBUG, "We got NULL results for symbol \"%s\"", symbol);
	return 0;
}

PluginEngine* PluginSystem::native_engine_ = 0;

ImplementDescriptor(PluginSystem, "dynamic plugin system", MOD_APPMODULE, PluginSystem);
ImplementDescriptor(PluginEngine, "dynamic plugin backend", MOD_APPMODULE, PluginEngine);
ImplementDescriptor(PluginSystem_Plugin, "plugin", MOD_OBJECT, PluginSystem::Plugin);
