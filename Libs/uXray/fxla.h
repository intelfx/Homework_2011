#ifndef _FXLA_H_
#define _FXLA_H_

#include "build.h"

#include "fxassert.h"
#include "fxcwrp.h"
#include "fxsearch.h"
#include "fxmisc.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxla.h
// Author		intelfx
// Description	Lexical analyzer
// -----------------------------------------------------------------------------

DeclareDescriptor(fxla);
DeclareDescriptor(fxla_token);

/*
 *	|-----------------[All tokens]-------------------|
 *	[U-tokens]								[S-tokens]
 *	{Unknown, or unspecified tokens			{Known, or specified tokens
 *	e.g. identifiers}		|---------------e.g. reserved words and operators}
 *							[delimiters]					[words]----------|
 *							{Tokens, which are not saved}	{Tokens, which are saved}
 */

class FXLIB_API fxla : LogBase(fxla)
{
	fxla (const fxla&)				= delete;
	fxla& operator= (const fxla&)	= delete;

public:
	class FXLIB_API position
	{
	public:
		// Main data
		unsigned			row;
		int					col;

		// Methods
		position();
		inline void			newline();
		inline void			update (unsigned move_);
		inline void			clear();
	};

private:
	// Main data
	searchEngine		finder; // Search engine used to find S-tokens

	position			pos;
	int					dc_status;
	int					dce_token;
	const char*			main_ptr; // Ptr to current U-token + S-token pair
	const char*			stoken_ptr; // Pointer to S-token in current pair
	unsigned			stoken_length; // Length of S-token in current pair
	unsigned			stoken_what; // Index of S-token in current pair
	unsigned			utoken_len; // Length of U-token in current pair

	// Internal methods
	void				init_lexer (const char* src_);

public:
	// External types
	struct FXLIB_API config
	{
		static const unsigned	tokenMax = 40;
		static const unsigned	dcMax = 4;
		const unsigned			tokenCnt;
		const unsigned			word_offset;

		const char*			tokens[tokenMax]; // S-tokens array

		/*
		 * Direct-copy mode:
		 * Direct-copy mode is the special lexer mode. When it reads a special
		 * direct-copy starting token (DCS) it begins searching for
		 * direct-copy finishing token (DCF) linked to read DCS.
		 * Lexer treats space between DCS and DCF tokens
		 * as a single U-token or delimiter (useful for comments).
		 *
		 * dcs: describes DCS-meaning presence for each S-token
		 * =0 : normal token, no direct-copy
		 * <0 : DCS token (comment mode)
		 * >0 : DCS token (U-token mode)
		 *
		 * dce: describes DCS ->DCF links for each S-token
		 * for normal tokens: unused
		 * for DCS tokens: linked DCF token
		 */

		const int			dcs[tokenMax];
		const int			dce[tokenMax];
	};

	class FXLIB_API token : LogBase(fxla_token)
	{
	public:
		const char*	udata;
		unsigned	ulen;
		unsigned	sdata;
		unsigned	type;
		position	pos;

		// This function compares U-token with an ASCIZ string.
		// It verifies token's type and compares not only contents
		// but lengths of strings
		bool compare (const char* str) const;

		// This function returns symbolic token data depending on its type.
		// It requires source config to retrieve S-token symbolic data.
		// You can take length in member ulen (it always contains valid length).
		inline const char* get_data (const config& conf) const
		{
			return (type == 2) ? udata : conf.tokens[sdata + conf.word_offset];
		}

		token();
		~token();

		token (const token&);
		token& operator= (const token&);
	};

	typedef pvect<token>::const_iterator token_iterator;

	// Main data
	const config*		conf;
	token				last;

	// Methods
						fxla();
						~fxla();

	// Used to set config describing the language.
	void				setup (const config* _config);

	// Used to parse in the streaming mode - one token per call.
	// When called with non-null pointer, sets input string.
	// When called with null pointer, parses next token leaving it in last field.
	void				stream (const char* src_ = 0);

	// Used to parse in the non-streaming mode - all tokens per call.
	// Must be called with non-null pointer, sets input string and pushes tokens
	// into given vector.
	void				vect (const char* src_, pvect<token>& tokens);

	// Used to parse one word. It is slow because it doesn't use search engine.
	static unsigned		get_token (const char* text, const config& conf);
};

#endif // _FXLA_H_
