#include "stdafx.h"
#include "Interfaces.h"

namespace Processor
{
	void ProcessorAPI::Flush()
	{
		verify_method;

		MMU() ->ResetEverything();
		CommandSet() ->ResetCommandSet();

		for (unsigned i = 0; i <= Value::V_MAX; ++i)
			Executor (static_cast<Value::Type> (i)) ->ResetImplementations();
	}

	void ProcessorAPI::Reset()
	{
		verify_method;

		MMU() ->ClearContext();
	}

	void ProcessorAPI::Clear()
	{
		verify_method;

		IMMU* mmu = MMU();
		mmu ->ResetBuffers (mmu ->GetContext().buffer);
	}

	void ProcessorAPI::Delete()
	{
		verify_method;

		MMU() ->RestoreContext();
	}

	void ProcessorAPI::Load (FILE* file)
	{
		verify_method;

		MMU() ->AllocContextBuffer();
		msg (E_INFO, E_VERBOSE, "Loading from stream -> context %zu", MMU() ->GetContext().buffer);

		__assert (Reader(), "Loader module is not attached");

		FileType file_type = Reader() ->RdSetup (file);

		switch (file_type)
		{
		case FT_BINARY:
		{
			MemorySectionType sec_type;
			size_t elements_count, buffer_size;

			void* image_section_buffer = 0;
			size_t image_section_buffer_size = 0;

			msg (E_INFO, E_DEBUG, "Loading binary file absolute image");

			while (Reader() ->NextSection (&sec_type, &elements_count, &buffer_size))
			{
				if (sec_type == SEC_SYMBOL_MAP)
				{
					msg (E_INFO, E_DEBUG, "Reading symbols section: %d records");
					symbol_map external_symbols;

					Reader() ->ReadSymbols (external_symbols);

					__assert (external_symbols.size() == elements_count, "Invalid symbol map size: %d",
							  external_symbols.size());

					MMU() ->ReadSymbolImage (std::move (external_symbols));
				}

				else
				{
					msg (E_INFO, E_DEBUG, "Reading section type \"%s\": %d records (%d bytes)",
						 ProcDebug::FileSectionType_ids[sec_type], elements_count, buffer_size);

					if (buffer_size > image_section_buffer_size)
					{
						image_section_buffer = realloc (image_section_buffer, buffer_size);
						__assert (image_section_buffer, "Could not allocate section image buffer: \"%s\"",
								  strerror (errno));

						image_section_buffer_size = buffer_size;
					}

					Reader() ->ReadSectionImage (image_section_buffer);
					MMU() ->ReadSection (sec_type, image_section_buffer, elements_count);
				}
			} // while (next section)

			msg (E_INFO, E_DEBUG, "Binary file read completed");
			free (image_section_buffer);

			break;
		} // binary file

		case FT_STREAM:
		{
			msg (E_INFO, E_DEBUG, "Stream file decode start");
			Linker() ->DirectLink_Init();

			while (DecodeResult* result = Reader() ->ReadStream())
			{
				size_t mmu_limits[SEC_MAX];
				MMU() ->QueryLimits (mmu_limits);

				if (!result ->mentioned_symbols.empty())
				{
					msg (E_INFO, E_DEBUG, "Adding symbols");
					Linker() ->DirectLink_Add (result ->mentioned_symbols, mmu_limits);
				}

				if (!result ->commands.empty())
				{
					msg (E_INFO, E_DEBUG, "Adding commands: %d", result ->commands.size());
					MMU() ->ReadSection (SEC_CODE_IMAGE, result ->commands.data(), result ->commands.size());
				}

				if (!result ->data.empty())
				{
					msg (E_INFO, E_DEBUG, "Adding data: %d", result ->data.size());
					MMU() ->ReadSection (SEC_DATA_IMAGE, result ->data.data(), result ->data.size());
				}

				if (!result ->bytepool.empty())
				{
					msg (E_INFO, E_DEBUG, "Adding bytepool data: %d bytes", result ->bytepool.size());
					MMU() ->ReadSection (SEC_BYTEPOOL_IMAGE, result ->bytepool.data(), result ->bytepool.size());
				}

			}

			msg (E_INFO, E_DEBUG, "Stream decode completed - committing symbols");
			Linker() ->DirectLink_Commit();

			break;

		} // stream file

		case FT_NON_UNIFORM:
			__asshole ("Not implemented");
			break;

		default:
			__asshole ("Switch error");
			break;
		} // switch (file_type)

		msg (E_INFO, E_VERBOSE, "Loading completed");
		Reader() ->RdReset();
	}

