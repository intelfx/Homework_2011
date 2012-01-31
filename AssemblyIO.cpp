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
									size_t*,
									size_t*)
	{
		verify_method;

		__assert (prop, "Invalid properties pointer");
		auto cur_section = prop ->file_description.front();

		if (cur_section.first != SEC_MAX)
			return 0; /* we always have 2 sections, with first being a stub (SEC_MAX) */

		prop ->file_description.pop_front();

		if (type)
			*type = prop ->file_description.front().first;

		return 1;
	}

	FileProperties AsmHandler::RdSetup (FILE* file)
	{
		__assert (file, "Invalid reading file");
		__assert (!ferror (file), "Error in stream");

		FileProperties fp;
		fp.file = file;
		fp.file_description.push_back (std::make_pair (SEC_MAX, 0));
		fp.file_description.push_back (std::make_pair (SEC_NON_UNIFORM, 0));
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

	AddrType AsmHandler::ReadReferenceSpecifier (char id)
	{
		switch (id)
		{
		case 'c':
			return S_CODE;
			break;

		case 'd':
			return S_DATA;
			break;

		case 'f':
			return S_FRAME;
			break;

		case 'p':
			return S_FRAME_BACK;
			break;

		case 'r':
			return S_REGISTER;
			break;

		default:
			__asshole ("Invalid plain address specifier: '%c'", id);
		}

		return S_NONE; /* for GCC not to complain */
	}

	Reference AsmHandler::ParseReference (Processor::symbol_map& symbols, char* arg)
	{
		verify_method;

		char id = 0;
		size_t address = 0, hash = 0;
		char* relative_ptr = 0;
		size_t relative_address = 0;
		Reference result;
		init (result);

		msg (E_INFO, E_DEBUG, "Parsing reference \"%s\"", arg);

		if (char* plus = strchr (arg, '+'))
		{
			relative_ptr = plus + 1;

			errno = 0;
			relative_address = Value::IntParse (relative_ptr);

			*plus = '\0';
		}

		if ((id = arg[0]) &&
			(arg[1] == ':')) /* uniform address reference */
		{
			__verify (!relative_ptr, "Relative address is not allowed with address reference in \"%s\"", arg);
			relative_ptr = arg + 2;

			result.type = Reference::RT_DIRECT;
			result.plain.type = ReadReferenceSpecifier (id);
			result.plain.address = Value::IntParse (relative_ptr);

			if (result.plain.type == S_REGISTER)
				__assert (result.plain.address < R_MAX, "Invalid register ID %zu in reference \"%s\"", address, arg);
		}

		else if (arg[0] == '$') /* symbolic reference to register */
		{
			__verify (!relative_ptr, "Relative address is not allowed with register reference in \"%s\"", arg);

			result.type = Reference::RT_DIRECT;
			result.plain.type = S_REGISTER;
			result.plain.address = proc_ ->LogicProvider() ->DecodeRegister (arg + 1);
		}

		else if (arg[0] == '"') /* string is a kind of reference */
		{
			std::string output_string;

			char *input_ptr = arg + 1, current, output;
			while ((current = *input_ptr++) != '"')
			{
				if (current == '\\') // escape-sequence
					switch (current = *input_ptr++)
					{
					case '\\': // backward slash
					case '\'': // single-quote
					case '\"': // double-quote
					case '\0': // NUL
						output = current;
						break;

					case 't': // Horizontal tabulator
						output = '\t';
						break;

					case 'v': // Vertical tabulator
						output = '\v';
						break;

					case 'n': // Newline
						output = '\n';
						break;

					case 'r': // Carriage return
						output = '\r';
						break;

					case 'f': // Form-feed
						output = '\f';
						break;

					case 'a': // Audible bell
						output = '\a';
						break;

					case 'b': // Backspace
						output = '\b';
						break;

					case 'x': // Hexadecimal
						output = strtol (input_ptr, &input_ptr, 16);
						break;

					default: // Octal
						output = strtol (input_ptr, &input_ptr, 8);
						break;
					} // escape sequence parse

				else
					output = current;

				// Commit the char
				output_string.push_back (output);
			} // while not closing double-apostrophe

			output_string.push_back ('\0');

			msg (E_INFO, E_DEBUG, "Adding string \"%s\" (length %zu)",
				 output_string.c_str(), output_string.size());

			// Insert the reference to the MMU
			size_t current_pointer = proc_ ->MMU() ->GetPoolSize();
			proc_ ->MMU() ->SetBytes (current_pointer, output_string.size() + 1, output_string.c_str());

			result.type = Reference::RT_DIRECT;
			result.plain.type = S_BYTEPOOL;
			result.plain.address = current_pointer + relative_address;
		}

		else if (sscanf (arg, "@%c", &id) == 1) /* uniform indirect reference */
		{
			result.type = Reference::RT_INDIRECT;
			result.plain.type = ReadReferenceSpecifier (id);
			result.plain.address = relative_address;
		}

		else /* reference to symbol */
		{
			Symbol referenced_symbol;
			init (referenced_symbol);

			referenced_symbol.is_resolved = 0; /* symbol is not defined here. */
			symbols.insert (PrepareSymbol (arg, referenced_symbol, &hash)); /* add symbols into mentioned */

			result.type = Reference::RT_SYMBOL;
			result.symbol.hash = hash;
			result.symbol.offset = relative_address;
		}

		return result;
	}

	char* AsmHandler::PrepLine (char* read_buffer)
	{
		char *begin, *tmp = read_buffer, *last_non_whitespace;

		while (isspace (*(begin = tmp++)));

		if (*begin == '\0')
			return 0;

		tmp = begin;
		last_non_whitespace = begin;

		/* drop unused parts of decoded string */
		do
		{
			if (*tmp == ';')
				*tmp = '\0';

			else if (*tmp == '\n')
				*tmp = '\0';

			else
			{
				if (!isspace (*tmp))
					last_non_whitespace = tmp;

				*tmp = tolower (*tmp);
			}

		}
		while (*tmp++);

		*(last_non_whitespace + 1) = '\0'; // cut traling whitespace

		return begin;
	}

	void AsmHandler::ReadSingleDeclaration (const char* decl_data, DecodeResult& output)
	{
		char name[STATIC_LENGTH], initialiser[STATIC_LENGTH], type;

		// Declaration is a symbol itself.
		Symbol declaration_symbol;
		init (declaration_symbol);
		declaration_symbol.is_resolved = 1;

		// we may get an unnamed declaration
		if (decl_data[0] == '=')
		{
			output.data.Parse (decl_data + 1); // type of value is already set

			msg (E_INFO, E_DEBUG, "Declaration: unnamed DATA entry = %s", (ProcDebug::PrintValue (output.data), ProcDebug::debug_buffer));

			return;
		}

		int arguments = sscanf (decl_data, "%[^:= ] %c %s", name, &type, initialiser);
		switch (arguments)
		{
		case 3:
			switch (type)
			{
			case '=':
				output.data.Parse (initialiser); // type of value is already set

				declaration_symbol.ref.type = Reference::RT_DIRECT;
				declaration_symbol.ref.plain.type = S_DATA;
				declaration_symbol.ref.plain.address = ILinker::symbol_auto_placement_addr;

				msg (E_INFO, E_DEBUG, "Declaration: DATA entry \"%s\" = %s", name, (ProcDebug::PrintValue (output.data), ProcDebug::debug_buffer));

				break;

			case ':':
				declaration_symbol.ref = ParseReference (output.mentioned_symbols, initialiser);

				ProcDebug::PrintReference (declaration_symbol.ref);
				msg (E_INFO, E_DEBUG, "Declaration: alias \"%s\" to %s", name, (ProcDebug::PrintValue (output.data), ProcDebug::debug_buffer));

				output.type = DecodeResult::DEC_NOTHING; // in this case declaration is not a declaration itself
				break;

			default:
				__asshole ("Parse error: \"%s\"", decl_data);
				break;
			}

			break;

		case 1:
			output.data = Value(); // default initialiser

			declaration_symbol.ref.type = Reference::RT_DIRECT;
			declaration_symbol.ref.plain.type = S_DATA;
			declaration_symbol.ref.plain.address = ILinker::symbol_auto_placement_addr;

			msg (E_INFO, E_DEBUG, "Declaration: DATA entry uninitialised");
			break;

		default:
			__asshole ("Parse error: \"%s\" - sscanf() says \"%d\"", decl_data, arguments);
			break;
		}

		output.mentioned_symbols.insert (PrepareSymbol (name, declaration_symbol));
	}

	void AsmHandler::ReadSingleCommand (const char* command,
										char* argument,
										Processor::DecodeResult& output)
	{
		const CommandTraits& desc = proc_ ->CommandSet() ->DecodeCommand (command);
		output.command.id = desc.id;

		// Default command type specification
		if (output.command.type == Value::V_MAX)
		{
			if (desc.is_service_command)
				output.command.type = Value::V_INTEGER;

			else
				output.command.type = Value::V_FLOAT;
		}

		if (!argument)
		{
			__verify (desc.arg_type == A_NONE,
					  "No argument required for command \"%s\" while given argument \"%s\"",
					  command, argument);

			msg (E_INFO, E_DEBUG, "Command: \"%s\" no argument", desc.mnemonic);
		}

		else /* have argument */
		{
			__verify (desc.arg_type != A_NONE,
					  "Argument needed for command \"%s\"", command);

			switch (desc.arg_type)
			{
			case A_REFERENCE:
			{
				output.command.arg.ref = ParseReference (output.mentioned_symbols, argument);

				msg (E_INFO, E_DEBUG, "Command: \"%s\" reference to %s",
					 desc.mnemonic, (ProcDebug::PrintReference (output.command.arg.ref), ProcDebug::debug_buffer));

				break;
			}

			case A_VALUE:
			{
				output.command.arg.value.type = output.command.type;
				output.command.arg.value.Parse (argument);

				msg (E_INFO, E_DEBUG, "Command: \"%s\" argument %s",
					 desc.mnemonic, (ProcDebug::PrintValue (output.command.arg.value), ProcDebug::debug_buffer));

				break;
			}

			case A_NONE:
			default:
				__asshole ("Switch error");
				break;
			} // switch (argument type)

		} // have argument
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
		}
		while (*string++);

		return 0;
	}

	void AsmHandler::ReadSingleStatement (size_t line_num, char* input, DecodeResult& output)
	{
		verify_method;
		size_t labels = 0;
		Symbol label_symbol;
		init (label_symbol);

		Value::Type statement_type = Value::V_MAX;
		char *command, *argument, typespec = 0, *current_position = PrepLine (input);


		/* skip leading space and return if empty string */

		if (!current_position)
		{
			msg (E_INFO, E_DEBUG, "Decoding line %zu: empty line", line_num);
			return;
		}

		msg (E_INFO, E_DEBUG, "Decoding line %zu: \"%s\"", line_num, current_position);


		/* parse labels */

		while (char* next_chunk = ParseLabel (current_position))
		{
			label_symbol.is_resolved = 1; /* defined here */
			label_symbol.ref.type = Reference::RT_DIRECT; /* it points directly here */
			label_symbol.ref.plain.type = S_CODE; /* it points to text */
			label_symbol.ref.plain.address = ILinker::symbol_auto_placement_addr; /* its address must be determined later */

			output.mentioned_symbols.insert (PrepareSymbol (current_position, label_symbol));

			msg (E_INFO, E_DEBUG, "Label: \"%s\"", current_position);

			++labels;

			current_position = next_chunk;
			while (isspace (*current_position)) ++current_position;
		}
		msg (E_INFO, E_DEBUG, "Labels: %zu", labels);


		/* get remaining statement */

		command = strtok (current_position, " \t");
		argument = strtok (0, "");

		if (!command)
			return; // no command

		/* parse explicit type-specifier */

		if (char* dot = strchr (command, '.'))
		{
			*dot++ = '\0';
			typespec = *dot;
		}

		/* decode type-specifier */

		switch (tolower (typespec))
		{
		case 'f':
			statement_type = Value::V_FLOAT;
			break;

		case 'd':
		case 'i':
			statement_type = Value::V_INTEGER;
			break;

		case '\0':
			statement_type = Value::V_MAX;
			break;

		default:
			__asshole ("Invalid command type specification: '%c'", typespec);
			break;
		}

		/* determine if we have a declaration or a command and parse accordingly */

		if (!strcmp (command, "decl"))
		{
			__verify (argument, "Empty declaration");

			output.type = DecodeResult::DEC_DATA;

			if (statement_type != '\0')
				output.data.type = statement_type;

			else
				output.data.type = Value::V_FLOAT;

			ReadSingleDeclaration (argument, output);
		}

		else
		{
			output.type = DecodeResult::DEC_COMMAND;
			output.command.type = statement_type;

			// in case of undefined type it will be set in ReadSingleCommand()
			// depending on command flavor (service/normal).

			ReadSingleCommand (command, argument, output);
		}
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

			ReadSingleStatement (++prop ->file_description.front().second, read_buffer, *destination);

		}
		while (destination ->type == DecodeResult::DEC_NOTHING);

		return 1;
	}
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
