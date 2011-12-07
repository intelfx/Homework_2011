#include "stdafx.h"
#include "InputServerApp.h"

#ifdef TARGET_POSIX
#include <termios.h>
#include <unistd.h>
#endif

// -----------------------------------------------------------------------------
// Library		Homework
// File			KeyboardHookReader.cpp
// Author		intelfx
// Description	Reader implementation - global system keyboard hook.
// -----------------------------------------------------------------------------

static const size_t typemagic = 'KBDH';

class KbdHookReader : public IReader, public Init::Base<KbdHookReader>
{
#ifdef TARGET_POSIX
	termios initial_terminal_settings;
	termios new_terminal_settings;
#endif

	enum
	{
		M_DEFAULT,
		M_CONSOLERAW
	} state;

public:
	KbdHookReader();
	virtual ~KbdHookReader();
	virtual void Set (const char* parameter);
	virtual void Read (void** dest, size_t* count);
	virtual size_t Type();
};

extern "C" __attribute__ ((visibility("default"))) IReader* initreader()
{
	return &KbdHookReader::Instance();
}

KbdHookReader::KbdHookReader() :
state (M_DEFAULT)
{
}

KbdHookReader::~KbdHookReader()
{
	int stdin_fd = fileno (stdin);

	switch (state)
	{
		case M_CONSOLERAW:
			tcsetattr (stdin_fd, TCSADRAIN, &initial_terminal_settings);
			break;

		case M_DEFAULT:
			break;

		default:
			__asshole ("Switch error");
	}
}

void KbdHookReader::Read (void** dest, size_t* count)
{
	*dest = malloc (1);
	*count = 1;

	char c;
	do { c = getchar(); } while (!isgraph (c) && (c != ' '));
	**reinterpret_cast<char**> (dest) = c;
}

void KbdHookReader::Set (const char* parameter)
{
	__assert (parameter, "NULL parameter stream");

	if (!strcmp (parameter, "global"))
	{
		msg (E_WARNING, E_USER, "Global hooking is not implemented, using console");
	}

	else if (!strcmp (parameter, "x11"))
	{
		msg (E_WARNING, E_USER, "X11 hooking is not implemented, using console");
	}

#ifdef TARGET_POSIX
	else if (!strcmp (parameter, "consoleraw"))
	{
		msg (E_WARNING, E_USER, "Setting console into raw mode");

		int stdin_fd = fileno (stdin);

		tcgetattr (stdin_fd, &initial_terminal_settings);
		tcgetattr (stdin_fd, &new_terminal_settings);

		// make raw - emulate cfmakeraw() which is nonstandard
		new_terminal_settings.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
		new_terminal_settings.c_oflag &= ~(OPOST);
		new_terminal_settings.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
		new_terminal_settings.c_cflag &= ~(CSIZE | PARENB);
		new_terminal_settings.c_cflag |= CS8;
	}
#endif

	else
		msg (E_CRITICAL, E_USER, "Unknown parameter: \"%s\"", parameter);
}

size_t KbdHookReader::Type()
{
	return typemagic;
}
