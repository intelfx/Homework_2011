#ifndef _DICT_OPERATIONS_H
#define _DICT_OPERATIONS_H

#include "build.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			DictOperations.h
// Author		intelfx
// Description	Word operations for the dictionary (assignment #4.b).
// -----------------------------------------------------------------------------

#include "DictOpImpl.h"

enum NmOperation
{
	S_NONE = 0,
	S_APOSTROPHES,
	S_PLURAL,
	S_PARTICIPLE,
	S_PR_PART,
	S_SCOUNT
};

const char* tr_op_names[S_SCOUNT] =
{
	"none",
// 	"decompressed"
	"apostrophes",
	"plural",
	"past participle",
	"present participle (-ing)"
};

typedef bool(*norm_op)(std::wstring&);

norm_op operations[S_SCOUNT] =
{
	0,
	&normalisation_apostrophes,
	&normalisation_plural,
	&normalisation_participle,
	&normalisation_prparticiple
};


#endif // _DICT_OPERATIONS_H