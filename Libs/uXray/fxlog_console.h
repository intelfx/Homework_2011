#ifndef _FXLOG_CONSOLE_H
#define _FXLOG_CONSOLE_H

#include "build.h"

#include "fxassert.h"
#include "pthread.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxlog_console.h
// Author		intelfx
// Description	Console (stdio) logger engine
// -----------------------------------------------------------------------------

DeclareDescriptor(FXConLog);
class FXLIB_API FXConLog : LogBase(FXConLog), public Debug::LogEngine, public Init::Base<FXConLog>
{
	pthread_mutex_t output_mutex;

	enum
	{
		EXT_NONE,
		EXT_ANSI
	} extended_mode;

	// Color code as used in ANSI SGR (Select Graphic Rendition) sequence
	enum ConsoleColorCode
	{
		CCC_BLACK = 0,
		CCC_RED = 1,
		CCC_GREEN = 2,
		CCC_YELLOW = 3,
		CCC_BLUE = 4,
		CCC_MAGENTA = 5,
		CCC_CYAN = 6,
		CCC_WHITE = 7,
		CCC_TRANSPARENT = 8 // My extension. It makes system to skip color set.
	};

	struct ConsoleExtendedData_
	{
		ConsoleColorCode foreground, background;
		bool brightness;
	};
	typedef const ConsoleExtendedData_& ConsoleExtendedData;

	void SetExtended (FILE* stream, ConsoleExtendedData data);
	void SetPosition (FILE* stream, unsigned short column, unsigned short row = 0);
	void ResetExtended (FILE* stream);

	FILE* OpenFile (const char* filename);
	void CloseFile (FILE* descriptor);
	static FILE* const FALLBACK_FILE;

	void InternalWrite (Debug::EventDescriptor event,
	                    Debug::SourceDescriptor place,
	                    Debug::ObjectParameters object,
	                    FILE* target);

public:
	// Microsoft are stupid motherfuckers.
	// They DROPPED the ANSI escape code support.
	// Replacing it with ioctls. Hate them.
	FXConLog (bool UseANSIEscapeSequences);
	FXConLog ();
	virtual ~FXConLog();

	virtual void RegisterTarget (Debug::TargetDescriptor_* target);
	virtual void CloseTarget (Debug::TargetDescriptor_* target);

	virtual void WriteLog (Debug::LogAtom atom);
	virtual void WriteLogEmergency (Debug::LogAtom atom) throw();
};

#endif // _FXLOG_CONSOLE_H