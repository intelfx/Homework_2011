#ifndef _FXHASHTABLE_H
#define _FXHASHTABLE_H

#include "build.h"

#include "LinkedList.h"

// -----------------------------------------------------------------------------
// Library      Homework
// File         Hashtable.h
// Author       intelfx
// Description  Implementation of a simple hash table, as per hometask assignment #4.a.
// -----------------------------------------------------------------------------

DeclareDescriptor (Hashtable);

template <typename T>
class ByteStreamRepresentation
{
public:
	void operator() (const T* object, const void** pointer, size_t* size)
	{
		*pointer = object;
		*size = sizeof (T);
	}
};

template <typename Key, typename Data>
struct Pair
{
	Key key;
	Data data;

	Pair (Key && k, Data && d) :
	key (std::move (k)),
	data (std::move (d))
	{}

	Pair (Pair && that) :
	key (std::move (that.key)),
	data (std::move (that.data))
	{}

	Pair& operator= (Pair && that)
	{
		if (this == &that)
			return *this;

		key = std::move (that.key);
		data = std::move (that.data);

		return *this;
	}

	Pair() = default;
	~Pair() = default;
	Pair (const Pair&) = delete;
	Pair& operator= (const Pair&) = delete;

	bool operator== (const Pair& that) { return key == that.key; }
	bool operator!= (const Pair& that) { return key != that.key; }
	bool operator< (const Pair& that) { return key < that.key; }
};

template <typename Key, typename Data, typename Rep = ByteStreamRepresentation<Key> >
class Hashtable : LogBase (Hashtable)
{
public:
	typedef size_t (*hash_function) (const void*, size_t);
	typedef LinkedList< Pair<Key, Data> > Hashlist;
	typedef typename Hashlist::Iterator Iterator;

private:
	Hashlist** table_;
	size_t table_size_;
	hash_function bytes_hasher_;
	Rep hash_data_prep_;

	Hashlist& AcTable (size_t hash)
	{
		size_t index = hash % table_size_;

		if (!table_[index])
		{
			msg (E_INFO, E_DEBUG, "Creating linked list in cell [%zu]", index);
			table_[index] = new Hashlist;
		}

		return *table_[index];
	}

	size_t Hash (const Key* object)
	{
		const void* bytes_ptr;
		size_t length;

		hash_data_prep_ (object, &bytes_ptr, &length);

		if (length <= sizeof (size_t))
			return *reinterpret_cast<const size_t*> (bytes_ptr); /* trivial hash */

		else return bytes_hasher_ (bytes_ptr, length); /* normal hash */
	}

protected:
	virtual bool _Verify() const
	{
		verify_statement (table_, "NULL table pointer");
		verify_statement (table_size_, "Zero table size");
		verify_statement (bytes_hasher_, "Zero hash worker pointer");

		return 1;
	}

public:
	Hashtable (const Hashtable& src) = delete;
	Hashtable& operator= (const Hashtable& src) = delete;

	Hashtable (Hashtable&& that) : move_ctor,
	table_ (that.table_),
	table_size_ (that.table_size_),
	bytes_hasher_ (that.bytes_hasher_),
	hash_data_prep_ (that.hash_data_prep_)
	{
		that.table_ = 0;
		that.table_size_ = 0;
		that.bytes_hasher_ = 0;
	}

	Hashtable& operator= (Hashtable&& that)
	{
		if (this == &that)
			return *this;

		move_op;

		if (table_)
			for (size_t i = 0; i < table_size_; ++i)
				delete table_[i];

		free (table_);

		table_ = that.table_;
		table_size_ = that.table_size_;
		bytes_hasher_ = that.bytes_hasher_;
		hash_data_prep_ = that.hash_data_prep_;

		that.table_ = 0;
		that.table_size_ = 0;
		that.bytes_hasher_ = 0;

		return *this;
	}

