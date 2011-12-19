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

	IExecutor* ICommandSet::default_exec_ = 0;

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

		if (module == backend_)
		{
			backend_ = 0;
			was_detach = 1;
		}

		if (module == executor_)
		{
			executor_ = 0;
			was_detach = 1;
		}

		if (module == reader_)
		{
			reader_ = 0;
			was_detach = 1;
		}

		if (module == writer_)
		{
			writer_ = 0;
			was_detach = 1;
		}

		if (module == cset_)
		{
			cset_ = 0;
			was_detach = 1;
		}

		if (module == internal_logic_)
		{
			internal_logic_ = 0;
			was_detach = 1;
		}

		if (module == linker_)
		{
			linker_ = 0;
			was_detach = 1;
		}

		if (module == mmu_)
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
		if (!backend)
		{
			msg (E_WARNING, E_VERBOSE, "Unregistered backend compiler module");
			backend_ = 0;
		}

		else
		{
			if (backend_)
				backend_ ->DetachSelf();

			msg (E_INFO, E_DEBUG, "Attaching backend compiler module");
			backend_ = backend;
			backend_ ->SetProcessor (this);
		}
	}

	void ProcessorAPI::Attach_ (ICommandSet* cset)
	{
		if (!cset)
		{
			msg (E_WARNING, E_VERBOSE, "Unregistered command set provider");
			cset_ = 0;
		}

		else
		{
			if (cset_)
				cset_ ->DetachSelf();

			msg (E_INFO, E_DEBUG, "Attaching command set provider");
			cset_ = cset;
			cset_ ->SetProcessor (this);
		}
	}

	void ProcessorAPI::Attach_ (IExecutor* executor)
	{
		if (!executor)
		{
			msg (E_WARNING, E_VERBOSE, "Unregistered executor");
			executor_ = 0;
		}

		else
		{
			if (executor_)
				executor_ ->DetachSelf();

			msg (E_INFO, E_DEBUG, "Attaching executor");
			executor_ = executor;
			executor_ ->SetProcessor (this);
		}
	}

	void ProcessorAPI::Attach_ (ILinker* linker)
	{
		if (!linker)
		{
			msg (E_WARNING, E_VERBOSE, "Unregistered linker");
			linker_ = 0;
		}

		else
		{
			if (linker_)
				linker_ ->DetachSelf();

			msg (E_INFO, E_DEBUG, "Attaching linker");
			linker_ = linker;
			linker_ ->SetProcessor (this);
		}
	}

	void ProcessorAPI::Attach_ (ILogic* logic)
	{
		if (!logic)
		{
			internal_logic_ = 0;
			msg (E_WARNING, E_VERBOSE, "Unregistered internal logic engine");
		}

		else
		{
			if (internal_logic_)
				internal_logic_ ->DetachSelf();

			msg (E_INFO, E_DEBUG, "Attaching internal logic engine");
			internal_logic_ = logic;
			internal_logic_ ->SetProcessor (this);
		}
	}

	void ProcessorAPI::Attach_ (IMMU* mmu)
	{
		if (!mmu)
		{
			msg (E_WARNING, E_VERBOSE, "Unregistered MMU");
			mmu_ = 0;
		}

		else
		{
			if (mmu_)
				mmu_ ->DetachSelf();

			msg (E_INFO, E_DEBUG, "Attaching MMU");
			mmu_ = mmu;
			mmu_ ->SetProcessor (this);
			mmu_ ->ResetEverything();
		}
	}

	void ProcessorAPI::Attach_ (IReader* reader)
	{
		if (!reader)
		{
			msg (E_WARNING, E_VERBOSE, "Unregistered reader");
			reader_ = 0;
		}

		else
		{
			if (reader_)
				reader_ ->DetachSelf();

			msg (E_INFO, E_DEBUG, "Attaching reader");
			reader_ = reader;
			reader_ ->SetProcessor (this);
		}
	}

	void ProcessorAPI::Attach_ (IWriter* writer)
	{
		if (!writer)
		{
			msg (E_WARNING, E_VERBOSE, "Unregistered writer");
			writer_ = 0;
		}

		else
		{
			if (writer_)
				writer_ ->DetachSelf();

			msg (E_INFO, E_DEBUG, "Attaching writer");
			writer_ = writer;
			writer_ ->SetProcessor (this);
		}
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

		verify_statement (executor_, "main executor not registered");
		verify_statement (executor_ ->CheckObject(), "main executor verification failure");

		verify_statement (linker_, "linker not registered");
		verify_statement (linker_ ->CheckObject(), "linker verification failure");

		verify_statement (executor_, "executor not registered");
		verify_statement (executor_ ->CheckObject(), "executor verification failure");

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

	void ICommandSet::SetFallbackExecutor (IExecutor* exec)
	{
		if (!exec)
		{
			smsg (E_WARNING, E_VERBOSE, "Deregistering fallback executor");
			default_exec_ = 0;
		}

		else
		{
			smsg (E_WARNING, E_VERBOSE, "Setting fallback executor");
			default_exec_ = exec;
		}
	}

	ProcessorAPI::ProcessorAPI() :
		reader_ (0),
		writer_ (0),
		mmu_ (0),
		executor_ (0),
		backend_ (0),
		linker_ (0),
		cset_ (0),
		internal_logic_ (0),
		initialise_completed (0)
	{
	}

} // namespace Processor

ImplementDescriptor (IReader, "reader module", MOD_APPMODULE, Processor::IReader);
ImplementDescriptor (IWriter, "writer module", MOD_APPMODULE, Processor::IWriter);
ImplementDescriptor (ILinker, "linker module", MOD_APPMODULE, Processor::ILinker);
ImplementDescriptor (IMMU, "memory management unit", MOD_APPMODULE, Processor::IMMU);
ImplementDescriptor (IExecutor, "executor module", MOD_APPMODULE, Processor::IExecutor);
ImplementDescriptor (ProcessorAPI, "processor API", MOD_APPMODULE, Processor::ProcessorAPI);
ImplementDescriptor (ICommandSet, "command set handler", MOD_APPMODULE, Processor::ICommandSet);
ImplementDescriptor (IBackend, "native compiler", MOD_APPMODULE, Processor::IBackend);
ImplementDescriptor (IInternalLogic, "processor logic", MOD_APPMODULE, Processor::ILogic);
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;

