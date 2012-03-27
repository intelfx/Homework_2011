#include <stdio.h>

const char* commands_list[] =
{
	"init",
	"push",
	"pop",
	"top",

	"sleep",

	"ld",
	"wr",

	"add",
	"sub",
	"mul",
	"div",

	"inc",
	"dec",

	"neg",
	"sqrt",

	"anal", // Fuck yeah
	"cmp",
	"swap",
	"dup",

	"je",
	"jne",
	"ja",
	"jna",
	"jae",
	"jnae",
	"jb",
	"jnb",
	"jbe",
	"jnbe",

	"jmp",
	"call",
	"ret",

	"dumpfl",
	"snfc",
	"cnfc",

	"cswitch",
	"exec",
	"invd",

	"lasm",
	"lbin",
	"wasm",
	"wbin",

	"quit",
	0
};

const char* commands_desc[] =
{
	"Stack: initialize stack and execution environment",
	"Stack: push a value onto the stack",
	"Stack: remove a value from the stack",
	"Stack: peek a value from the stack",

	"System: hand off control to the OS",

	"Data: load data memory/register",
	"Data: write data memory/register",

	"Arithmetic: addition",
	"Arithmetic: subtraction (minuend on top)",
	"Arithmetic: multiplication",
	"Arithmetic: division (dividend on top)",

	"Arithmetic: increment by one",
	"Arithmetic: decrement by one",
	"Arithmetic: negation",
	"Arithmetic: square root extraction",

	"Accumulator: analyze top of the stack",
	"Accumulator: compare two values on the stack (R/O subtraction)",
	"Accumulator: swap two values on the stack",
	"Accumulator: duplicate the value on the stack",

	"Address: jump if ZERO",
	"Address: jump if not ZERO",
	"Address: jump if ABOVE",
	"Address: jump if not ABOVE",
	"Address: jump if ABOVE or EQUAL",
	"Address: jump if not ABOVE or EQUAL",
	"Address: jump if BELOW",
	"Address: jump if not BELOW",
	"Address: jump if BELOW or EQUAL",
	"Address: jump if not BELOW or EQUAL",

	"Address: unconditional jump",
	"Address: unconditional call",
	"Address: unconditional return",

	"Flags: dump flag register",
	"Flags: set No-Flag-Change flag",
	"Flags: clear No-Flag-Change flag",

	"Management: switch to next context (useful only in decoding mode)",
	"Management: execute next context buffer",
	"Management: clear (invalidate) next context buffer",

	"Management: load next context buffer [ASM]",
	"Management: load next context buffer [BIN]",
	"Management: write next context buffer [ASM]",
	"Management: write next context buffer [BIN]",

	"Management: stop execution/quit context",
	0
};

enum types
{
	A_NONE = 0,
	A_VALUE = 1,
	A_REFERENCE = 2
};


enum types commands_traits[] =
{
	A_NONE,
	A_VALUE,
	A_NONE,
	A_NONE,

	A_NONE,

	A_REFERENCE,
	A_REFERENCE,

	A_NONE,
	A_NONE,
	A_NONE,
	A_NONE,

	A_NONE,
	A_NONE,
	A_NONE,
	A_NONE,

	A_NONE,
	A_NONE,
	A_NONE,
	A_NONE,

	A_REFERENCE,
	A_REFERENCE,
	A_REFERENCE,
	A_REFERENCE,
	A_REFERENCE,
	A_REFERENCE,
	A_REFERENCE,
	A_REFERENCE,
	A_REFERENCE,
	A_REFERENCE,

	A_REFERENCE,
	A_REFERENCE,
	A_NONE,

	A_NONE,
	A_NONE,
	A_NONE,

	A_NONE,
	A_NONE,
	A_NONE,

	A_NONE,
	A_NONE,
	A_NONE,
	A_NONE,

	A_NONE
};

const char* cts[] =
{
	"A_NONE",
	"A_VALUE",
	"A_REFERENCE"
};

int main()
{
	FILE* f = fopen ("cmdtable.h", "w");
	unsigned i;

	fprintf (f, "initial_commands[] = {\n");

	for (i = 0; commands_list[i]; ++i)
	{
		fprintf (f,
				 "{\n"
				 "\"%s\",\n"
				 "\"%s\",\n"
				 "%s\n},\n",
				 commands_list[i],
				 commands_desc[i],
				 cts[commands_traits[i]]);
	}

	fprintf (f, "};\n");
	fclose (f);
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4; 
