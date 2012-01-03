#ifndef _FXCRITICAL_H
#define _FXCRITICAL_H

#include <ctype.h>
#include <wctype.h>

// -----------------------------------------------------------------------------
// Library		uXray
// File			fxcritical.h
// Author		intelfx
// Description	Some critical definitions.
// -----------------------------------------------------------------------------

// Array of C-strings -- just for usability
typedef const char* const* CStrArray;

template <typename T>
inline T min (const T& a, const T& b)
{
	return (a < b) ? a : b;
}

template <typename T>
inline T max (const T& a, const T& b)
{
	return (a > b) ? a : b;
}

inline void str_tolower (char* s, bool cut_newline = 1)
{
	while (char& a = *s++)
		a = tolower (static_cast<int> (a));

	if (cut_newline)
	{
		s -= 2;
		if (*s == '\n') *s = '\0';
	}
}

inline void wstr_tolower (wchar_t* s, bool cut_newline = 1)
{
	while (wchar_t& a = *s++)
		a = towlower (static_cast<int> (a));

	if (cut_newline)
	{
		s -= 2;
		if (*s == L'\n') *s = L'\0';
	}
}

#endif // _FXCRITICAL_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
