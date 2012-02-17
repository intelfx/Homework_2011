#include "stdafx.h"
#include "Interfaces.h"

namespace Processor
{
	void ProcessorAPI::Flush()
	{
		verify_method;

		mmu_ ->ResetEverything();
		cset_ ->ResetCommandSet();
		for (unsigned i = 0; i <= Value::V_MAX; ++i)
			executors_[i] ->ResetImplementations();
	}

	void ProcessorAPI::Reset()
	{
		verify_method;

		mmu_ ->ClearContext();
	}

	void ProcessorAPI::Clear()
	{
		verify_method;

		mmu_ ->ResetBuffers (mmu_ ->GetContext().buffer);
	}

	void ProcessorAPI::Delete()
	{
		verify_method;

		mmu_ ->RestoreContext();
	}

	void ProcessorAPI::Load (FILE* file)
	{
		verify_method;

		mmu_ ->AllocContextBuffer();
		msg (E_INFO, E_VERBOSE, "Loading from stream -> context %zu", mmu_ ->GetContext().buffer);

		__assert (reader_, "Loader module is not attached");

		FileProperties rd_prop = reader_ ->RdSetup (file);
		FileSectionType sec_type = FileSectionType::SEC_MAX;
		size_t sec_size, read_size, req_bytes;

		while (reader_ ->NextSection (&rd_prop, &sec_type, &sec_size, &req_bytes))
		{
			__assert (sec_type < SEC_MAX, "Invalid section type");

			msg (E_INFO, E_DEBUG, "Loading section: type \"%s\" size %zu",
				 ProcDebug::FileSectionType_ids[sec_type], sec_size);

			// Read uniform section as a whole image
			if (sec_type != SEC_NON_UNIFORM)
			{
				__assert (sec_size, "Section size is zero");
				__assert (req_bytes, "Required length is zero");

				char* data_buffer = reinterpret_cast<char*> (malloc (req_bytes));
				__assert (data_buffer, "Unable to malloc() read buffer");

				read_size = reader_ ->ReadSectionImage (&rd_prop, data_buffer, req_bytes);
				__assert (read_size == sec_size, "Failed to read section: received %zu elements of %zu",
						  read_size, sec_size);

				msg (E_INFO, E_DEBUG, "Passing image to MMU");

				switch (sec_type)
				{
				case SEC_CODE_IMAGE:
					mmu_ ->ReadText (reinterpret_cast<Command*> (data_buffer), read_size);
					break;

				case SEC_DATA_IMAGE:
					mmu_ ->ReadData (reinterpret_cast<calc_t*> (data_buffer), read_size);
					break;

				case SEC_STACK_IMAGE:
					mmu_ ->ReadStack (reinterpret_cast<calc_t*> (data_buffer), read_size, 0);
					break;

				case SEC_SYMBOL_MAP:
					mmu_ ->ReadSyms (data_buffer, read_size);
					break;

				case SEC_NON_UNIFORM:
				case SEC_MAX:
				default:
					__asshole ("Switch error");
				} // switch (section type)

				free (data_buffer);
				data_buffer = 0;

			} // uniform-type section

			else /* non-uniform section */
			{
				DecodeResult decode_result;

				linker_ ->InitLinkSession();

				msg (E_INFO, E_DEBUG, "Non-uniform section decode start");

				while (reader_ ->ReadStream (&rd_prop, &decode_result))
				{
					if (!decode_result.mentioned_symbols.empty())
					{
						msg (E_INFO, E_DEBUG, "Adding symbols");
						linker_ ->LinkSymbols (decode_result);
					}

					switch (decode_result.type)
					{
					case DecodeResult::DEC_COMMAND:
						msg (E_INFO, E_DEBUG, "Decode completed - Adding command");
						mmu_ ->InsertText (decode_result.command);
						break;

					case DecodeResult::DEC_DATA:
						msg (E_INFO, E_DEBUG, "Decode completed - Adding data");
						mmu_ ->InsertData (decode_result.data);
						break;

					case DecodeResult::DEC_NOTHING:
						msg (E_WARNING, E_DEBUG,
							 "Reader returned no primary data in decoded set");
						break;

					default:
						__asshole ("Switch error");
						break;
					}
				}

				msg (E_INFO, E_DEBUG, "Non-uniform section decode OK, linking phase");
				linker_ ->Finalize();

			} // non-uniform section

		} // while (nextsection)

		msg (E_INFO, E_VERBOSE, "Loading completed");
		reader_ ->RdReset (&rd_prop);
	}

