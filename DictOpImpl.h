#ifndef _DICT_OP_IMPL_H
#define _DICT_OP_IMPL_H

#include "build.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			DictOpImpl.h
// Author		intelfx
// Description	Dictionary normalisations implementation
// -----------------------------------------------------------------------------

// matches sequences:
// <word>'s -> <word>s
// <word>'  -> <word>
bool normalisation_apostrophes (std::wstring& str);

// matches sequences:
// <word>es -> <word>
// <word>s  -> <word>
bool normalisation_plural (std::wstring& str);

// matches sequences:
// <word>ed -> <word>
// <word>t  -> <word>
bool normalisation_participle (std::wstring& str);

// matches sequences:
// <word>ing -> <word>
bool normalisation_prparticiple (std::wstring& str);

#endif // _DICT_OP_IMPL_H