	Hashtable (hash_function hasher, size_t table_size = 0x100) :
	table_ (reinterpret_cast<Hashlist**> (calloc (table_size, sizeof (Hashlist*)))),
	table_size_ (table_size),
	bytes_hasher_ (hasher),
	hash_data_prep_()
	{
		__assert (table_size, "Zero table size");
		__assert (table_, "Could not allocate table");

		for (size_t i = 0; i < table_size_; ++i)
			table_[i] = 0;
	}

	virtual ~Hashtable()
	{
		if (table_)
			for (size_t i = 0; i < table_size_; ++i)
				delete table_[i];

		free (table_);
	}

	Iterator Add (Key&& key, Data&& data, bool allow_multi_insertion = 0)
	{
		verify_method;

		size_t hash = Hash (&key);

		msg (E_INFO, E_DEBUG, "Adding new object with hash %p -> %zu [len %zu]",
			 hash, hash % table_size_, table_size_);

		Iterator i = AcTable (hash).Begin();


		if (!allow_multi_insertion) /* check for existing keys */
		{
			for (; !i.End(); ++i)
				if (i ->key == key)
					break;

			if (!i.End())
			{
				msg (E_WARNING, E_DEBUG, "Key already exists");
				return i;
			}
		}

		i = AcTable (hash).PushFront (Pair <Key, Data> (std::move (key), std::move (data)));

		verify_method;
		msg (E_INFO, E_DEBUG, "Addition completed");

		return i;
	}

	Iterator Find (const Key& key)
	{
		verify_method;

		size_t hash = Hash (&key);

		msg (E_INFO, E_DEBUG, "Searching for hash %p -> %zu [len %zu]",
			 hash, hash % table_size_, table_size_);

		// We won't use native find methods since we only need to match key in pair
		Iterator i = AcTable (hash).Begin();

		for (; !i.End(); ++i)
			if (i ->key == key)
				break;

		if (i.End())
			msg (E_WARNING, E_DEBUG, "Key not found, returning end");

		return i;
	}

	std::vector<Iterator> FindAll (const Key& key)
	{
		verify_method;

		size_t hash = Hash (&key);

		msg (E_INFO, E_DEBUG, "Searching for hash %p -> %zu [len %zu]",
			 hash, hash % table_size_, table_size_);

		std::vector<Iterator> results;

		for (Iterator i = AcTable (hash).Begin(); !i.End(); ++i)
			if (i ->key == key)
				results.push_back (i);

		if (results.empty())
			msg (E_WARNING, E_DEBUG, "No results, returning empty vector");

		return std::move (results);
	}

	void RemoveDuplicates()
	{
		verify_method;

		msg (E_INFO, E_DEBUG, "Cleaning up hash table, removing duplicates");

		for (size_t idx = 0; idx < table_size_; ++idx)
			if (table_[idx])
			{
				Hashlist& lst = *table_[idx];

				for (Iterator i = lst.Begin(); !i.End(); ++i)
					for (Iterator j = i + 1; !j.End();)
					{
						if (i ->data == j ->data)
							j = lst.Erase (j);

						else
							++j;
					}
			}
	}

	void Remove (const Key& key)
	{
		verify_method;

		size_t hash = Hash (&key);

		msg (E_INFO, E_DEBUG, "Removing with hash %p -> %zu [len %zu]",
			 hash, hash % table_size_, table_size_);

		for (Iterator i = AcTable (hash).Begin(); !i.End();)
		{
			if (i ->key == key)
				AcTable (hash).Erase (i);

			else
				++i;
		}
	}

	void ReadStats (size_t* total, size_t* used, size_t** counts)
	{
		size_t used_ = 0;
		size_t* counts_ = reinterpret_cast<size_t*> (malloc (sizeof (size_t) * table_size_));

		for (size_t i = 0; i < table_size_; ++i)
		{
			if (table_[i])
			{
				++used_;
				counts_[i] = table_[i] ->Count();
			}

			else
				counts_[i] = 0;
		}

		*total = table_size_;
		*used = used_;
		*counts = counts_;
	}
};

#endif // _FXHASHTABLE_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
