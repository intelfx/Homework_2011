#include "stdafx.h"
#include "fxla.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxla.cpp
// Author		intelfx
// Description	Lexical analyzer
// -----------------------------------------------------------------------------

fxla::position::position() :
row (1),
col (1)
{
}

inline void	fxla::position::newline()
{
	row++;
	col = 1;
}

inline void	fxla::position::update (unsigned move)
{
	col += move;
}

inline void fxla::position::clear()
{
	row = col = 1;
}

fxla::fxla() :
finder(),
pos(),
dc_status (0),
dce_token (0),
main_ptr (0),
stoken_ptr (0),
stoken_length (0),
stoken_what (0),
utoken_len (0),
conf (0),
last()
{
}

fxla::~fxla()
{
}

void fxla::setup (const config* conf_)
{
	__assert (conf_, "Invalid pointer");
	conf = conf_;

	// Initialize search engine
	unsigned t_count;
	for (t_count = 0; (t_count < config::tokenMax) && (conf ->tokens[t_count]); ++t_count) {}

	finder.load (conf ->tokens, t_count);
}

void fxla::stream (const char* src_ /*= 0*/)
{
	bool utoken_cq = 0;

	// Here we initialize static variables and just return
	if (src_)
	{
		init_lexer (src_);
		return;
	}

	// Else we do parsing
	__assert (main_ptr, "Invalid pointer");

	last.type = token::T_NONE;

	do
	{
		if (stoken_length)
		{
			// If we have valid S-token data, add (return) S-token
			// Save it (skipping delimiters)
			if (stoken_what >= conf ->word_offset)
			{
				last.type = token::T_WORD;
				last.sdata = stoken_what - conf ->word_offset;
				last.udata = stoken_ptr;
				last.length = stoken_length;
				last.pos = pos;
			}

			// Save direct-copy data
			dc_status = conf ->dcs[stoken_what];
			dce_token = conf ->dce[stoken_what];

			// Update internal parameters
			main_ptr += stoken_length;
			pos.update (stoken_length);

			// Invalidate current data to tokenize next pair
			stoken_length = 0;
		}

		// Now we should scan for the next pair
		// then save S-token data
		// then add (return) U-token
		else
		{
			for (;;)
			{
				switch (*main_ptr)
				{
				case '\0':
					// String end - return now with no token.
					return;

				case '\n':
					pos.newline();
					main_ptr++;
					continue;

				case '\r':
					pos.newline();

					if (*main_ptr++ == '\n') // '\r''\n'
						main_ptr++; // Treat it as a single character
					continue;

				default:
					break;
				}

				break;
			}

			// First check for direct-copy mode
			if (dc_status)
			{
				// We'll scan for complement token
				// searchEngine would do the same for single word, so use strstr.

				// Get pointer to S-token for common tasks
				stoken_ptr = strstr (main_ptr, conf ->tokens[dce_token]);

				// Found the complement token
				if (stoken_ptr)
				{
					// Get correct S-token info
					stoken_what = dce_token;
					stoken_length = strlen (conf ->tokens[dce_token]);

					// Now stoken_* describe the DCF token.
					// main_ptr points to the space between DCS and DCF tokens.
					// If we do not need to save it, let main_ptr point to DCF token.
					if (dc_status < 0)
						main_ptr = stoken_ptr;
				}
			}

			// Then check for quoting symbols
			else if ((*main_ptr == '\'') || (*main_ptr == '"'))
			{
				// Get stoken_ptr for U-token length
				__assert (stoken_ptr = strchr (main_ptr + 1, *main_ptr) + 1,
					"Unterminated quoting character '%c' @ %d : %d", *main_ptr, last.pos.row, last.pos.col);

				utoken_cq = 1;

				// We can't get S-token, so leave it empty.
				// This branch will be called again on the next run.
			}

			// If all is as usual, normally scan for S-token
			else
				finder.search (main_ptr, stoken_ptr, stoken_what, stoken_length);

			// Calculate U-token length
			if (!stoken_ptr)
				stoken_ptr = strchr (main_ptr, '\0');

			utoken_len = stoken_ptr - main_ptr;

			// Add the U-token
			if (utoken_len)
			{
				// Save it
				last.type = token::T_UTOKEN; // U-token
				last.sdata = -1;
				last.udata = main_ptr + utoken_cq;
				last.length = utoken_len - 2 * utoken_cq;
				last.pos = pos;

				// Update internal parameters
				main_ptr = stoken_ptr;
				pos.update (utoken_len);
			}

			// Now we have:
			// main_ptr = stoken_ptr - the next token
			// stoken_length - length of following S-token, or 0 if re-pass required.
			// stoken_what - S-token index (or garbage if !stoken_length)
		}
	} while (last.type == token::T_NONE); // We will repeat until we get a token
}

void fxla::init_lexer (const char* src_)
{
	__assert (src_, "Invalid pointer");

	pos.clear();
	main_ptr = src_;
	stoken_ptr = 0;
	dc_status = dce_token = utoken_len = stoken_length = stoken_what = 0;
}

bool fxla::token::compare (const char* str) const
{
	__assert (str, "Invalid pointer");

	if (type != token::T_UTOKEN)
		return 0;

	return !strncmp (udata, str, length);
}

fxla::token::token() :
udata (0),
length (0),
type (T_NONE),
pos()
{
}

fxla::token::~token()
{
}

fxla::token::token (const fxla::token& that) :
udata (that.udata),
length (that.length),
type (that.type),
pos (that.pos)
{
}

fxla::token& fxla::token::operator= (const fxla::token& that)
{
	udata = that.udata;
	length = that.length;
	sdata = that.sdata;
	type = that.type;
	pos = that.pos;

	return *this;
}

ImplementDescriptor(fxla, "lexic analyzer", MOD_APPMODULE);
ImplementDescriptor(fxla_token, "token", MOD_OBJECT);