	void ProcessorAPI::Dump (FILE* file)
	{
		verify_method;

		msg (E_INFO, E_VERBOSE, "Writing context to stream (ctx %zu)", MMU() ->GetContext().buffer);
		__assert (Writer(), "Writer module is not attached");

		Writer() ->WrSetup (file);
		Writer() ->Write (MMU() ->GetContext().buffer);
		Writer() ->WrReset();
	}

	void ProcessorAPI::Compile()
	{
		verify_method;

		try
		{
			msg (E_INFO, E_VERBOSE, "Attempting to compile context %zu", MMU() ->GetContext().buffer);
			__assert (Backend(), "Backend is not attached");

			size_t chk = LogicProvider() ->ChecksumState();

			Backend() ->CompileBuffer (chk, &InterpreterCallbackFunction);
			__assert (Backend() ->ImageIsOK (chk), "Backend reported compile error");

			msg (E_INFO, E_VERBOSE, "Compilation OK: checksum assigned %p", chk);
		}

		catch (std::exception& e)
		{
			msg (E_CRITICAL, E_USER, "Compilation FAILED: %s", e.what());
		}
	}

	calc_t ProcessorAPI::Exec()
	{
		verify_method;

		Context& interpreter_context = MMU() ->GetContext();
		size_t chk = LogicProvider() ->ChecksumState();

		msg (E_INFO, E_VERBOSE, "Starting execution of context %zu (system checksum %p)",
			 interpreter_context.buffer, chk);

		// Try to use backend if image was compiled
		if (Backend() && Backend() ->ImageIsOK (chk))
		{
			msg (E_INFO, E_VERBOSE, "Backend reports image is OK. Using precompiled image");

			try
			{
				SetCallbackProcAPI (this);

				void* address = Backend() ->GetImage (chk);
				calc_t result = ExecuteGate (address);

				SetCallbackProcAPI (0);

				msg (E_INFO, E_VERBOSE, "Native code execution COMPLETED.");
				return result;
			}

			catch (std::exception& e)
			{
				msg (E_CRITICAL, E_USER, "Execution FAILED: Error = \"%s\". Reverting to interpreter", e.what());
			}
		}

		// Else fall back to the interpreter.
		msg (E_INFO, E_VERBOSE, "Using interpreter");

		while (! (interpreter_context.flags & MASK (F_EXIT)))
		{
			Command& command = MMU() ->ACommand (interpreter_context.ip);

			try
			{
				LogicProvider() ->ExecuteSingleCommand (command);
			}

			catch (std::exception& e)
			{

				msg (E_WARNING, E_USER, "Last executed command: %s", LogicProvider() ->DumpCommand (command));

				const char* ctx_dump, *reg_dump, *stack_dump;
				MMU() ->DumpContext (&ctx_dump, &reg_dump, &stack_dump);

				msg (E_WARNING, E_USER, "MMU context dump:");
				msg (E_WARNING, E_USER, "%s", ctx_dump);
				msg (E_WARNING, E_USER, "%s", reg_dump);
				msg (E_WARNING, E_USER, "%s", stack_dump);

				throw;
			}
		} // interpreter loop

		calc_t result;

		// Interpreter return value is in stack of last command.
		size_t last_command_stack_top = 0;
		MMU() ->QueryActiveStack (0, &last_command_stack_top);

		if (last_command_stack_top)
			result = MMU() ->AStackTop (0);

		msg (E_INFO, E_VERBOSE, "Interpreter COMPLETED.");
		return result;
	}

	void ProcessorAPI::SetCallbackProcAPI (ProcessorAPI* procapi)
	{
		callback_procapi = procapi;
	}

	abiret_t ProcessorAPI::InterpreterCallbackFunction (Command* cmd)
	{
		ILogic* logic = callback_procapi ->LogicProvider();
		logic ->ExecuteSingleCommand (*cmd);

		// stack is still set to the last type
		calc_t temporary_result = logic ->StackPop();
		return temporary_result.GetABI();
	}
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