	void ProcessorAPI::Dump (FILE* file)
	{
		verify_method;

		msg (E_INFO, E_VERBOSE, "Writing state to stream (ctx %zu)", mmu_ ->GetContext().buffer);
		__assert (writer_, "Writer module is not attached");

		writer_ ->WrSetup (file);
		writer_ ->Write (mmu_ ->GetContext().buffer);
		writer_ ->WrReset();
	}

	void ProcessorAPI::Compile()
	{
		verify_method;

		try
		{
			msg (E_INFO, E_VERBOSE, "Attempting to compile context %zu", mmu_ ->GetContext().buffer);
			__assert (backend_, "Backend is not attached");

			mmu_ ->ClearContext(); // reset all fields since we (hopefully) won't use interpreter on this context
			size_t chk = internal_logic_ ->ChecksumState(); // checksum the system state right after the cleanup

			backend_ ->CompileBuffer (chk, &InterpreterCallbackFunction);
			__assert (backend_ ->ImageIsOK (chk), "Backend reported compile error");

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

		size_t initial_ctx = mmu_ ->GetContext().buffer;
		size_t chk = internal_logic_ ->ChecksumState();

		msg (E_INFO, E_VERBOSE, "Starting execution of context %zu (system checksum %p)", initial_ctx, chk);

		// Try to use backend if image was compiled
		if (backend_ && backend_ ->ImageIsOK (chk))
		{
			msg (E_INFO, E_VERBOSE, "Backend reports image is OK. Using precompiled image");

			try
			{
				SetCallbackProcAPI (this);

				void* address = backend_ ->GetImage (chk);
				calc_t result = ExecuteGate (address);

				SetCallbackProcAPI (0);

				ProcDebug::PrintValue (result);
				msg (E_INFO, E_VERBOSE, "Execution OK: Result = %s", ProcDebug::debug_buffer);
				mmu_ ->RestoreContext();
				return result;
			}

			catch (std::exception& e)
			{
				msg (E_CRITICAL, E_USER, "Execution FAILED: Error = %s. Reverting to interpreter", e.what());
			}
		}

		// Else fall back to the interpreter.
		msg (E_INFO, E_VERBOSE, "Using interpreter");
		calc_t last_result;

		for (;;)
		{
			Command& command = mmu_ ->ACommand();

			try
			{

				internal_logic_ ->ExecuteSingleCommand (command);
				Context& command_context = mmu_ ->GetContext();

				if (command_context.flags & MASK (F_EXIT))
				{
					if (command_context.buffer == initial_ctx)
						break;

					mmu_ ->RestoreContext();
				}

				else if (! (command_context.flags & MASK (F_WAS_JUMP)))
				{
					++command_context.ip;
				}

			}

			catch (std::exception& e)
			{
				const CommandTraits& cmd_traits = cset_ ->DecodeCommand (command.id);
				msg (E_WARNING, E_USER, "Last executed command [PC=%zu] \"%s\" (%s) argument \"%s\"",
					 mmu_ ->GetContext().ip, cmd_traits.mnemonic, ProcDebug::ValueType_ids[command.type],
					 (ProcDebug::PrintArgument (cmd_traits.arg_type, command.arg, mmu_), ProcDebug::debug_buffer));

				throw;
			}

		} // for (interpreter)

		msg (E_INFO, E_VERBOSE, "Interpreter COMPLETED.");

		if (mmu_ ->GetStackTop())
			last_result = mmu_ ->AStackTop (0);

		mmu_ ->ClearContext();

		return last_result;
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
