#include "stdafx.h"
#include "encap.h"

// -----------------------------------------------------------------------------
// Library		uXray
// File			encap.cpp
// Author		intelfx
// Description	Encapsulation classes for fundamental types - explicit instantiation
// -----------------------------------------------------------------------------

/*
 * This piece of shit deserves some special quote:
 *
 * Ich schleich' mich an
 * Und rede fein
 * Wer ficken will
 * Muss freundlich sein
 * -- Rammstein - Liebe Ist Fur Alle Da
 */

#define ENCAP_IMPLEMENTATION(Type, fund)												\
	ImplementDescriptor (Type, #fund, MOD_INTERNAL, Type);								\
	Type::Type (fund data) : data_ (data) {}											\
	Type::~Type() = default;

ENCAP_IMPLEMENTATION (Long, long);
ENCAP_IMPLEMENTATION (Int, int);
ENCAP_IMPLEMENTATION (Short, short);
ENCAP_IMPLEMENTATION (Char, char);
ENCAP_IMPLEMENTATION (Bool, bool);
ENCAP_IMPLEMENTATION (Double, double);
ENCAP_IMPLEMENTATION (SizeT, size_t);
ENCAP_IMPLEMENTATION (CStr, const char*);