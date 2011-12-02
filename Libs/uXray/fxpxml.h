#ifndef _FXPXML_H_
#define _FXPXML_H_

#include "build.h"

#include "fxassert.h"
#include "fxcwrp.h"
#include "fxla.h"

// -----------------------------------------------------------------------------
// Library		FXlib
// File			fxpxml.h
// Author		intelfx
// Description	XML parser
// -----------------------------------------------------------------------------

DeclareDescriptor(fxpxml);

class FXLIB_API fxpxml : LogBase(fxpxml)
{
public:
	// Constants
	static const fxla::config fxla_xml;

	// Data types (predefinition)
	enum state // All parser's states
	{
		S_NOTAG = 0x0,
		S_TAG,
		S_PARMS,
		S_PARM2,
		S_PARMV,
		S_CTAG_CHECKNAME,
		S_CTAG_CHECKPARMS,
		S_SCTAG,
		S_TAG_FIN,
		S_CTAG_RET,
		S_INVALID
	};

	class FXLIB_API param; // Represents tag's single parameter
	class FXLIB_API tag; // Represents DOM's single tag

private:
	void			parse_ (fxla& lexer,
							const char* name,
							pvect<tag>& tlist,
							unsigned lv); // Internal parsing automaton function
	fxpxml::state	dostate_notag (const fxla& lexer);
	fxpxml::state	dostate_tag (const fxla& lexer, pvect<tag>& taglist);
	fxpxml::state	dostate_cl_cn (const fxla& lexer, const char* name);
	fxpxml::state	dostate_cl_cp (const fxla& lexer, bool selfclosing);
	fxpxml::state	dostate_parms (const fxla& lexer, pvect<param>& parlist, bool mdf);
	fxpxml::state	dostate_parmv (const fxla& lexer, param* parameter);

public:

	// External data types
	class FXLIB_API param
	{
	public:
		// Main data
		char*			pname;
		fxla::position	pname_pos;
		char*			pvalue;
		fxla::position	pvalue_pos;

		// Functions
						param (const char* pname_,
							   unsigned pname_len_,
							   const fxla::position& pname_pos_); // CTOR
						~param(); // DTOR
		void			storeVal (const char* pvalue_,
								 unsigned pvalue_len_,
							   	 const fxla::position& pvalue_pos_); // Set value
	};

	class FXLIB_API tag
	{
	public:
		// Main data
		char*			tname; // Name token
		fxla::position	tname_pos;
		pvect<param>	parameters; // Tag's parameters
		pvect<tag>		childtags; // Tag's contained entities

		// Functions
						tag (const char* tname_,
							 unsigned tname_len_,
							 const fxla::position& tname_pos_);
						~tag();
		param*			getParByName (const char* name);
	};

	// Main data
	static fxpxml instance;

	// ctor / dtor
	fxpxml(); // CTOR
	~fxpxml(); // DTOR

	// Functions
	void				load (const char* filename, pvect<tag>& tags);

	static void			hsc_strcpy (char* dest_, const char* src_, unsigned src_len_);
};

#endif // _FXPXML_H_
