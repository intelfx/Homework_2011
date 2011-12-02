#ifndef _FXMISC_H_
#define _FXMISC_H_

#include "build.h"

#include "fxassert.h"
#include "fxcritical.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxmisc.h
// Author		intelfx
// Description	Miscellaneous functions
// -----------------------------------------------------------------------------

// -------------void setflag (unsigned& data, unsigned flags)-------------------
// Sets flags specified by 'flags' in 'data'.
// -----------------------------------------------------------------------------
inline void setflag (unsigned& data, unsigned flags)
{
	data |= flags;
}

// -----------------------------------------------------------------------------
// Clears flags specified by 'flags' in 'data'.
// -----------------------------------------------------------------------------
inline void clearflag (unsigned& data, unsigned flags)
{
	data &= ~flags;
}

// -----------------------------------------------------------------------------
// Checks if all flags specified by 'flags' are set in 'data'.
// -----------------------------------------------------------------------------
inline bool testflag (unsigned data, unsigned flags)
{
	return (data & flags) == flags;
}

// -----------------------------------------------------------------------------
// Same as strchr, but limiting length.
// -----------------------------------------------------------------------------
FXLIB_API const char* strnchr (const char* s, size_t len, char c);

// ----------void strcncat (char* dest, const char src, size_t length)----------
// -----------------------------------------------------------------------------
// Concatenates string dest with char src,
// while limiting resulting string's length (with terminating null byte).
// It replaces existing null byte with char, then sets the new terminating byte.
// If there's no space left for this - it replaces last meaningful byte
// with char.
// Example:
// -----------------------------------------------------------------------------
// 		0  1  2  3  4  5  6  7 | <- length = 8 or 7
// DEST	s  t  r  i  n  g \0  ? | <- src = '!'
// DEST	s  t  r  i  n  g  ! \0 | <- strcncat (dest, src, 8)
// DEST	s  t  r  i  n  ! \0  ? | <- strcncat (dest, src, 7)
// -----------------------------------------------------------------------------
FXLIB_API void strcncat (char* dest, const char src, size_t length);

// ----------void strsub (char* dest, const char* src, size_t length)-----------
// -----------------------------------------------------------------------------
// Works same as strncpy (char* dest, const char* src, size_t n),
// but sets the terminating null byte.
// It actually calls strncpy (dest, src, length); and then
// sets last byte of dest to 0.
// !!! dest must be <length + 1> length !!!
// Comparison:
// -----------------------------------------------------------------------------
// 		0  1  2  3  4  5  6  7  8  9  A  B | <- we'll copy first 6 bytes.
// SRC	s  t  r  i  n  g  s  t  r  i  n  g | <- source string
// DEST	s  t  r  i  n  g  ?  ?  ?  ?  ?  ? | <- strncpy (dest, src, 6)
// DEST s  t  r  i  n  g \0  ?  ?  ?  ?  ? | <- strsub  (dest, src, 6)
// -----------------------------------------------------------------------------
FXLIB_API void strsub (char* dest, const char* src, size_t length);

// -------------------------int fsize (FILE* file)------------------------------
// -----------------------------------------------------------------------------
// Returns size of file. The side effect is that the file is loaded into
// the buffer, which speeds up the following read, so this should be used before
// loading the file into somewhat buffer.
// -----------------------------------------------------------------------------
FXLIB_API int fsize (FILE* file);

// -------void fxreplace (char*& src, const char* what, const char* with)-------
// -----------------------------------------------------------------------------
// Replaces every occurrence of what to with in src.
// -----------------------------------------------------------------------------
FXLIB_API void fxreplace (char*& src,
						  const char* what,
						  const char* with);

// ------unsigned hashstr (const char* source_, unsigned source_len_ = 0)-------
// -----------------------------------------------------------------------------
// Creates hash of string. If source_len_ is omitted or zero, it is replaced
// by strlen (source_), taking whole string,
// otherwise only first source_len_ bytes are taken.
// New hash will be valid for[source_ .. source_ + len_ - 1].
// If a EOS occurs in that range, new hash will be 0.
// -----------------------------------------------------------------------------
FXLIB_API unsigned hashstr (const char* source_, unsigned source_len_ = 0);

// ---unsigned rehashstr (unsigned hash_, const char* source_, unsigned len_)---
// -----------------------------------------------------------------------------
// Re-calculates the hash of length len_ which begins at source_.
// New hash will be valid for[source_ + 1 .. source_ + len_].
// If a EOS occurs in that range, new hash will be 0.
// Example how to call (searching str2 in str1):
// -----------------------------------------------------------------------------
/*
unsigned hash1 = hashstr (src1, src2_len);
if (!hash1)
	return 0; // Source string is too short.
unsigned hash2 = hashstr (src2);
do
{
	if (hash1 == hash2)
		if (!strncmp (src1, src2, src2_len))
			return src1; // Found src2 at src1
} while (hash1 = rehashstr (hash1, src1++, src2_len))
return 0; // Not found src2
*/
// -----------------------------------------------------------------------------
FXLIB_API unsigned rehashstr (unsigned hash_, const char* source_, unsigned len_);

// ------------srctype_ castback<to, srctype> (srctype_ src)--------------------
// -----------------------------------------------------------------------------
// Casts an expression to given typename "to", then back to source type.
// -----------------------------------------------------------------------------
template <typename to, typename srctype_>
srctype_ castback (srctype_ src)
{
	return static_cast<srctype_> (static_cast<to> (src));
}

// ------------srctype_ losscheck<to, srctype> (srctype_ src)-------------------
// -----------------------------------------------------------------------------
// Checks if a conversion can be made without information loss.
// -----------------------------------------------------------------------------
/*
losscheck<int> (4.3); // Will report false, can't cast 4.3 to int without loss.
losscheck<int> (4.0); // Will report true, 4.0 can be casted to int losslessly.
*/
// -----------------------------------------------------------------------------
template <typename to, typename srctype_>
bool losscheck (srctype_ src)
{
	return castback<to> (src) == src;
}

// -----BaseT* rtti_<PassT, BaseT> (bool compare, const type_info* comp_ti)-----
// -----------------------------------------------------------------------------
// An instrument to pass a type name to some module.
// It can be used for instance creation as following:
// This function creates an instance of class "PassT" and returns a pointer
// to created object, upcasting it to type name "BaseT", provided that we
// pass 0 as its first parameter (as default).
// Creation is done by operator new.
// You need to define a base abstract class in the acceptor module and derive
// all passed types from that base class.
// You need to set up a parameter in acceptor module of type "BaseT*(*)()",
// where BaseT is the base class.
// Then you'll be able to pass a type name PassT using function pointer
// "&replicator<PassT, BaseT>" .
// Upcasting is done because it is needed for accepting module to know exact
// return type of given replicator function pointer. Also, the acceptor
// can work with created objects of unknown type only via virtual functions
// of base type.
// Also it can be used for type comparing as following:
// The function is really a shell to std::type_info::operator==() method,
// The passed type name is CompT. 1 should be passed as first parameter, and
// the type_info object to compare is passed as second parameter.
// It takes type_info instead of pointer to allow comparing with plain types -
// it can be useful to identify some structure by typename only.
// WARNING: This use of function is type-unsafe - it actually returns a boolean,
// not BaseT* !
// -----------------------------------------------------------------------------
template <typename PassT, typename BaseT>
BaseT* rtti_ (bool compare, const std::type_info* comp_ti)
{
	if (!compare)
		return dynamic_cast<BaseT*> (new PassT);

	else
		return reinterpret_cast<BaseT*> (typeid (PassT) == *comp_ti);
}

#endif // _FXMISC_H_
