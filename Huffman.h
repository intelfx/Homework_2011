#ifndef _HUFFMAN_H
#define _HUFFMAN_H

// -----------------------------------------------------------------------------
// Library		Homework
// File			Huffman.h
// Author		intelfx
// Description	Huffman compression implementation (as per hometask assignment #5.a)
// -----------------------------------------------------------------------------

#include "build.h"
#include "time_ops.h"

#include "sys/stat.h"
#include "sys/mman.h"
#include "fcntl.h"

#include "limits.h"

DeclareDescriptor (BinaryDataOperator);
DeclareDescriptor (HuffmanWorker);
DeclareDescriptor (ThreadManager);

enum HuffmanFlags
{
	FL_DEFAULT = 0,
	FL_DO_DEBUG_WRITE,
	FL_USE_THREADS
};

class FileRecord
{
	int fildes;
	void* mapped;
	FILE* stdio_file;
	stat info;

public:
	FileRecord (const FileRecord&) = delete;
	FileRecord& operator= (const FileRecord&) = delete;

	FileRecord (const char* name, bool do_mapping = 1) :
	fildes (open (name, O_RDONLY)),
	mapped (0),
	info()
	{
		__sassert (fildes != -1, "Input file \"%s\" open failed: open() says %s", name, strerror (errno));

		int stat_result = fstat (fildes, &info);
		__sassert (stat_result != -1, "Input file \"%s\" stat failed: fstat() says %s", name, strerror (errno));

		__sassert (S_ISREG (info.st_mode), "Non-regular input file \"%s\"", name);
		msg (E_INFO, E_DEBUG, "Input file \"%s\": ownership %ld:%ld mode %lo", info.st_uid, info.st_gid, info.st_mode);

		if (do_mapping)
		{
			mapped = mmap (0, info.st_size, PROT_READ, MAP_SHARED, fildes, 0);
			__sassert (mapped, "Input file \"%s\" mmap failed: mmap() says %s", name, strerror (errno));
		}

		else
		{
			stdio_file = fdopen (fildes, "rb");
			__sassert (stdio_file, "Input file \"%s\" stdio open failed: fdopen() says %s", name, strerror (errno));
		}
	}

	FileRecord (FileRecord&& that) :
	fildes (that.fildes),
	mapped (that.mapped),
	info (that.info)
	{
		that.mapped = 0;
		that.fildes = -1;
	}

	FileRecord& operator= (FileRecord&& that)
	{
		if (mapped)
			munmap (mapped, info.st_size);

		if (fildes != -1)
			close (fildes);

		fildes = that.fildes;
		mapped = that.mapped;
		info = that.info;

		that.mapped = 0;
		that.fildes = -1;
	}

	const stat& Stat() const
	{
		return info;
	}

	int fd() const
	{
		return fildes;
	}

	~FileRecord()
	{
		if (mapped)
			munmap (mapped, info.st_size);

		if (fildes != -1)
			close (fildes);
	}

	void* Read (off_t offset, size_t length)
	{
		__sassert (mapped, "Unable to read from non-mapped file");

		__sassert (offset + length <= info.st_size, "Cannot access %zu+%zu: file size %zu",
				   offset, length, info.st_size);

		return reinterpret_cast<char*> (mapped) + offset;
	}
};

struct HuffmanCoderSettings
{
	unsigned short threads_num;
	unsigned short block_size_bytes;

	size_t flags;

	std::vector<FileRecord*> input_files;
	FILE* output_file;

	~HuffmanCoderSettings();
} settings;

struct HuffmanTree
{
	static const size_t tree_elements_max = (UCHAR_MAX + 1) * 2;
	struct Node
	{
		unsigned char data;
		off_t parent, _0, _1;
	} PACKED;

	struct WriteoutInfo
	{
		unsigned char bitstream;
		unsigned char length;
	};

	Node index_data[tree_elements_max];
	size_t weights[tree_elements_max];

	WriteoutInfo writer_data[UCHAR_MAX + 1];

	size_t elements_used;
};

class IDataOperator
{
public:
	virtual void WriteHeader (const HuffmanTree* tree) = 0;
	virtual void WriteChar (const HuffmanTree::WriteoutInfo* info) = 0;
	virtual void WriteEOF() = 0;

	virtual void ReadFile (HuffmanTree* tree) = 0;
	virtual bool GetNextChar (HuffmanTree::WriteoutInfo* info) = 0;
};

class BinaryDataOperator : LogBase (BinaryDataOperator), public IDataOperator
{
	const HuffmanTree* used_tree;

	struct Header
	{
		char header_signature[4];
		size_t tree_elements;
	} PACKED;

	struct Footer
	{
		char footer_signature[4];
	} PACKED;

	HuffmanTree::WriteoutInfo output_buffer;

	static const Header signature_header;
	static const Footer signature_footer;

public:
	BinaryDataOperator() :
	used_tree (0)
	{
	}

	virtual void WriteHeader (const HuffmanTree* tree)
	{
		__assert (settings.output_file && !ferror (settings.output_file), "Invalid output file");

		Header written_header = signature_header;
		written_header.tree_elements = tree ->elements_used;

		msg (E_INFO, E_DEBUG, "Writing output file header: %zu elements in tree", tree ->elements_used);

		fwrite (&written_header, sizeof (Header), 1, settings.output_file);
		fwrite (tree ->index_data, sizeof (HuffmanTree::Node), tree ->elements_used, settings.output_file);

		used_tree = tree;
	}

	virtual void WriteChar (const HuffmanTree::WriteoutInfo* info)
	{
		__assert (settings.output_file && !ferror (settings.output_file), "Invalid output file");
		__assert (used_tree, "Tree has not been set");

		HuffmanTree::WriteoutInfo to_write = *info;

		__assert (to_write.length <= 8, "Input bit sequence malformed");
		while (to_write.length--)
		{
			__assert (output_buffer.length <= 8, "Output buffer malformed");
			if (output_buffer.length == 8)
			{
				putc (output_buffer.bitstream, settings.output_file);
				output_buffer.length = 0;
				output_buffer.bitstream = 0;
			}

			// input sequence has its true MSB in container LSB.
			// so insert it bit-by-bit, inverting bit order once more.

			++output_buffer.length;
			output_buffer.bitstream = output_buffer.bitstream << 1;

			output_buffer.bitstream |= (to_write.bitstream & 1);

			--to_write.length;
			to_write.bitstream = to_write.bitstream >> 1;
		}
	}

	virtual void WriteEOF()
	{
		__assert (settings.output_file && !ferror (settings.output_file), "Invalid output file");

		fwrite (&signature_footer, sizeof (Footer), 1, settings.output_file);
		used_tree = 0;
	}

	virtual void ReadFile (HuffmanTree* tree)
	{
		Header read_header;
	}

	virtual bool GetNextChar (HuffmanTree::WriteoutInfo* info);
};

class HuffmanWorker : LogBase (HuffmanWorker)
{
};

class ThreadManager : LogBase (ThreadManager)
{
public:

};

#endif // _HUFFMAN_H