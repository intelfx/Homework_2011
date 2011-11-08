#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

const char* filename = "morse_code.h";

void write_letter (unsigned letter)
{
	switch (letter)
	{
		case '0': printf ("\"-----\""); break;
		case '1': printf ("\".----\""); break;
		case '2': printf ("\"..---\""); break;
		case '3': printf ("\"...--\""); break;
		case '4': printf ("\"....-\""); break;
		case '5': printf ("\".....\""); break;
		case '6': printf ("\"-....\""); break;
		case '7': printf ("\"--...\""); break;
		case '8': printf ("\"---..\""); break;
		case '9': printf ("\"----.\""); break;

		case 'a': printf ("\".-\""); break;
		case 'b': printf ("\"-...\""); break;
		case 'c': printf ("\"-.-.\""); break;
		case 'd': printf ("\"-..\""); break;
		case 'e': printf ("\".\""); break;
		case 'f': printf ("\"..-.\""); break;
		case 'g': printf ("\"--.\""); break;
		case 'h': printf ("\"....\""); break;
		case 'i': printf ("\"..\""); break;
		case 'j': printf ("\".---\""); break;
		case 'k': printf ("\"-.-\""); break;
		case 'l': printf ("\".-..\""); break;
		case 'm': printf ("\"--\""); break;
		case 'n': printf ("\"-.\""); break;
		case 'o': printf ("\"---\""); break;
		case 'p': printf ("\".--.\""); break;
		case 'q': printf ("\"--.-\""); break;
		case 'r': printf ("\".-.\""); break;
		case 's': printf ("\"...\""); break;
		case 't': printf ("\"-\""); break;
		case 'u': printf ("\"..-\""); break;
		case 'v': printf ("\"...-\""); break;
		case 'w': printf ("\".--\""); break;
		case 'x': printf ("\"-..-\""); break;
		case 'y': printf ("\"-.--\""); break;
		case 'z': printf ("\"--..\""); break;

		case '.': printf ("\".-.-.-\""); break;
		case ',': printf ("\"--..--\""); break;
		case ':': printf ("\"---...\""); break;
		case ';': printf ("\"-.-.-.\""); break;
		case ')': printf ("\"-.--.\""); break;
		case '(': printf ("\"-.--.-\""); break;
		case '\'': printf ("\".----.\""); break;
		case '"': printf ("\".-..-.\""); break;
		case '-': printf ("\"-....-\""); break;
		case '/': printf ("\"-..-.\""); break;
		case '!': printf ("\"-.-.--\""); break;
		case '?': printf ("\"..--..\""); break;
		case '|': printf ("\"--..--\""); break;
		case '#':
		case '=': printf ("\"-...-\""); break;
		case '_': printf ("\"..--.-\""); break;
		case '$': printf ("\"...-..-\""); break;
		case '@': printf ("\".--.-.\""); break;

		default: printf ("0"); break;
	}
}

int main (int argc, char** argv)
{
	freopen (filename, "w", stdout);

	printf ("static const char* const morse_code[256] =\n"
			"{\n");

	for (unsigned c = 0; c < 0x100; ++c)
	{
		putchar ('\t');
		printf ("/* %c */ ", isgraph (c) ? c : ' ');
		write_letter (c);

		if (c != 0xFF)
			putchar (',');

		putchar ('\n');
	}

	printf ("};\n");

	fclose (stdout);
}