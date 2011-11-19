#include "stdafx.h"
#include "InputServerApp.h"
#include "SharedServerClientProtocol.h"

#include "morse_code.h"

class ScrollLockMorseTransmitter : public ITransmitter, public Init::Base<ScrollLockMorseTransmitter>
{
	unsigned base_length_ms;
	static const char* const translate_table [256];
	static const char* const invalid_char_string;
	static const char* const transmission_end_string;

	bool connected;

	void _flash (bool is_dash);
	void _init();

	void _microsleep (size_t ms);

	void transmit_string (const char* string);

public:
	ScrollLockMorseTransmitter();

	virtual void Connect();
	virtual bool ConnStatus();
	virtual void Disconnect();
	virtual void Set (const char* parameter);
	virtual void Tx (const Buffer* data);
};

extern "C" __attribute__ ((visibility("default"))) ITransmitter* inittransmitter()
{
	return &ScrollLockMorseTransmitter::Instance();
}

ScrollLockMorseTransmitter::ScrollLockMorseTransmitter() :
base_length_ms (0),
connected (0)
{
}


void ScrollLockMorseTransmitter::_flash (bool is_dash)
{
	system ("sudo setleds -F +caps < /dev/tty7");
	_microsleep ((is_dash ? 3 : 1) * base_length_ms);
	system ("sudo setleds -F -caps < /dev/tty7");
}

void ScrollLockMorseTransmitter::_init()
{
	system ("sudo setleds -F -caps < /dev/tty7");
}

void ScrollLockMorseTransmitter::_microsleep (size_t ms)
{
	timespec tm;
	tm.tv_sec = ms / 1000;
	tm.tv_nsec = (ms % 1000) * 1000000;

	nanosleep (&tm, 0);
}


void ScrollLockMorseTransmitter::Connect()
{
	base_length_ms = 100;
	connected = 1;
}

bool ScrollLockMorseTransmitter::ConnStatus()
{
	return connected;
}

void ScrollLockMorseTransmitter::Disconnect()
{
	connected = 0;
}

void ScrollLockMorseTransmitter::Set (const char* parameter)
{
	if (sscanf (parameter, "baselength=%u", &base_length_ms) != 1)
		msg (E_CRITICAL, E_USER, "Unknown parameter: \"%s\"", parameter);
}

void ScrollLockMorseTransmitter::transmit_string (const char* string)
{
	while (char morse_char = *string++)
	{
		switch (morse_char)
		{
			case '.':
				_flash (0);
				break;

			case '-':
				_flash (1);
				break;

			default:
				__asshole ("Invalid symbol '%c' in morse string \"%s\"",
						   morse_char, string);
		}

		_microsleep (base_length_ms); // sleep between elements of a char - 1 * dot
	}

	_microsleep (3 * base_length_ms); // sleep between chars - 3 * dot
}


void ScrollLockMorseTransmitter::Tx (const Buffer* input)
{
	_init();

	if (!input ->data || !input ->length)
	{
		transmit_string (transmission_end_string);
	}

	char* dptr = reinterpret_cast<char*> (input ->data);

	msg (E_INFO, E_DEBUGLIB, "Transmitting \"%.*s\" [%zu]", input ->length, dptr, input ->length);
	for (unsigned i = 0; i < input ->length; ++i)
	{
		if (dptr[i] == ' ')
		{
			_microsleep (7 * base_length_ms); // sleep between words - 7 * dot
		}

		else
		{
			unsigned subscript = dptr[i];
			__assert (subscript < 0x100, "Invalid char somewhy...");

			const char* morse_string = morse_code[subscript];
			if (!morse_string)
				morse_string = invalid_char_string;

			transmit_string (morse_string);
		}
	}

	_init();
}

const char* const ScrollLockMorseTransmitter::invalid_char_string = ".......";
const char* const ScrollLockMorseTransmitter::transmission_end_string = "..-.-";