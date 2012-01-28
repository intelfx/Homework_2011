#include "stdafx.h"
#include "fxpxml.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxpxml.cpp
// Author		intelfx
// Description	XML parser
// -----------------------------------------------------------------------------

const fxla::config fxpxml::fxla_xml =
{
	8,
	{
		" ", // Standard delimiters
		"\n",
		"\r",
		"\t",
		"<?", // Declaration begin
		"?>", // Declaration end
		"<!--", // Comment begin
		"-->", // Comment end
		"<",
		">",
		"=",
		"/"
	},
	{
		0,
		0,
		0,
		0,
		-1, // 4th token '<?' is DCS in comment mode
		0,
		-1 // 6th token '<!--' is DCS in comment mode
	},
	{
		0,
		0,
		0,
		0,
		5, // 4th token '<?' has DCF @ 5th token '?>'
		0,
		7 // 6th token '<!--' has DCF @ 7th token '-->'
	},
};

fxpxml::param::param (const char* pname_,
					 unsigned pname_len_,
					 const fxla::position& pname_pos_) :
pname_pos (pname_pos_),
pvalue (0)
{
	__sassert (pname_, "Invalid pointer");

	pname = new char[pname_len_ + 1];
	strsub (pname, pname_, pname_len_);
}

fxpxml::param::~param()
{
	delete[] pname;
	delete[] pvalue;
}

void fxpxml::param::storeVal (const char* pvalue_,
							  unsigned pvalue_len_,
							  const fxla::position& pvalue_pos_)
{
	__sassert (pvalue_, "Invalid pointer");

	pvalue_pos = pvalue_pos_;
	pvalue = new char[pvalue_len_ + 1];
	fxpxml::hsc_strcpy (pvalue, pvalue_, pvalue_len_);
}

fxpxml::tag::tag (const char* tname_,
				  unsigned tname_len_,
				  const fxla::position& tname_pos_) :
tname_pos (tname_pos_)
{
	__sassert (tname_, "Invalid pointer");

	tname = new char[tname_len_ + 1];
	strsub (tname, tname_, tname_len_);
}

fxpxml::tag::~tag()
{
	delete[] tname;
}

fxpxml::param* fxpxml::tag::getParByName (const char* name)
{
	__sassert (name, "Invalid pointer");

	for (unsigned i = 0; i < parameters.size(); ++i)
	{
		if (!strcmp (parameters[i] ->pname, name))
			return parameters[i];
	}

	return 0;
}

fxpxml::fxpxml()
{
}

fxpxml::~fxpxml()
{
}

void fxpxml::load (const char* filename, pvect<tag>& tags)
{
	fxla lexer;
	lexer.setup (&fxla_xml);

	__assert(filename, "Invalid pointer");
	FILE* xmlfile = fopen (filename, "rb");
	__verify (xmlfile, "Invalid file '%s'", filename);

	int xmlfsize = fsize (xmlfile);
	char* xmlfdata = new char[xmlfsize + 1];
	fread (xmlfdata, 1, xmlfsize, xmlfile);
	xmlfdata[xmlfsize] = 0;
	fclose (xmlfile);

	str_tolower (xmlfdata);
	lexer.stream (xmlfdata);

	tags.clear();
	parse_ (lexer, "", tags, 0);
	delete[] xmlfdata;
}

