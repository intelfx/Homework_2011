#ifndef INTERPRETER_X86BACKEND_H
#define INTERPRETER_X86BACKEND_H

#include "build.h"

#include "Interfaces.h"
#include <uXray/fxcontainers.h>

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		x86Backend.h
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	x86 JIT compiler backend.
// -------------------------------------------------------------------------------------

namespace ProcessorImplementation
{
using namespace Processor;

class x86Backend : public IBackend
{
	struct NativeImage
	{
		llarray data; // primary working buffer
		struct // mapped executable region
		{
			void* image; 
			size_t length;
		} mm;
		abi_callback_fn_t callback; // processor API callback function
	};
	std::map<size_t, NativeImage> images_;
	size_t current_chk_;
	NativeImage* current_image_;

	void Select( size_t chk, bool create = false );
	void Finalize();
	void Deallocate();
	void Clear();

	void CompileCommand( Command& cmd );

protected:
	virtual bool _Verify() const;
	
public:
	virtual void CompileBuffer( size_t chk, abi_callback_fn_t callback );
	virtual abi_native_fn_t GetImage( size_t chk );
	virtual bool ImageIsOK( size_t chk );
};

} // namespace ProcessorImplementation

#endif // INTERPRETER_X86BACKEND_H
