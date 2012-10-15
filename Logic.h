#ifndef INTERPRETER_LOGIC_H
#define INTERPRETER_LOGIC_H

#include "build.h"

#include "Interfaces.h"

// -------------------------------------------------------------------------------------
// Library:		Homework
// File:		Logic.h
// Author:		Ivan Shapovalov <intelfx100@gmail.com>
// Description:	Default supplementary logic plugin implementation.
// -------------------------------------------------------------------------------------

namespace ProcessorImplementation
{
using namespace Processor;

class INTERPRETER_API Logic : public ILogic
{
	static const char* RegisterIDs [R_MAX];
	std::stack<Context> call_stack_;
	Value::Type current_stack_type_;

public:
	virtual void Analyze( calc_t value );
	virtual void Syscall( size_t index );

	virtual Register DecodeRegister( const char* reg );
	virtual const char* EncodeRegister( Register reg );

	virtual std::string DumpCommand( Command& command ) const;

	virtual void Jump( const DirectReference& ref );
	virtual calc_t Read( const DirectReference& ref );
	virtual void Write( const DirectReference& ref, calc_t value );
	virtual void UpdateType( const DirectReference& ref, Value::Type requested_type );

	virtual size_t StackSize();
	virtual calc_t StackTop();
	virtual calc_t StackPop();
	virtual void StackPush( calc_t value );

	virtual void ResetCurrentContextState();
	virtual void SetCurrentContextBuffer( ctx_t ctx );
	virtual void SaveCurrentContext();
	virtual void RestoreCurrentContext();
	virtual void ClearContextStack();

	virtual size_t ChecksumState();

	virtual void ExecuteSingleCommand( Command& command );
};

} // namespace ProcessorImplementation

#endif // INTERPRETER_LOGIC_H
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
