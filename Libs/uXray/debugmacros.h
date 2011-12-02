#ifndef _DEBUGMACROS_H
#define _DEBUGMACROS_H


// -----------------------------------------------------------------------------
// Library		FXLib
// File			debugmacros.h
// Author		intelfx
// Description	Macros for declaring/implementing debug system descriptors.
// -----------------------------------------------------------------------------

#if defined(TARGET_POSIX)
# define DBC_VISIBILITY __attribute__ ((visibility("default")))
#elif defined(TARGET_WINDOWS)
# warning "Debug class exporting for WINDOWS DL: implement me"
#endif

#define _ObjectDescName(var_name) _ ## var_name ## _dbginfo

#define DoExternBaseClass(var_name) extern template class DBC_VISIBILITY ::Debug::BaseClass<_ObjectDescName(var_name)>
#define DoExternDescriptor(var_name) struct DBC_VISIBILITY _ObjectDescName(var_name) 	\
	{																					\
	static const ::Debug::ObjectDescriptor_ desc;										\
	const ::Debug::ObjectDescriptor_* operator()() { return &desc;}						\
	}


#define DoImplementBaseClass(var_name) template class ::Debug::BaseClass<_ObjectDescName(var_name)>

#define DoImplementDescriptor(var_name, object_name, object_type, class_name)			\
	const Debug::ObjectDescriptor_ _ObjectDescName(var_name)::desc = ::Debug::CreateObject (object_name, object_type, &typeid (class_name))

#define DoImplementDescriptorNoid(var_name, object_name, object_type)					\
	const Debug::ObjectDescriptor_ _ObjectDescName(var_name)::desc = ::Debug::CreateObject (object_name, object_type, 0)

// Derives the logged class from base helper
#define LogBase(var_name) public ::Debug::BaseClass<_ObjectDescName(var_name)>

// Descriptor pre-declaration
#define DeclareDescriptor(var_name)														\
DoExternDescriptor(var_name);															\
DoExternBaseClass(var_name)

// Descriptor definition
#define ImplementDescriptor(var_name, object_name, object_type, class_name)				\
DoImplementDescriptor(var_name, object_name, ModuleType_::object_type, class_name);	\
DoImplementBaseClass(var_name)

#define ImplementDescriptorNoid(var_name, object_name, object_type)						\
DoImplementDescriptorNoid(var_name, object_name, ModuleType_::object_type);				\
DoImplementBaseClass(var_name)

// -----------------------------------------------------------------------------
// Just some helper macros
// -----------------------------------------------------------------------------

#if defined(VERBOSE_FUNCTION)
# define __FXFUNC__ __PRETTY_FUNCTION__
#else
# define __FXFUNC__ __func__
#endif

#define THIS_PLACE Debug::CreatePlace (__FILE__, __FXFUNC__, __LINE__)

// -----------------------------------------------------------------------------
// End
// -----------------------------------------------------------------------------

#endif // _DEBUGMACROS_H