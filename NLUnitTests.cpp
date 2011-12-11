#include "stdafx.h"

#include <uXray/fxlog_console.h>
#include "MMU.h"

bool TestMMU()
{
	Processor::IMMU* mmu = new ProcessorImplementation::MMU;

	mmu ->AllocContextBuffer();

	Processor::calc_t data_image[10] = {0.0, 0.1, 1.0, 1.1, 1.2, 2.0, 3.4, 5.1, 6.0, 7.9};
	Processor::calc_t stack_image[10] = {10.0, 10.1, 11.0, 11.1, 11.2, 12.0, 13.4, 15.1, 16.0, 17.9};

	mmu ->ReadData (data_image, 10);
	mmu ->ReadStack (stack_image, 10);

	Processor::DecodedCommand commands[10] = {};

	for (unsigned i = 0; i < 10; ++i)
	{
		commands[i].command = rand() % 100;
		if (commands[i].command & 0x1)
			commands[i].arg.value = data_image[i];

		else
		{
			commands[i].arg.ref.is_symbol = 0;
			commands[i].arg.ref.direct.type = Processor::S_DATA;
			commands[i].arg.ref.direct.address = i;
		}
	}

	mmu ->ReadText (commands, 10);


	sverify_statement (mmu ->GetDataSize() == 10, "Invalid resulting data size");
	sverify_statement (mmu ->GetTextSize() == 10, "Invalid resulting data size");
	sverify_statement (mmu ->GetStackTop() == 10, "Invalid resulting stack size");

	smsg (E_INFO, E_USER, "MMU init completed. Executing random-access tests.");

	for (unsigned i = 0; i < 10; ++i)
	{
		mmu ->GetContext().frame = rand() % 10;
		smsg (E_INFO, E_VERBOSE, "Trying with stack frame = %lu", mmu ->GetContext().frame);

		for (unsigned j = 0; j < 10; ++j)
		{
			sverify_statement (mmu ->AStackTop (j) == stack_image[10 - (j + 1)],
							  "Stack top access failed: [%u] = %lg (req %lg)",
							  j, mmu ->AStackTop (j), stack_image[10 - (j + 1)]);

			int frm_access = j - mmu ->GetContext().frame;
			sverify_statement (mmu ->AStackFrame (frm_access) == stack_image[j],
							  "Stack frame access failed: [%d] = %lg (req %lg)",
							  frm_access, mmu ->AStackFrame (frm_access), stack_image[j]);

			sverify_statement (mmu ->AData (j) == data_image[j], "Data access failed: [%u] = %lg (req %lg)",
							  j, mmu ->AData (j), data_image[j]);

			Processor::DecodedCommand& dc = mmu ->ACommand (j);

			if (dc.command & 0x1)
			{
				sverify_statement (mmu ->AData (j) == dc.arg.value, "Command test failed: [%u] cmd %lu: arg = %lg (req %lg)",
								  j, dc.command, dc.arg.value, mmu ->AData (j));
			}

			else
			{
				sverify_statement (mmu ->AData (j) == data_image[dc.arg.ref.direct.address],
								  "Command test failed: [%u] cmd %lu: arg = %lg (req %lg)",
								  j, dc.command, data_image[dc.arg.ref.direct.address], mmu ->AData (j));
			}
		} // for (j)
	} // for (i)

	return 1;
}

int main (int argc, char** argv)
{
	Debug::System::Instance().SetTargetProperties (Debug::CreateTarget ("stderr",
																		EVERYTHING,
																		EVERYTHING & ~MASK (Debug::E_DEBUG)),
												   &FXConLog::Instance());

	for (unsigned i = 1; i < argc; ++i)
	{
		const char* arg = argv[i];
		bool result = 0;

		smsg (E_INFO, E_USER, "Doing \"%s\" test", arg);

		if (!strcmp (arg, "mmu"))
			result = TestMMU();

		else
		{
			smsg (E_WARNING, E_USER, "Unknown argument \"%s\"", arg);
			continue;
		}

		smsg (E_INFO, E_USER, "Test result: %s", result ? "passed" : "failed");
	}
}