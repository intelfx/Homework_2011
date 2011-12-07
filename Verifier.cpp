#include "stdafx.h"
#include "Verifier.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			Verifier.cpp
// Author		intelfx
// Description	Verifier and allocator classes for containers.
// -----------------------------------------------------------------------------

ImplementDescriptorNoid(AllocBase, "allocator", MOD_INTERNAL);
ImplementDescriptorNoid(StaticAllocator, "static allocator", MOD_INTERNAL);
ImplementDescriptorNoid(MallocAllocator, "malloc-pwrd allocator", MOD_INTERNAL);