#ifndef _SERVERAPP_H
#define _SERVERAPP_H

#include "build.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			InputServerApp.h
// Author		intelfx
// Description	Queue application - server header
// -----------------------------------------------------------------------------

DeclareDescriptor (IReader);
DeclareDescriptor (ITransmitter);

struct Buffer;
class IReader;
class ITransmitter;

const char* reader_init_func = "initreader";
typedef IReader*(*reader_func_proto)();

const char* transmitter_init_func = "inittransmitter";
typedef ITransmitter*(*transmitter_func_proto)();

class IReader : LogBase (IReader)
{
public:
	virtual void Set (const char* parameter) = 0;

	/*
	 * returns dest == 0 or length == 0 in case of stopped sequence
	 */
	virtual void Read (void** dest, size_t* length) = 0;
	virtual size_t Type() = 0;
};

class ITransmitter : LogBase (ITransmitter)
{
public:
	virtual void Set (const char* parameter) = 0;
	virtual void Connect() = 0;
	virtual void Disconnect() = 0;
	virtual bool ConnStatus() = 0;

	virtual void Tx (const Buffer* data) = 0;
};

#endif // _SERVERAPP_H