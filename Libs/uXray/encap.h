#ifndef _ENCAP_H
#define _ENCAP_H

#include "build.h"

#include "fxassert.h"

// -----------------------------------------------------------------------------
// Library		uXray
// File			encap.h
// Author		intelfx
// Description	Encapsulation classes for various fundamental types
// -----------------------------------------------------------------------------

#define ENCAP_DECLARATION(Type, T)														\
DeclareDescriptor(Type);																\
class FXLIB_API Type : LogBase(Type)													\
{																						\
	T data_;																			\
																						\
public:																					\
	Type (T data);																		\
	virtual ~Type();																	\
																						\
	operator T() const { return data_; }												\
	operator T&() { return data_; }														\
}

ENCAP_DECLARATION (Long, long);
ENCAP_DECLARATION (Int, int);
ENCAP_DECLARATION (Short, short);
ENCAP_DECLARATION (Char, char);
ENCAP_DECLARATION (Bool, bool);
ENCAP_DECLARATION (Double, double);
ENCAP_DECLARATION (SizeT, size_t);
ENCAP_DECLARATION (CStr, const char*);

#endif // _ENCAP_H