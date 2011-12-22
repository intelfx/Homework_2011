#include "stdafx.h"
#include "Interfaces.h"

namespace Processor
{
	void ProcessorAPI::Flush()
	{
		verify_method;

		mmu_ ->ResetEverything();
		cset_ ->ResetCommandSet();
		executor_ ->ResetImplementations();
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

	void ProcessorAPI::Load (FILE* file, bool execute_stream)
	{
		verify_method;

		if (!execute_stream)
		{
			mmu_ ->AllocContextBuffer();
			msg (E_INFO, E_VERBOSE, "Loading from stream -> context %zu", mmu_ ->GetContext().buffer);
		}

		else
		{
			msg (E_INFO, E_VERBOSE, "Reading and executing stream in temporary context");
			mmu_ ->SaveContext();
			mmu_ ->ClearContext();
		}

		__assert (reader_, "Reader module is not attached");

		FileProperties rd_prop = reader_ ->RdSetup (file);
		FileSectionType sec_type = FileSectionType::SEC_MAX;
		size_t sec_size, read_size, req_bytes;

		while (reader_ ->NextSection (&rd_prop, &sec_type, &sec_size, &req_bytes))
		{
			__assert (sec_type < SEC_MAX, "Invalid section type");
			__assert (sec_size, "Section size is zero");
			__assert (req_bytes, "Required length is zero");

			msg (E_INFO, E_DEBUG, "Reading section: type \"%s\" size %zu",
				 ProcDebug::FileSectionType_ids[sec_type], sec_size);

			// Read uniform section as a whole image
			if (sec_type != SEC_NON_UNIFORM)
			{
				__assert (!execute_stream, "Uniform (image) section in stream execute mode");

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
						mmu_ ->ReadStack (reinterpret_cast<calc_t*> (data_buffer), read_size);
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

				if (execute_stream)
				{
					size_t initial_ctx_n = mmu_ ->GetContext().buffer;
					size_t module = executor_ ->ID();

					/* while (mmu_ ->DecodeLoopCondition (initial_ctx_n)) */

					for (;;) // since we've got an extra check to exit
					{
						if (reader_ ->ReadStream (&rd_prop, &decode_result))
						{
							__verify (decode_result.mentioned_symbols.empty(),
									  "Symbols are not allowed in EIP mode");

							__verify (decode_result.type == DecodeResult::DEC_COMMAND,
									  "Only commands are allowed in EIP mode");

							void* handle = cset_ ->GetExecutionHandle (decode_result.command.id, module);
							executor_ ->Execute (handle, decode_result.command.arg);
						}

						else
						{
							msg (E_WARNING, E_VERBOSE, "Preliminary EOS reading and executing stream");
							mmu_ ->GetContext().flags |= MASK (F_EXIT);
						}


						if (mmu_ ->GetContext().flags & MASK (F_EXIT)) /* we are marked to exit context, do it */
						{
							if (mmu_ ->GetContext().buffer == initial_ctx_n)
							{
								msg (E_INFO, E_VERBOSE, "Load and execute OK. Result = %lg",
									 internal_logic_ ->StackTop());
								break; /* we leave used context for the user */
							}

							msg (E_INFO, E_DEBUG, "EXIT condition - restoring context");
							mmu_ ->RestoreContext();
						}
					} /* decode loop condition */

				} /* if in in-place execution mode */

				else /* normal decode */
				{
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
								msg (E_INFO, E_DEBUG, "Adding command");
								mmu_ ->InsertText (decode_result.command);
								break;

							case DecodeResult::DEC_DATA:
								msg (E_INFO, E_DEBUG, "Adding data");
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
				}

			} // non-uniform section

		} // while (nextsection)

		msg (E_INFO, E_VERBOSE, "Reading completed");

		reader_ ->RdReset (&rd_prop); // close file
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

			backend_ ->CompileBuffer (chk);
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

		size_t initial_ctx = mmu_ ->GetContext().buffer, now_ctx = initial_ctx;
		size_t chk = internal_logic_ ->ChecksumState(), execid = executor_ ->ID();

		msg (E_INFO, E_VERBOSE, "Starting execution of context %zu (system checksum %p)", initial_ctx, chk);

		// Try to use backend if image was compiled
		if (backend_ && backend_ ->ImageIsOK (chk))
		{
			msg (E_INFO, E_VERBOSE, "Backend reports image is OK. Using precompiled image");

			try
			{
				calc_t result = backend_ ->ExecuteImage (chk);

				msg (E_INFO, E_VERBOSE, "Execution OK: Result = %lg", result);
				mmu_ ->RestoreContext();
				return result;
			}

			catch (std::exception& e)
			{
				msg (E_CRITICAL, E_USER, "Execution FAILED: Error = %s. Reverting to interpreter", e.what());
			}
		}

		// Else fall back to the interpreter.
		msg (E_INFO, E_VERBOSE, "Using interpreter (ID %p)", execid);
		calc_t last_result;
		for (;;)
		{
			Command& now_cmd = mmu_ ->ACommand();

			msg (E_INFO, E_DEBUG, "Executing : [PC=%zu] : \"%s\"",
				 mmu_ ->GetContext().ip, cset_ ->DecodeCommand(now_cmd.id).mnemonic);

			if (mmu_ ->GetContext().ip + 1 < mmu_ ->GetTextSize())
				++mmu_ ->GetContext().ip;

			void* handle = cset_ ->GetExecutionHandle (now_cmd.id, execid);
			__assert (handle, "Invalid handle for command \"%s\"",
					  cset_ ->DecodeCommand (now_cmd.id).mnemonic);

			executor_ ->Execute (handle, now_cmd.arg);
			now_ctx = mmu_ ->GetContext().buffer;

			if (mmu_ ->GetContext().flags & MASK (F_EXIT))
			{
				last_result = internal_logic_ ->StackTop();
				mmu_ ->RestoreContext();

				if (now_ctx == initial_ctx)
					break; /* exit the loop if we're off the initial context */
			}
		} // for (interpreter)

		msg (E_INFO, E_VERBOSE, "Interpreter COMPLETED: Result = %lg", last_result);
		return last_result;
	}
}