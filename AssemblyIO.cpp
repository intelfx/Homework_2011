#include "stdafx.h"
#include "AssemblyIO.h"

ImplementDescriptor(AsmHandler, "assembly reader/writer", MOD_APPMODULE, ProcessorImplementation::AsmHandler);

namespace ProcessorImplementation
{
	using namespace Processor;

	AsmHandler::AsmHandler() :
	writing_file_ (0)
	{
	}

	bool AsmHandler::_Verify() const
	{
		if (writing_file_)
			verify_statement (!ferror (writing_file_), "Error in writing stream");
		return 1;
	}

	void AsmHandler::WrReset()
	{
		verify_method;

		msg (E_INFO, E_DEBUG, "Resetting writing file");
		fclose (writing_file_);
		writing_file_ = 0;
	}

	void AsmHandler::WrSetup (FILE* file)
	{
		writing_file_ = file;
		verify_method;

		msg (E_INFO, E_DEBUG, "Writer set up");
	}

	void AsmHandler::Write (size_t ctx_id)
	{
		verify_method;
		__assert (writing_file_, "Writer has not been set up");

		msg (E_INFO, E_VERBOSE, "Beginning ASM write of context %zu", ctx_id);

		proc_ ->MMU() ->SetTemporaryContext (ctx_id);
		InternalWriteFile();
		proc_ ->MMU() ->RestoreContext();
	}

	void AsmHandler::InternalWriteFile()
	{
		__asshole ("Not implemented");
	}

	size_t AsmHandler::NextSection (Processor::FileProperties* prop,
									Processor::FileSectionType* type,
									size_t* count,
									size_t*,
									bool)
	{
		verify_method;

		__assert (prop, "Invalid properties pointer");
		auto cur_section = prop ->file_description.front();

		if (cur_section.first != SEC_MAX)
			return 0; /* we always have 2 sections, with first being a stub (SEC_MAX) */

		prop ->file_description.pop_front();

		if (type)
			*type = prop ->file_description.front().first;

		if (count)
			*count = prop ->file_description.front().second;

		return 1;
	}

	FileProperties AsmHandler::RdSetup (FILE* file)
	{
		__assert (file, "Invalid reading file");
		__assert (!ferror (file), "Error in stream");

		FileProperties fp;
		fp.file = file;
		fp.file_description.push_back (std::make_pair (SEC_MAX, 0));
		fp.file_description.push_back (std::make_pair (SEC_NON_UNIFORM, 1));
		return fp;
	}

	void AsmHandler::RdReset (FileProperties* prop)
	{
		__assert (prop, "Invalid properties");
		__assert (prop ->file, "Invalid file");

		fclose (prop ->file);
		prop ->file = 0;
		prop ->file_description.clear();
	}

	size_t AsmHandler::ReadNextElement (FileProperties*, void*, size_t)
	{
		__asshole ("Not implemented");
		return 0;
	}

	size_t AsmHandler::ReadSectionImage (FileProperties*, void*, size_t)
	{
		__asshole ("Not implemented");
		return 0;
	}

	Reference AsmHandler::ReadSingleReference (Processor::symbol_map& symbols)
	{
		verify_method;

		char id;
		size_t address;
		Reference result;
		init (result);

		if (sscanf (arg, "%c:%zu", &id, &address) == 2)
		{
			result.is_symbol = 0;
			result.direct.address = address;

			switch (id)
			{
				case 'c':
					result.direct.type = S_CODE;
					break;

				case 'd':
					result.direct.type = S_DATA;
					break;

				case 'f':
					result.direct.type = S_FRAME;
					break;

				case 'p':
					result.direct.type = S_FRAME_BACK;
					break;

				case 'r':
					result.direct.type = S_REGISTER;
					__verify (address < R_MAX, "Invalid address in register reference: %zu", address);
					break;

				default:
					result.direct.type = S_NONE;
			}

			__verify (result.direct.type != S_NONE, "Invalid direct address specifier: '%c'", id);
		}

		else if (arg[0] == '$')
		{
			result.is_symbol = 0;
			result.direct.type = S_REGISTER;
			result.direct.address = proc_ ->LogicProvider() ->DecodeRegister (arg + 1);
		}

		else
		{
			Symbol decoded_symbol;
			init (decoded_symbol);

			decoded_symbol.is_resolved = 0; /* symbol is not defined here. */
			symbols.insert (PrepareSymbol (arg, decoded_symbol, &result.symbol_hash)); /* add symbols into mentioned and write hash to reference */
		}

		return result;
	}

	char* AsmHandler::PrepLine()
	{
		char *begin, *tmp = read_buffer;

		while (isspace (*(begin = tmp++)));

		if (*begin == '\0')
			return 0;

		tmp = begin;

		/* drop unused parts of decoded string */
		do
		{
			if (*tmp == ';')
				*tmp = '\0';

			else if (*tmp == '\n')
				*tmp = '\0';

			else
				*tmp = tolower (*tmp);

		} while (*tmp++);

		return begin;
	}

