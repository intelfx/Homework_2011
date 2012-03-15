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

size_t AsmHandler::NextSection (FileProperties* prop,
								FileSectionType* type,
								size_t*,
								size_t*)
{
	verify_method;

	__assert (prop, "Invalid properties pointer");

	prop ->file_description.pop_front();

	if (prop ->file_description.empty())
		return 0;

	if (type)
		*type = prop ->file_description.front().first;

	return 1;
}

FileProperties AsmHandler::RdSetup (FILE* file)
{
	__assert (file, "Invalid reading file");
	__assert (!ferror (file), "Error in stream");

	FileProperties fp (file);

	fp.file_description.push_back (std::make_pair (SEC_NON_UNIFORM, 0));
	return std::move (fp);
}

void AsmHandler::RdReset (FileProperties*)
{
	// no-op
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

AddrType AsmHandler::DecodeSectionType (char id)
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

	return S_NONE; /* for compiler not to complain */
}

Reference::IndirectRef AsmHandler::ParseIndirectReference (char* arg, symbol_map& symbols)
{
	verify_method;

	Reference::IndirectRef result;
	init (result);

	arg = cut_whitespace (arg);
	msg (E_INFO, E_DEBUG, "Parsing indirect reference \"%s\"", arg);

	if (arg[0] == '$')
	{
		result.section = S_REGISTER;
		result.target = ParseRegisterReference (arg + 1).target;
	}

	else
	{
		// Read section specifier (if present)
		if (arg[1] == ':')
		{
			result.section = DecodeSectionType (arg[0]);
			arg += 2;
		}

		result.target = ParseBaseReference (arg, symbols);
	}

	return result;
}

Reference AsmHandler::ParseFullReference (char* arg, symbol_map& symbols)
{
	verify_method;

	Reference result;
	init (result);

	arg = cut_whitespace (arg);
	msg (E_INFO, E_DEBUG, "Parsing complete reference \"%s\"", arg);

	if (arg[0] == '$')
	{
		result.has_second_component = 0;
		result.global_section = S_REGISTER;
		result.components[0] = ParseRegisterReference (arg + 1);
	}

	else if (arg[0] == '\"')
	{
		result.has_second_component = 0;
		result.global_section = S_BYTEPOOL;
		result.components[0] = ParseInsertString (arg /* no increment */);
	}

	else
	{
		char* second_component = 0;

		// Read section specifier (if present)
		if (arg[1] == ':')
		{
			result.global_section = DecodeSectionType (arg[0]);
			arg += 2;
		}

		// Get pointer to second component (if present)
		if ( (second_component = strchr (arg, '+')))
			* second_component++ = '\0';


		result.components[0] = ParseSingleReference (arg, symbols);

		if (second_component)
		{
			result.has_second_component = 1;
			result.components[1] = ParseSingleReference (second_component, symbols);
		}
	}

	return result;
}

Reference::SingleRef AsmHandler::ParseRegisterReference (char* arg)
{
	Reference::SingleRef result;
	init (result);

	result.is_indirect = 0;
	result.target.is_symbol = 0;
	result.target.memory_address = proc_ ->LogicProvider() ->DecodeRegister (arg);

	return result;
}

Reference::SingleRef AsmHandler::ParseSingleReference (char* arg, symbol_map& symbols)
{
	Reference::SingleRef result;
	init (result);

	arg = cut_whitespace (arg);
	msg (E_INFO, E_DEBUG, "Parsing single reference \"%s\"", arg);

	if (arg[0] != '(') /* direct reference */
	{
		result.is_indirect = 0;
		result.target = ParseBaseReference (arg, symbols);
	}

	else /* indirect reference */
	{
		++arg; // skip opening bracket
		char* closing_bracket = strchr (arg, ')');
		__verify (closing_bracket, "Malformed indirect reference: \"%s\"", arg);
		*closing_bracket = '\0';

		result.is_indirect = 1;
		result.indirect = ParseIndirectReference (arg, symbols);
	}

	return result;
}

Reference::BaseRef AsmHandler::ParseBaseReference (char* arg, symbol_map& symbols)
{
	Reference::BaseRef result;
	init (result);

	arg = cut_whitespace (arg);
	msg (E_INFO, E_DEBUG, "Parsing basic reference \"%s\"", arg);

	errno = 0;
	char* endptr;
	long immediate = strtol (arg, &endptr, 0); // base autodetermine

	if (endptr != arg)
	{
		__verify (!errno, "Invalid address: \"%s\": strtol() says \"%s\"",
				  arg, strerror (errno));

		__verify (*endptr == '\0', "Invalid address: \"%s\": garbage at end-of-input");

		result.is_symbol = 0;
		result.memory_address = immediate;

		msg (E_INFO, E_DEBUG, "Basic reference: immediate \"%lu\"", immediate);
	}

	else
	{
		Symbol referenced_symbol (arg);
		InsertSymbol (referenced_symbol, arg, symbols);

		result.is_symbol = 1;
		result.symbol_hash = referenced_symbol.hash;

		msg (E_INFO, E_DEBUG, "Basic reference: symbol \"%s\" (%p)", arg, referenced_symbol.hash);
	}

	return result;
}

Reference::SingleRef AsmHandler::ParseInsertString (char* arg)
{
	Reference::SingleRef result;
	std::string output_string;

	msg (E_INFO, E_DEBUG, "Parsing string literal %s", arg);

	char* input_ptr = arg + 1, current, output;

	while ( (current = *input_ptr++) != '"')
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

	result.is_indirect = 0;
	result.target.is_symbol = 0;
	result.target.memory_address = current_pointer;
	return result;
}

