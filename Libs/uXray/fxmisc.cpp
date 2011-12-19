#include "stdafx.h"
#include "fxmisc.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxmisc.cpp
// Author		intelfx
// Description	Miscellaneous functions
// -----------------------------------------------------------------------------

char* multibyte (wchar_t* wstr)
{
	static char output_buffer[STATIC_LENGTH];
	wcstombs (output_buffer, wstr, STATIC_LENGTH);

	return output_buffer;
}


FXLIB_API int fsize (FILE* file)
{
	fseek (file, 0, SEEK_END);
	int size = ftell (file);
	fseek (file, 0, SEEK_SET);
	return size;
}

FXLIB_API void strsub (char* dest, const char* src, size_t length)
{
	strncpy (dest, src, length);
	dest[length] = 0;
}

FXLIB_API void strcncat (char* dest, const char src, size_t length)
{
	for (size_t i = 0; (*dest) && (i < length - 2); ++dest);

	*dest++ = src;
	*dest = 0;
}

FXLIB_API void fxreplace (char*& src, const char* what, const char* with)
{
	__sassert (src, "Invalid pointer");

	char* found = strstr (src, what);
	if (!found)
		return;

	int src_len = strlen (src);
	int what_len = strlen (what);
	int with_len = strlen (with);
	int shift = with_len / what_len + 1;

	char* dest = new char[src_len * shift + 1];
	char* cur_src = src;
	char* cur_dest = dest;

	while (found)
	{
		const char* temp_rer = with;
		int skipped = found - cur_src;

		memmove (cur_dest, cur_src, skipped); // Copy normal text
		cur_dest += skipped;

		found += what_len; // Skip "what"
		while ((*cur_dest++ = *temp_rer++)); // Copy "with"

		cur_src = found;
		found = strstr (found, what);
	}

		strcpy (cur_dest, cur_src);
		delete[] src;
		src = dest;
}

FXLIB_API unsigned hashstr (const char* source_, unsigned source_len_ /*= 0*/)
{
	const unsigned base = 101;
	unsigned hash = 0;

	if (!source_len_)
		source_len_ = strlen (source_);

	for (unsigned i = 0; i < source_len_; ++i)
	{
		if (!source_[i])
			return 0;

		unsigned add_h = unsigned (source_[i]) *
				unsigned (pow (double (base), double (source_len_ - i - 1)));

		smsg (E_INFO, E_DEBUG, "Applying value %d from char %c pos %d\n", add_h, source_[i], i);
		hash += add_h;
	}

	return hash;
}

FXLIB_API unsigned rehashstr (unsigned hash_, const char* source_, unsigned len_)
{
	const unsigned base = 101;
	if (!source_[len_])
		return 0;

	return (hash_ -
				unsigned (*source_) *
				unsigned (pow (double (base), double (len_ - 1)))) * base +
				unsigned (source_[len_]);
}

FXLIB_API const char* strnchr (const char* s, size_t len, char c)
{
	for (size_t pos = 0; s[pos] && pos < len; ++pos)
		if (s[pos] == c)
			return s + pos;

	return 0;
}
