#include "stdafx.h"
#include "Interfaces.h"

// -----------------------------------------------------------------------------
// Library		Homework
// File			Interfaces.cpp
// Author		intelfx
// Description	Interfaces helper default methods.
// -----------------------------------------------------------------------------

namespace Processor
{
	ProcessorAPI::~ProcessorAPI() = default;
	IReader::~IReader() = default;
	IWriter::~IWriter() = default;
	IMMU::~IMMU() = default;
	IExecutor::~IExecutor() = default;
	ILinker::~ILinker() = default;
	IBackend::~IBackend() = default;
	ICommandSet::~ICommandSet() = default;
	ILogic::~ILogic() = default;

	ProcessorAPI* ICommandSet::callback_procapi = 0;

	void ProcessorAPI::Attach (IModuleBase* module)
	{
		bool was_attach = 0;

		if (IBackend* backend = dynamic_cast<IBackend*> (module))
		{
			Attach_ (backend);
			was_attach = 1;
		}

		if (IReader* reader = dynamic_cast<IReader*> (module))
		{
			Attach_ (reader);
			was_attach = 1;
		}

		if (IWriter* writer = dynamic_cast<IWriter*> (module))
		{
			Attach_ (writer);
			was_attach = 1;
		}

		if (IMMU* mmu = dynamic_cast<IMMU*> (module))
		{
			Attach_ (mmu);
			was_attach = 1;
		}

		if (IExecutor* executor = dynamic_cast<IExecutor*> (module))
		{
			Attach_ (executor);
			was_attach = 1;
		}

		if (ICommandSet* cset = dynamic_cast<ICommandSet*> (module))
		{
			Attach_ (cset);
			was_attach = 1;
		}

		if (ILogic* internal_logic = dynamic_cast<ILogic*> (module))
		{
			Attach_ (internal_logic);
			was_attach = 1;
		}

		if (ILinker* linker = dynamic_cast<ILinker*> (module))
		{
			Attach_ (linker);
			was_attach = 1;
		}

		if (!was_attach)
			msg (E_CRITICAL, E_USER, "Could not attach module \"%s\" (%p) - unrecognized module type",
				 Debug::API::GetClassName (module), module);
	}

	void ProcessorAPI::Detach (const IModuleBase* module)
	{
		bool was_detach = 0;

		if (dynamic_cast<const IBackend*> (module) == backend_)
		{
			backend_ = 0;
			was_detach = 1;
		}

		if (const IExecutor* exec_module = dynamic_cast<const IExecutor*> (module))
		{
			Value::Type supported_type = exec_module ->SupportedType();

			if (exec_module == executors_[supported_type])
			{
				executors_[supported_type] = 0;
				was_detach = 1;
			}
		}

		if (dynamic_cast<const IReader*> (module) == reader_)
		{
			reader_ = 0;
			was_detach = 1;
		}

		if (dynamic_cast<const IWriter*> (module) == writer_)
		{
			writer_ = 0;
			was_detach = 1;
		}

		if (dynamic_cast<const ICommandSet*> (module) == cset_)
		{
			cset_ = 0;
			was_detach = 1;
		}

		if (dynamic_cast<const ILogic*> (module) == internal_logic_)
		{
			internal_logic_ = 0;
			was_detach = 1;
		}

		if (dynamic_cast<const ILinker*> (module) == linker_)
		{
			linker_ = 0;
			was_detach = 1;
		}

		if (dynamic_cast<const IMMU*> (module) == mmu_)
		{
			mmu_ = 0;
			was_detach = 1;
		}

		if (!was_detach)
			msg (E_WARNING, E_VERBOSE, "Not detaching \"%s\" (%p) - has not been attached",
				 Debug::API::GetClassName (module), module);
	}

	void ProcessorAPI::Attach_ (IBackend* backend)
	{
		__assert (backend, "NULL backend to be attached");

		if (backend_)
			backend_ ->DetachSelf();

		backend_ = backend;
		backend_ ->SetProcessor (this);
	}

	void ProcessorAPI::Attach_ (ICommandSet* cset)
	{
		__assert (cset, "NULL command set to be attached");

		if (cset_)
			cset_ ->DetachSelf();

		cset_ = cset;
		cset_ ->SetProcessor (this);
	}