void fxpxml::parse_ (fxla& lexer,
					 const char* name,
					 pvect<tag>& taglist,
					 unsigned lv)
{
	state curState = S_NOTAG;

	while ((lexer.stream(), lexer.last.type))
	{
		switch (curState)
		{
			case S_NOTAG:
				// FROM: initial.
				// COND: initial.
				// EXPT: '<' (new tag) ->S_TAG.
				curState = dostate_notag (lexer);
				break;

			case S_TAG:
				// FROM: S_NOTAG
				// COND: Got a new tag.
				// EXPT: '/' (closing tag's marker) ->S_CTAG_CHECKNAME.
				// EXPT:[[:alpha:]]* (opening tag's name) ->S_PARMS.

				curState = dostate_tag (lexer, taglist);
				break;

			case S_PARMS:
				// FROM: S_TAG, S_PARMV
				// COND: Got opening tag's name, got parameter's value
				// EXPT:[[:alpha]]* (parameter name) ->push_back parameter with specified name ->S_PARM2.
				// EXPT: '>' (tag finished) ->S_TAG_FIN.
				// EXPT: '/' (self-closing tag) ->S_SCTAG.

				curState = dostate_parms (lexer, taglist.back() ->parameters, 0);
				break;

			case S_PARM2:
				// FROM: S_PARMS, S_PARM2
				// COND: Got parameter's name
				// EXPT:[[:alpha:]]* (parameter name, store it) ->S_PARM2.
				// EXPT: '=' (equal sign after parameter name) ->S_PARMV.
				// EXPT: '>' (tag_finished) ->S_TAG_FIN.
				// EXPT: '/' (self-closing tag) ->S_SCTAG.

				curState = dostate_parms (lexer, taglist.back() ->parameters, 1);
				break;

			case S_PARMV:
				// FROM: S_PARM2
				// COND: Current parameter should have a value
				// EXPT:[[:alpha:]]* (parameter value, store it) ->S_PARMS.

				curState = dostate_parmv (lexer, taglist.back() ->parameters.back());
				break;

			case S_CTAG_CHECKNAME:
				// FROM: S_TAG
				// COND: Closing tag
				// EXPT: {name} (input-parameter 'name') ->S_CTAG_CHECKPARMS.

				curState = dostate_cl_cn (lexer, name);
				break;

			case S_CTAG_CHECKPARMS:
				// FROM: S_CTAG_CHECKNAME
				// COND: Current closing tag's name is equal to parameter name
				// EXPT: '>' (tag finished) ->S_CTAG_RET.

				curState = dostate_cl_cp (lexer, 0);
				break;

			case S_SCTAG:
				// FROM: S_PARMS / S_PARM2
				// COND: Self-closing tag
				// EXPT: '>' (tag finished) ->S_NOTAG.

				curState = dostate_cl_cp (lexer, 1);
				break;

			// These states live only inside of an iteration
			case S_TAG_FIN:
			case S_CTAG_RET:

			case S_INVALID:
			default:
				__asshole ("Some bug in XML parser main automaton");
				break;
		} // switch


		if (curState == S_TAG_FIN)
		{
			// FROM: multiple
			// COND: Normal tag closed.
			// EXPT: chain to next level immediately, then ->S_NOTAG

			parse_ (lexer,
					taglist.back() ->tname,
					taglist.back() ->childtags,
					lv + 1);

			curState = S_NOTAG;
		}


		if (curState == S_CTAG_RET)
		// FROM: S_CTAG_CHECKPARMS
		// COND: Closing tag has finished
		// ACTN: Return to previous level immediately.
			return;


	} // for

	__verify (!lv, "Unexpected EOF");
}

fxpxml::state fxpxml::dostate_notag (const fxla& lexer)
{
	if (lexer.last.type == 1 && lexer.last.sdata == 0)
		return S_TAG; // If we get open bracket - we have a tag and expect its name.

	return S_NOTAG;
}

fxpxml::state fxpxml::dostate_tag (const fxla& lexer, pvect<tag>& taglist)
{
	if (lexer.last.type == 1)
	{
		__verify (lexer.last.sdata == 3,
				  "Invalid token (in tag, before name) '%.*s' @ %d : %d, "
				  "expected '%s' or tag name",
				  lexer.last.length,
				  lexer.last.get_data (fxla_xml),
				  lexer.last.pos.row,
				  lexer.last.pos.col,
				  fxla_xml.tokens[3 + fxla_xml.word_offset]);

		return S_CTAG_CHECKNAME; // Closing tag - verify the hierarchical pair.
	}

	else
	{
		// Token is supposed to be a tag name.
		taglist.push_back (new tag (lexer.last.udata, lexer.last.length, lexer.last.pos));
		return S_PARMS; // After name we expect parameters.
	}
}

fxpxml::state fxpxml::dostate_cl_cn (const fxla& lexer, const char* name)
{
	__verify (lexer.last.compare (name),
			  "Invalid token (in closing tag, tag name) '%.*s' @ %d : %d, "
			  "expected '%s': Check hierarchy",
			  lexer.last.length,
			  lexer.last.get_data (fxla_xml),
			  lexer.last.pos.row,
			  lexer.last.pos.col,
			  name);

	return S_CTAG_CHECKPARMS;
}

