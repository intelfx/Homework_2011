#ifndef _DEBUGMACROS_H
#define _DEBUGMACROS_H


// -----------------------------------------------------------------------------
// Library		FXLib
// File			debugmacros.h
// Author		intelfx
// Description	Macros for declaring/implementing debug system descriptors.
// -----------------------------------------------------------------------------

#define _ObjectDescName(var_name) _ ## var_name ## _dbginfo
#define _ObjectVarName(var_name) _ ## var_name ## _dbgobject

#define DoExternBaseClass(var_name) extern template class  ::Debug::VerifierWrapper<_ObjectDescName(var_name)>
#define DoExternDescriptor(var_name) struct EXPORT _ObjectDescName(var_name)					\
	{																					\
	static const ::Debug::ObjectDescriptor_ desc;										\
	const ::Debug::ObjectDescriptor_* operator()() { return &desc; }						\
	}


#define DoImplementBaseClass(var_name) template class ::Debug::VerifierWrapper<_ObjectDescName(var_name)>

#define DoImplementDescriptor(var_name, object_name, object_type)						\
	const ::Debug::ObjectDescriptor_ _ObjectDescName(var_name)::desc (object_name, object_type, #var_name);

// Derives the logged class from base helper
#define LogBase(var_name) public ::Debug::VerifierWrapper<_ObjectDescName(var_name)>

// Descriptor pre-declaration
#define DeclareDescriptor(var_name)														\
DoExternDescriptor(var_name);															\
DoExternBaseClass(var_name)

// Descriptor definition

#define ImplementDescriptor(var_name, object_name, object_type)							\
DoImplementDescriptor(var_name, object_name, ::Debug::object_type);						\
DoImplementBaseClass(var_name)

// -----------------------------------------------------------------------------
// Just some helper macros
// -----------------------------------------------------------------------------

#if defined(VERBOSE_FUNCTION)
# define __FXFUNC__ __PRETTY_FUNCTION__
#else
# define __FXFUNC__ __func__
#endif

#define THIS_PLACE Debug::SourceDescriptor_ (__FILE__, __FXFUNC__, __LINE__)

// -----------------------------------------------------------------------------
// End
// -----------------------------------------------------------------------------

#endif // _DEBUGMACROS_H