	void ProcessorAPI::Attach_ (IExecutor* executor)
	{
		Value::Type supported_type = executor ->SupportedType();
		__assert (supported_type <= Value::V_MAX,
					"Invalid supported type (executor module to be registered \"%s\")",
					Debug::API::GetClassName (executor));

		IExecutor*& executor_ = executors_[supported_type];

		if (executor_)
			executor_ ->DetachSelf();

		executor_ = executor;
		executor_ ->SetProcessor (this);
	}

	void ProcessorAPI::Attach_ (ILinker* linker)
	{
		if (linker_)
			linker_ ->DetachSelf();

		linker_ = linker;
		linker_ ->SetProcessor (this);
	}

	void ProcessorAPI::Attach_ (ILogic* logic)
	{
		if (internal_logic_)
			internal_logic_ ->DetachSelf();

		internal_logic_ = logic;
		internal_logic_ ->SetProcessor (this);
	}

	void ProcessorAPI::Attach_ (IMMU* mmu)
	{
		if (mmu_)
			mmu_ ->DetachSelf();

		mmu_ = mmu;
		mmu_ ->SetProcessor (this);
	}

	void ProcessorAPI::Attach_ (IReader* reader)
	{
		if (reader_)
			reader_ ->DetachSelf();

		reader_ = reader;
		reader_ ->SetProcessor (this);
	}

	void ProcessorAPI::Attach_ (IWriter* writer)
	{
		if (writer_)
			writer_ ->DetachSelf();

		writer_ = writer;
		writer_ ->SetProcessor (this);
	}

	IModuleBase::~IModuleBase()
	{
		DetachSelf();
	}


	void ProcessorAPI::Initialise()
	{
		if (initialise_completed)
			return;

		initialise_completed = 1;
		verify_method;
		msg (E_INFO, E_VERBOSE, "API initialised");
	}

	bool ProcessorAPI::_Verify() const
	{
		if (!initialise_completed)
			return 1;

		verify_statement (internal_logic_, "logic provider not registered");
		verify_statement (internal_logic_ ->CheckObject(), "logic provider verification failure");

		verify_statement (cset_, "command set provider not registered");
		verify_statement (cset_ ->CheckObject(), "command set provider verification failure");

		for (unsigned i = 0; i < Value::V_MAX; ++i)
		{
			verify_statement (executors_[i], "executor [type \"%s\"] not registered",
							  ProcDebug::ValueType_ids[i]);
			verify_statement (executors_[i] ->CheckObject(), "executor [type \"%s\"] verification failure",
							  ProcDebug::ValueType_ids[i]);
		}

		verify_statement (linker_, "linker not registered");
		verify_statement (linker_ ->CheckObject(), "linker verification failure");

		verify_statement (mmu_, "MMU not registered");
		verify_statement (mmu_ ->CheckObject(), "MMU verification failure");

		if (reader_)
			verify_statement (reader_ ->CheckObject(), "read engine verification failure");

		if (writer_)
			verify_statement (writer_ ->CheckObject(), "write engine verification failure");

		if (backend_)
			verify_statement (backend_ ->CheckObject(), "native compiler verification failure");

		return 1;
	}

	ProcessorAPI::ProcessorAPI() :
	reader_ (0),
	writer_ (0),
	mmu_ (0),
	executors_(),
	backend_ (0),
	linker_ (0),
	cset_ (0),
	internal_logic_ (0),
	initialise_completed (0)
	{
	}

} // namespace Processor

ImplementDescriptor (IReader, "reader module", MOD_APPMODULE);
ImplementDescriptor (IWriter, "writer module", MOD_APPMODULE);
ImplementDescriptor (ILinker, "linker module", MOD_APPMODULE);
ImplementDescriptor (IMMU, "memory management unit", MOD_APPMODULE);
ImplementDescriptor (IExecutor, "executor module", MOD_APPMODULE);
ImplementDescriptor (ProcessorAPI, "processor API", MOD_APPMODULE);
ImplementDescriptor (ICommandSet, "command set handler", MOD_APPMODULE);
ImplementDescriptor (IBackend, "native compiler", MOD_APPMODULE);
ImplementDescriptor (IInternalLogic, "processor logic", MOD_APPMODULE);
ImplementDescriptor (IModuleBase, "unspecified module", MOD_APPMODULE);
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;

