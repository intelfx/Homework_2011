#ifndef _FXCRITICAL_H
#define _FXCRITICAL_H

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

inline void str_tolower (char* s)
{
	while (char& a = *s++)
		a = tolower (static_cast<int> (a));
}

#endif // _FXCRITICAL_H