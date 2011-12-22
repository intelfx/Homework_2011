#include "stdafx.h"
#include "Verifier.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			Verifier.cpp
// Author		intelfx
// Description	Verifier and allocator classes for containers.
// -----------------------------------------------------------------------------

ImplementDescriptor(AllocBase, "allocator", MOD_OBJECT);
ImplementDescriptor(StaticAllocator, "static allocator", MOD_OBJECT);
ImplementDescriptor(MallocAllocator, "malloc-pwrd allocator", MOD_OBJECT);