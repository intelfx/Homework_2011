#include "stdafx.h"
#include "Hashtable.h"

ImplementDescriptorNoid (Hashtable, "hash table", MOD_OBJECT);

template<>
void ByteStreamRepresentation<std::string>::operator() (const std::string* object, const void** pointer, size_t* size)
{
	*pointer = object ->c_str();
	*size = object ->size();
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
