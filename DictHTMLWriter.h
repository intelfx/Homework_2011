#ifndef _DICTHTMLWRITER_H
#define _DICTHTMLWRITER_H

#include "build.h"
#include "DictStructures.h"

// -----------------------------------------------------------------------------
// Library      Homework
// File         DictHTMLWriter.h
// Author       intelfx
// Description  HTML output driver for the dictionary (assignment #4.b).
// -----------------------------------------------------------------------------

class HTMLWriter
{
public:
	void Prepare (FILE* output);
	void Close();

	void Write();
};

#endif // _DICTHTMLWRITER_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
