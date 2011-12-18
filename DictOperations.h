#ifndef _DICT_OPERATIONS_H
#define _DICT_OPERATIONS_H

#include "build.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			DictOperations.h
// Author		intelfx
// Description	Word operations for the dictionary (assignment #4.b).
// -----------------------------------------------------------------------------

enum NmOperation
{
	S_EXPAND = 1,
	S_SCOUNT
};

const char* tr_op_names[S_SCOUNT] =
{
	"none"
// 	"decompressed"
// 	"apostrophes",
// 	"plural",
// 	"past participle",
// 	"present participle (-ing)"
};

typedef bool(*norm_op)(wchar_t*);



#endif // _DICT_OPERATIONS_H