fxpxml::state fxpxml::dostate_cl_cp (const fxla& lexer, bool selfclosing)
{
	__verify (lexer.last.type == 1 && lexer.last.sdata == 1,
			  "Invalid token (in closing tag, after name) '%.*s' @ %d : %d, "
			  "expected '%s': Garbage in closing tag",
			  lexer.last.length,
			  lexer.last.get_data (fxla_xml),
			  lexer.last.pos.row,
			  lexer.last.pos.col,
			  fxla_xml.tokens[1 + fxla_xml.word_offset]);

	return selfclosing ? S_NOTAG : S_CTAG_RET; // No "trash" following. It's OK.
}

fxpxml::state fxpxml::dostate_parms (const fxla& lexer, pvect<param>& parlist, bool accept_value)
{
	if (lexer.last.type == 1)
	{
		if (accept_value)
		{
			__verify (lexer.last.sdata == 1 ||
					  lexer.last.sdata == 2 ||
					  lexer.last.sdata == 3,
					  "Invalid token (in tag, after par. name) '%.*s' @ %d : %d, "
					  "expected '%s' '%s' '%s': Generic syntax error",
					  lexer.last.length,
					  lexer.last.get_data (fxla_xml),
					  lexer.last.pos.row,
					  lexer.last.pos.col,
					  fxla_xml.tokens[1 + fxla_xml.word_offset],
					  fxla_xml.tokens[2 + fxla_xml.word_offset],
					  fxla_xml.tokens[3 + fxla_xml.word_offset]);
		}

		else
		{
			__verify (lexer.last.sdata == 1 ||
					  lexer.last.sdata == 3,
					  "Invalid token (in tag, after name) '%.*s' @ %d : %d, "
					  "expected '%s' '%s' or parameter value",
					  lexer.last.length,
					  lexer.last.get_data (fxla_xml),
					  lexer.last.pos.row,
					  lexer.last.pos.col,
					  fxla_xml.tokens[1 + fxla_xml.word_offset],
					  fxla_xml.tokens[3 + fxla_xml.word_offset]);
		}

		switch (lexer.last.sdata)
		{
		case 1:
			return S_TAG_FIN; // End of tag.

		case 2:
			return S_PARMV; // Equal sign.

		case 3:
			return S_SCTAG; // Self-closing tag.

		default: // Fake case - this condition is checked in assertions above
			break;
		}

		msg (E_FUCKING_EPIC_SHIT, E_USER, "Logic fail");
		return S_INVALID;
	}

	else
	{
		// Current token is supposed to be a parameter name.
		parlist.push_back (new param (lexer.last.get_data (fxla_xml),
									  lexer.last.length,
									  lexer.last.pos));
		return S_PARM2; // Now we expect for equal sign or for next par name.
	}
}

fxpxml::state fxpxml::dostate_parmv (const fxla& lexer, param* parameter)
{
	__assert (parameter, "Invalid pointer");

	__verify (lexer.last.type == 2,
			  "Invalid token (in tag, after equal sign) '%.*s' @ %d : %d, "
			  "expected parameter value",
			  lexer.last.length,
			  lexer.last.get_data (fxla_xml),
			  lexer.last.pos.row,
			  lexer.last.pos.col);

	// Current data is supposed to be a parameter value.
	parameter ->storeVal (lexer.last.get_data (fxla_xml),
						  lexer.last.length,
						  lexer.last.pos);
	return S_PARMS; // Now we expect new parameter or tag end.
}

void fxpxml::hsc_strcpy (char* dest_, const char* src_, unsigned src_len_)
{
	__sassert (dest_ && src_, "Invalid pointer");

	const char* src_begin = src_;

	while (const char* src_fnd = strnchr (src_, src_len_, '&'))
	{
		unsigned delta = src_fnd - src_;
		strncpy (dest_, src_, delta);
		dest_ += delta;

		src_ = src_fnd + 1;

		if (!strncmp (src_, "amp;", 4))
		{
			*dest_++ = '&';
			src_ += 4;
		}

		else if (!strncmp (src_, "lt;", 3))
		{
			*dest_++ = '<';
			src_ += 3;
		}

		else if (!strncmp (src_, "gt;", 3))
		{
			*dest_++ = '>';
			src_ += 3;
		}

		else if (!strncmp (src_, "quot;", 5))
		{
			*dest_++ = '"';
			src_ += 5;
		}

		else if (!strncmp (src_, "apos;", 5))
		{
			*dest_++ = '\'';
			src_ += 5;
		}

		else
			*dest_++ = '&'; // Leave it alone...
	}

	strsub (dest_, src_, src_len_ - (src_ - src_begin));
}

ImplementDescriptor(fxpxml, "XML parser", MOD_APPMODULE);
