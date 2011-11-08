#ifndef _SHAREDSCP_H
#define _SHAREDSCP_H

#include "build.h"

#include "Queue.h"

// -----------------------------------------------------------------------------
// Library		Antided
// File			SharedServerClientProtocol.h
// Author		intelfx
// Description	Shared part of the headers - common for both server and client applications.
// -----------------------------------------------------------------------------

struct Buffer // .....should comply to POD definition.....
{
	size_t length;
	size_t type;
	void* data;

	Buffer (const Buffer&) = delete;
	Buffer& operator= (const Buffer&) = delete;

	Buffer (void* data_, size_t length_, size_t type_) :
	length (length_), type (type_), data (data_) {}

	Buffer (Buffer&& that) :
	length (that.length), type (that.type), data (that.data) { that.data = 0; that.length = 0; that.type = 0; }

	Buffer& operator= (Buffer&& that)
	{
		if (this == &that)
			return *this;

		free (data);

		data = that.data;
		type = that.type;
		length = that.length;

		that.data = 0;
		that.type = 0;
		that.length = 0;

		return *this;
	}

	~Buffer() { free (data); }
} PACKED;


#endif // _SHAREDSCP_H