char* AsmHandler::PrepLine (char* read_buffer)
{
	char* begin, *tmp = read_buffer, *last_non_whitespace;

	while (isspace (* (begin = tmp++)));

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

	* (last_non_whitespace + 1) = '\0'; // cut traling whitespace

	return begin;
}

void AsmHandler::ReadSingleDeclaration (const char* decl_data, DecodeResult& output)
{
	char name[STATIC_LENGTH], initialiser[STATIC_LENGTH], type;
	Reference declaration_reference;
	init (declaration_reference);

	// we may get an unnamed declaration
	if (decl_data[0] == '=')
	{
		++decl_data;

		__assert (output.data.type != Value::V_MAX,
				  "Declaration: unnamed DATA entry = \"%s\": type not set!",
				  decl_data);

		output.data.Parse (decl_data);

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
			__assert (output.data.type != Value::V_MAX,
					  "Declaration: DATA entry \"%s\" = \"%s\": type not set!",
					  name, initialiser);

			// type of value is already set, parse the declaration value
			output.data.Parse (initialiser);

			// Set placeholder reference
			declaration_reference.global_section = S_DATA;
			declaration_reference.needs_linker_placement = 1;

			msg (E_INFO, E_DEBUG, "Declaration: DATA entry \"%s\" = %s",
				 name, (ProcDebug::PrintValue (output.data), ProcDebug::debug_buffer));

			break;

		case ':':
			// in this case declaration is not a declaration itself
			output.type = DecodeResult::DEC_NOTHING;

			// Parse aliased reference
			declaration_reference = ParseFullReference (initialiser, output.mentioned_symbols);

			msg (E_INFO, E_DEBUG, "Declaration: alias \"%s\" to %s",
				 name, (ProcDebug::PrintValue (output.data), ProcDebug::debug_buffer));

			break;

		default:
			__asshole ("Invalid declaration: \"%s\"", decl_data);
			break;
		}

		break;

	case 1:
		// Set placeholder reference
		declaration_reference.global_section = S_DATA;
		declaration_reference.needs_linker_placement = 1;


		// if type is known, initialise to 0 in appropriate type.
		if (output.data.type != Value::V_MAX)
		{
			output.data.Set (Value::V_MAX, 0);
			msg (E_INFO, E_DEBUG, "Declaration: DATA entry \"%s\" uninitialised (set to 0)", name);
		}

		// otherwise, leave value uninitialised
		else
		{
			msg (E_INFO, E_DEBUG, "Declaration: DATA entry \"%s\" uninitialised (untyped)", name);
		}

		break;

	default:
		__asshole ("Invalid declaration: \"%s\"", decl_data);
		break;
	}

	// Create symbol from reference and insert it to the map
	Symbol declaration_symbol (name, declaration_reference);
	InsertSymbol (declaration_symbol, name, output.mentioned_symbols);
}

void AsmHandler::ReadSingleCommand (const char* command,
									char* argument,
									DecodeResult& output)
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

	if (!argument) /* no argument */
	{
		__verify (desc.arg_type == A_NONE,
				  "No argument required for command \"%s\" while given argument \"%s\"",
				  command, argument);
	}

	else /* have argument */
	{
		__verify (desc.arg_type != A_NONE,
				  "Argument needed for command \"%s\"", command);

		switch (desc.arg_type)
		{
		case A_REFERENCE:
			output.command.arg.ref = ParseFullReference (argument, output.mentioned_symbols);
			break;

		case A_VALUE:
			output.command.arg.value.type = output.command.type;
			output.command.arg.value.Parse (argument);
			break;

		case A_NONE:
		default:
			__asshole ("Switch error");
			break;
		} // switch (argument type)

	} // have argument

	msg (E_INFO, E_DEBUG, "Command: \"%s\" argument %s",
		 desc.mnemonic,
		 (ProcDebug::PrintArgument (desc.arg_type, output.command.arg), ProcDebug::debug_buffer));
}

char* AsmHandler::ParseLabel (char* string)
{
	do // TODO rewrite to while()
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

	Value::Type statement_type = Value::V_MAX;
	char* command, *argument, typespec = 0, *current_position = PrepLine (input);


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
		Reference label_reference;
		init (label_reference);

		label_reference.global_section = S_CODE;
		label_reference.needs_linker_placement = 1;

		Symbol label_symbol (current_position, label_reference);
		InsertSymbol (label_symbol, current_position, output.mentioned_symbols);

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

		switch (tolower (*dot))
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
	}

	/* determine if we have a declaration or a command and parse accordingly */

	if (!strcmp (command, "decl"))
	{
		__verify (argument, "Empty declaration");

		output.type = DecodeResult::DEC_DATA;
		output.data.type = statement_type;

		// In case of undefined type it will be set in ReadSingleDeclaration().

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

	size_t& line_num = prop ->file_description.front().second;

	*destination = DecodeResult();
	char read_buffer[STATIC_LENGTH];

	try
	{
		do
		{
			if (!fgets (read_buffer, STATIC_LENGTH, prop ->file))
				return 0;

			ReadSingleStatement (++line_num, read_buffer, *destination);

		}
		while (destination ->type == DecodeResult::DEC_NOTHING);

		return 1;
	}

	catch (Debug::Exception& e)
	{
		switch (e.Type())
		{
		case Debug::EX_INPUT:
			msg (E_CRITICAL, E_USER, "Syntax error on line %d: %s",
				 line_num, e.what());
			break;

		case Debug::EX_BUG:
			msg (E_CRITICAL, E_USER, "Parser internal error on line %d: %s",
				 line_num, e.what());
			break;

		default:
			break; // no need in throwing different exception...
		}

		throw; // ...since we can just rethrow this one.
	}

	catch (std::exception& e)
	{
		msg (E_CRITICAL, E_USER, "Unspecified error parsing line %d: %s",
			 line_num, e.what());

		throw;
	}
}
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