	std::pair<bool, DecodeResult> AsmHandler::ReadSingleStatement (size_t line_num)
	{
		verify_method;

		std::pair<bool, DecodeResult> result;
		size_t labels = 0;
		Symbol current_label;
		char* tmp_reading = PrepLine();

		init (current_label);
		result.first = 0;

		/* skip leading space and return if empty string */
		if (!tmp_reading)
		{
			msg (E_INFO, E_DEBUG, "Not decoding empty line");
			return result;
		}

		msg (E_INFO, E_DEBUG, "Decoding single line: \"%s\"", tmp_reading);

		/* parse labels */
		while (char* colon = strchr (tmp_reading, ':'))
		{
			*colon++ = '\0'; /* set symbol name boundary */
			while (isspace (*tmp_reading)) ++tmp_reading; /* skip leading space */

			current_label.is_resolved = 1; /* defined here */
			current_label.ref.is_symbol = 0; /* it does not reference another symbol */
			current_label.ref.direct.type = S_CODE; /* it points to text */
			current_label.ref.direct.address = ILinker::symbol_auto_placement_addr; /* its address must be determined later */

			result.second.mentioned_symbols.insert (PrepareSymbol (tmp_reading, current_label));

			msg (E_INFO, E_DEBUG, "Label: \"%s\"", tmp_reading);

			tmp_reading = colon;
			++labels;
		}
		msg (E_INFO, E_DEBUG, "Parsed labels: %zu", labels);

		// the only statement type now is instruction
		int arguments = sscanf (tmp_reading, "%s %s", command, arg);
		switch (arguments)
		{
		case 1: /* no argument */
		{
			const CommandTraits& desc = proc_ ->CommandSet() ->DecodeCommand (command);
			__verify (desc.arg_type == A_NONE,
					  "Line %zu: No argument required for command \"%s\" while given argument \"%s\"",
					  line_num, command, arg);

			result.second.command.id = desc.id;

			msg (E_INFO, E_DEBUG, "Command: \"%s\" -> [%zu]", desc.mnemonic, desc.id);
			break;
		}

		case 2: /* single argument */
		{
			const CommandTraits& desc = proc_ ->CommandSet() ->DecodeCommand (command);
			__verify (desc.arg_type != A_NONE,
					  "Line %zu: Argument needed for command \"%s\"", line_num, command);

			result.second.command.id = desc.id;

			switch (desc.arg_type)
			{
			case A_REFERENCE:
			{
				result.second.command.arg.ref = ReadSingleReference (result.second.mentioned_symbols);

				ProcDebug::PrintReference (result.second.command.arg.ref);
				msg (E_INFO, E_DEBUG, "Command: \"%s\" -> [%zu] reference to %s",
					 desc.mnemonic, desc.id, ProcDebug::debug_buffer);

				break;
			}

			case A_VALUE:
			{
				char* endptr;
				long double decoded_a = strtold (arg, &endptr);

				__verify (endptr != arg, "Line %zu: Malformed value argument: \"%s\"", line_num, arg);

				int classification = fpclassify (decoded_a);
				__verify (classification == FP_NORMAL || classification == FP_ZERO,
						  "Line %zu: Invalid floating-point value: \"%s\" -> %lg", line_num, arg, decoded_a);

				result.second.command.arg.value = static_cast<calc_t> (decoded_a);
				msg (E_INFO, E_DEBUG, "Command: \"%s\" -> [%zu] argument %lg",
					 desc.mnemonic, desc.id, decoded_a);

				break;
			}

			case A_NONE:
				msg (E_INFO, E_DEBUG, "Command: \"%s\" -> [%zu]", desc.mnemonic, desc.id);
				break;

			default:
				__asshole ("Switch error");
				break;
			} // switch (argument type)

			break;
		} // argument processing

		case EOF:
		default:
			msg (E_INFO, E_DEBUG, "Command: No command");
			break;
		} // switch (argument count)

		result.first = static_cast<bool> (arguments);
		return std::move (result);
	}

	size_t AsmHandler::ReadStream (FileProperties* prop, DecodeResult* destination)
	{
		__assert (prop, "Invalid properties");
		__assert (destination, "Invalid destination");
		__assert (prop ->file_description.front().first == SEC_NON_UNIFORM,
				  "Invalid (non-stream) section type: \"%s\"",
				  ProcDebug::FileSectionType_ids[prop ->file_description.front().first]);

		std::pair<bool, DecodeResult> result;

		do
		{
			if (feof (prop ->file))
				return 0;

			fgets (read_buffer, line_length, prop ->file);
			result = ReadSingleStatement (prop ->file_description.front().second++);

		} while (!result.first);

		*destination = result.second;
		return 1;
	}
}
// kate: indent-mode cstyle; replace-tabs off; indent-width 4; tab-width 4;
