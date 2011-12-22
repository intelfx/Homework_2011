#include "stdafx.h"
#include "AssemblyIO.h"

ImplementDescriptor (AsmHandler, "assembly reader/writer", MOD_APPMODULE);

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

	Reference AsmHandler::ParseReference (Processor::symbol_map& symbols, const char* arg)
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

	char* AsmHandler::PrepLine(char* read_buffer)
	{
		char *begin, *tmp = read_buffer;

		while (isspace (* (begin = tmp++)));

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

		}
		while (*tmp++);

		return begin;
	}

	calc_t AsmHandler::ParseValue (const char* input)
	{
		char* endptr;
		long double decoded_a = strtold (input, &endptr);

		__verify (endptr != input, "Malformed value argument: \"%s\"", input);

		int classification = fpclassify (decoded_a);
		__verify (classification == FP_NORMAL || classification == FP_ZERO,
				  "Invalid floating-point value: \"%s\" -> %llg", input, decoded_a);

		return static_cast<calc_t> (decoded_a);
	}

	bool AsmHandler::ReadSingleDeclaration (const char* input, DecodeResult& output)
	{
		char name[STATIC_LENGTH], init[STATIC_LENGTH], type;
		Symbol declaration;
		init (declaration);

		declaration.is_resolved = 1;

		int arguments = sscanf (input, "decl %s %c %s", name, &type, init);
		switch (arguments)
		{
			case 3:
				switch (type)
				{
					case '=':
						output.data = ParseValue (init);

						declaration.ref.is_symbol = 0;
						declaration.ref.direct.type = S_DATA;
						declaration.ref.direct.address = ILinker::symbol_auto_placement_addr;

						msg (E_INFO, E_DEBUG, "Declaration: DATA entry, initialiser <%lg>", output.data);

						output.type = DecodeResult::DEC_DATA;
						break;

					case ':':
						declaration.ref = ParseReference (output.mentioned_symbols, init);

						ProcDebug::PrintReference (declaration.ref);
						msg (E_INFO, E_DEBUG, "Declaration: alias to %s", ProcDebug::debug_buffer);

						output.type = DecodeResult::DEC_NOTHING;
						break;

					default:
						__asshole ("Parse error: \"%s\"", input);
						break;
				}

				output.mentioned_symbols.insert (PrepareSymbol (name, declaration));
				break;

			case 1:
				output.data = 0; // default initialiser

				declaration.ref.is_symbol = 0;
				declaration.ref.direct.type = S_DATA;
				declaration.ref.direct.address = ILinker::symbol_auto_placement_addr;

				msg (E_INFO, E_DEBUG, "Declaration: DATA entry, default initialiser");

				output.type = DecodeResult::DEC_DATA;
				output.mentioned_symbols.insert (PrepareSymbol (name, declaration));
				break;

			case 2:
				__asshole ("Parse error: \"%s\"", input);
				break;

			case EOF:
			case 0:
				msg (E_INFO, E_DEBUG, "Declaration: no declaration");
				break;

			default:
				__asshole ("Switch error");
				break;
		}

		return (arguments > 0);
	}

	bool AsmHandler::ReadSingleCommand (size_t line_num,
										const char* input,
										DecodeResult& output)
	{
		char command[STATIC_LENGTH], arg[STATIC_LENGTH];

		int arguments = sscanf (input, "%s %s", command, arg);
		switch (arguments)
		{
		case 1: /* no argument */
		{
			const CommandTraits& desc = proc_ ->CommandSet() ->DecodeCommand (command);
			__verify (desc.arg_type == A_NONE,
					  "Line %zu: No argument required for command \"%s\" while given argument \"%s\"",
					  line_num, command, arg);

			output.type = DecodeResult::DEC_COMMAND;
			output.command.id = desc.id;

			msg (E_INFO, E_DEBUG, "Command: \"%s\" -> [%hu]", desc.mnemonic, desc.id);
			break;
		}

		case 2: /* single argument */
		{
			const CommandTraits& desc = proc_ ->CommandSet() ->DecodeCommand (command);
			__verify (desc.arg_type != A_NONE,
					  "Line %zu: Argument needed for command \"%s\"", line_num, command);

			switch (desc.arg_type)
			{
			case A_REFERENCE:
			{
				output.command.arg.ref = ParseReference (output.mentioned_symbols, arg);

				ProcDebug::PrintReference (output.command.arg.ref);
				msg (E_INFO, E_DEBUG, "Command: \"%s\" -> [%hu] reference to %s",
					 desc.mnemonic, desc.id, ProcDebug::debug_buffer);

				break;
			}

			case A_VALUE:
			{
				output.command.arg.value = ParseValue (arg);
				msg (E_INFO, E_DEBUG, "Command: \"%s\" -> [%hu] argument %lg",
					 desc.mnemonic, desc.id, output.command.arg.value);

				break;
			}

			case A_NONE:
				msg (E_INFO, E_DEBUG, "Command: \"%s\" -> [%hu]", desc.mnemonic, desc.id);
				break;

			default:
				__asshole ("Switch error");
				break;
			} // switch (argument type)

			output.type = DecodeResult::DEC_COMMAND;
			output.command.id = desc.id;

			break;
		} // argument processing

		case EOF:
		case 0:
			msg (E_INFO, E_DEBUG, "Command: No command");
			break;

		default:
			__asshole ("Switch error");
			break;
		} // switch (argument count)

		return (arguments > 0);
	}

	char* AsmHandler::ParseLabel (char* string)
	{
		do
		{
			if (*string == ' ' || *string == '\0')
				return 0; // no label

			if (*string == ':')
			{
				*string = '\0';
				return string + 1; // found label
			}
		} while (*string++);

		return 0;
	}

	void AsmHandler::ReadSingleStatement (size_t line_num, char* input, DecodeResult& output)
	{
		verify_method;
		size_t labels = 0;
		Symbol label_symbol;
		char* current_position = PrepLine (input);

		init (label_symbol);

		/* skip leading space and return if empty string */
		if (!current_position)
		{
			msg (E_INFO, E_DEBUG, "Not decoding empty line");
			return;
		}

		msg (E_INFO, E_DEBUG, "Decoding line %zu: \"%s\"", line_num, current_position);

		/*
		 * parse labels
		 * while() condition is a mess.
		 * Don't touch, it is supposed to work.
		 */

		while (char* next_chunk = ParseLabel (current_position))
		{
			label_symbol.is_resolved = 1; /* defined here */
			label_symbol.ref.is_symbol = 0; /* it does not reference another symbol */
			label_symbol.ref.direct.type = S_CODE; /* it points to text */
			label_symbol.ref.direct.address = ILinker::symbol_auto_placement_addr; /* its address must be determined later */

			output.mentioned_symbols.insert (PrepareSymbol (current_position, label_symbol));

			msg (E_INFO, E_DEBUG, "Label: \"%s\"", current_position);

			++labels;

			current_position = next_chunk;
			while (isspace (*current_position)) ++current_position;
		}
		msg (E_INFO, E_DEBUG, "Labels: %zu", labels);


		if (ReadSingleDeclaration (current_position, output)) return;
		if (ReadSingleCommand (line_num, current_position, output)) return;
	}

	size_t AsmHandler::ReadStream (FileProperties* prop, DecodeResult* destination)
	{
		__assert (prop, "Invalid properties");
		__assert (destination, "Invalid destination");
		__assert (prop ->file_description.front().first == SEC_NON_UNIFORM,
				  "Invalid (non-stream) section type: \"%s\"",
				  ProcDebug::FileSectionType_ids[prop ->file_description.front().first]);

		*destination = DecodeResult();
		char read_buffer[STATIC_LENGTH];

		do
		{
			if (!fgets (read_buffer, STATIC_LENGTH, prop ->file))
				return 0;

			ReadSingleStatement (prop ->file_description.front().second++, read_buffer, *destination);

		} while (destination ->type == DecodeResult::DEC_NOTHING);

		return 1;
	}
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
