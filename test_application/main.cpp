#undef __STRICT_ANSI__

#include "../API.h"

#include <uXray/fxjitruntime.h>
#include <uXray/time_ops.h>
#include <uXray/fxmath.h>

DeclareDescriptor( ICA, , )
ImplementDescriptor( ICA, "interpreter client", MOD_APPMODULE )

DeclareDescriptor( SDLApplication, , )
ImplementDescriptor( SDLApplication, "SDL-based UI", MOD_APPMODULE )

DeclareDescriptor( Timer, , )
ImplementDescriptor( Timer, "timer helper", MOD_APPMODULE )

using Processor::int_t;
using Processor::fp_t;
using Processor::calc_t;
using Processor::ctx_t;

struct InputFile
{
	const char* filename;
	bool is_bytecode;
	ctx_t context_assigned;
};

struct ExecutionParameters
{
	bool use_jit;
	bool use_timer;
	Debug::EventLevelIndex_ debug_level;
	std::vector<InputFile> files;
	const char* dump_bytecode_to;
	bool no_exec;
};

struct Statistics {
	size_t syscall_count;
	size_t user_cmd_count;
	size_t command_count[Processor::Value::V_MAX + 1];

	size_t all() const {
		size_t result = syscall_count + user_cmd_count;

		for( size_t i = 0; i <= Processor::Value::V_MAX; ++i )
			result += command_count[i];

		return result;
	}
};

// ---- pthreads global
pthread_attr_t global_detached_attrs;

// ---- Interpreter thread
class InterpreterClientApplication;

pthread_t interpreter_thread;
InterpreterClientApplication* application = nullptr;

void* interpreter_threadfunc( void* argument );
void interpreter_cancellation( void* );

// ---- Timer thread
class Timer;

pthread_t timer_thread;
Timer* timer = nullptr;

void* timer_threadfunc( void* );
void timer_cancellation( void* );
// ----

class Timer : LogBase( Timer )
{
	void* target_clockid;
	time_data last_capture_time, last_interval;
	Statistics last_capture_stats;

public:
	Timer();
	virtual ~Timer();

	void DumpResults( const time_data* interval, const Statistics* stats );
	void ProcessSingleFrame();

	void* TargetCID() const {
		return target_clockid;
	}
};

class StatisticsCollectorLogic : public ProcessorImplementation::Logic
{
	mutable pthread_mutex_t statistics_mutex;
	Statistics statistics;

public:
	void ResetStatistics() {
		memset( &statistics, 0, sizeof( statistics ) );
	}

	StatisticsCollectorLogic() {
		pthread_mutex_init( &statistics_mutex, nullptr );
		ResetStatistics();
	}

	virtual ~StatisticsCollectorLogic() {
		pthread_mutex_destroy( &statistics_mutex );
	}

	Statistics GetStatistics() {
		pthread_mutex_lock( &statistics_mutex );
		Statistics local_copy = statistics;
		ResetStatistics();
		pthread_mutex_unlock( &statistics_mutex );

		return local_copy;
	}

	virtual void Syscall( size_t index ) {
		pthread_mutex_lock( &statistics_mutex );
		++statistics.syscall_count;
		pthread_mutex_unlock( &statistics_mutex );

		Logic::Syscall( index );
	}

	virtual void ExecuteSingleCommand( Processor::Command& command ) {
		Logic::ExecuteSingleCommand( command );

		pthread_mutex_lock( &statistics_mutex );

		if( command.cached_executor )
			++statistics.command_count[command.cached_executor->SupportedType()];

		else
			++statistics.user_cmd_count;

		pthread_mutex_unlock( &statistics_mutex );

		pthread_testcancel();
	}
};

class InterpreterClientApplication : LogBase( ICA )
{

	Processor::ProcessorAPI processor;
	StatisticsCollectorLogic* custom_logic;
	bool is_running;
	ExecutionParameters* params;

	static void delay_command( Processor::ProcessorAPI*, Processor::Command& command ) {
		float ms = 0;
		command.arg.value.Get( Processor::Value::V_MAX, ms, false );

		smsg( E_INFO, E_DEBUG, "Delaying execution for %g milliseconds", ms );

		timeops::sleep( ms );

		smsg( E_INFO, E_DEBUG, "Continuing execution" );
	}

	static void rand_c( Processor::ProcessorAPI* proc, Processor::Command& ) {
		proc->LogicProvider()->StackPush( static_cast<int_t>( rand() ) );
	}

	static void pi_c( Processor::ProcessorAPI* proc, Processor::Command& ) {
		proc->LogicProvider()->StackPush( static_cast<fp_t>( M_PI ) );
	}

	static void lcm_c( Processor::ProcessorAPI* proc, Processor::Command& ) {
		unsigned a, b;

		proc->LogicProvider()->StackPop().Get( Processor::Value::V_INTEGER, a, false );
		proc->LogicProvider()->StackPop().Get( Processor::Value::V_INTEGER, b, false );

		proc->LogicProvider()->StackPush( static_cast<fp_t>( ( a * b ) / gcd( a, b ) ) );
	}

	void StartTimer() {
		if( !is_running ) {
			is_running = true;

			// Synchronously do preparation.
			timer = new Timer;

			if( params->use_timer )
				pthread_create( &timer_thread, nullptr, &timer_threadfunc, nullptr );
		}
	}

	void StopTimer() {
		if( is_running ) {
			is_running = false;

			if( params->use_timer ) {
				pthread_cancel( timer_thread );
				pthread_join( timer_thread, nullptr );
			}

			delete timer; timer = nullptr;
		}
	}

public:
	void RegisterModules() {
		processor.Attach( new ProcessorImplementation::MMU );
		processor.Attach( new ProcessorImplementation::UATLinker );
		processor.Attach( new ProcessorImplementation::CommandSet_mkI );
		processor.Attach( new ProcessorImplementation::FloatExecutor );
		processor.Attach( new ProcessorImplementation::IntegerExecutor );
		processor.Attach( new ProcessorImplementation::ServiceExecutor );

		if( params->use_jit ) {
			processor.Attach( Processor::IBackend::BackendForCurrentProcessor() );
		}
	}

	void RegisterCustomLogic() {
		custom_logic = new StatisticsCollectorLogic;
		processor.Attach( custom_logic );
	}

	void RegisterCommandHandlers() {
		processor.CommandSet()->AddCommand( Processor::CommandTraits( "delay", "User: Delay execution for milliseconds", Processor::A_VALUE, true ) );
		processor.CommandSet()->AddCommandImplementation( "delay", 0, Fcast<void*>( &delay_command ) );

		processor.CommandSet()->AddCommand( Processor::CommandTraits( "rand", "User: Random value", Processor::A_NONE, true ) );
		processor.CommandSet()->AddCommandImplementation( "rand", 0, Fcast<void*>( &rand_c ) );

		processor.CommandSet()->AddCommand( Processor::CommandTraits( "pi", "User: Get Pi", Processor::A_NONE, false ) );
		processor.CommandSet()->AddCommandImplementation( "pi", 0, Fcast<void*>( &pi_c ) );

		processor.CommandSet()->AddCommand( Processor::CommandTraits( "lcm", "User: Least Common Multiple", Processor::A_NONE, false ) );
		processor.CommandSet()->AddCommandImplementation( "lcm", 0, Fcast<void*>( &lcm_c ) );
	}

	InterpreterClientApplication( ExecutionParameters* parameters ) :
		custom_logic( nullptr ),
		is_running( false ),
		params( parameters )
	{
		if( !processor.ExecutionManager().EHSelftest() )
			msg( E_CRITICAL, E_USER, "Exception handling self-tests failed!" );

		else
			processor.ExecutionManager().EnableExceptionHandling();

		RegisterModules();
		RegisterCustomLogic();
		RegisterCommandHandlers();
		processor.Initialise();
	}

	~InterpreterClientApplication() {
		if( is_running ) {
			msg( E_WARNING, E_USER, "Killing interpreter kernel" );
			StopTimer();
		}

		processor.Initialise( false );
	}

	Statistics GetStatistics() {
		return custom_logic->GetStatistics();
	}

	void ResetStatistics() {
		custom_logic->ResetStatistics();
	}

	void LoadKernel( std::vector<InputFile>& files ) {
		processor.MMU()->ResetEverything();

		msg( E_INFO, E_USER, "Loading processor kernel" );

		ProcessorImplementation::AsmHandler* asm_handler = new ProcessorImplementation::AsmHandler;
		ProcessorImplementation::BytecodeHandler* bytecode_handler = new ProcessorImplementation::BytecodeHandler;

		{
			timeops t( "Kernel loading" );
			for( InputFile& input_file: files ) {
				msg( E_INFO, E_USER, "Loading file \"%s\" (%s)", input_file.filename, input_file.is_bytecode ? "byte-code" : "assembly" );
				FILE* file = fopen( input_file.filename, "rt" );
				cassert( file, "Could not open file \"%s\" for reading", input_file.filename );

				if( input_file.is_bytecode ) {
					processor.Attach( bytecode_handler );
					input_file.context_assigned = processor.Load( file );
					processor.Detach( bytecode_handler );
				} else {
					processor.Attach( asm_handler );
					input_file.context_assigned = processor.Load( file );
					processor.Detach( asm_handler );
				}
			}
		}

		ctx_t final_context = 0;

		if( files.size() > 1 ) {
			timeops t( "Kernel merging" );
			std::vector<ctx_t> contexts;
			for( InputFile& input_file: files ) {
				contexts.push_back( input_file.context_assigned );
			}

			final_context = processor.MergeContexts( contexts );

			for( ctx_t context: contexts ) {
				processor.DeleteContext( context );
			}
		} else {
			final_context = files.front().context_assigned;
		}

		if( params->dump_bytecode_to ) {
			timeops t( "Kernel dumping" );

			FILE* dump_file = fopen( params->dump_bytecode_to, "wb" );
			cassert( dump_file, "Could not open file \"%s\" for dumping", params->dump_bytecode_to );

			processor.Attach( bytecode_handler );
			processor.Dump( final_context, dump_file );
			processor.Detach( bytecode_handler );
		}

		delete asm_handler;
		delete bytecode_handler;

		processor.SelectContext( final_context );

		if( params->use_jit ) {
			timeops t( "Kernel JIT-compilation" );
			processor.Compile();
		}
	}

	void ExecKernelOnce() {
		Processor::calc_t exec_result;

		try {
			msg( E_INFO, E_USER, "Beginning kernel execution" );
			StartTimer();

			timeops t( "Kernel execution" );

			exec_result = processor.Exec();
		}

		catch( NativeException& e ) {
			e.Handle();
			msg( E_CRITICAL, E_USER, "Native exception while executing kernel: %s", e.what() );
		}

		catch( std::exception& e ) {
			msg( E_CRITICAL, E_USER, "Unspecified exception while executing kernel: %s", e.what() );
		}

		StopTimer();
		msg( E_INFO, E_USER, "Kernel execution completed (result: %s)",
		     Processor::ProcDebug::PrintValue( exec_result ).c_str() );
	}

	void TestExecKernel() {
		static const size_t kernel_test_exec_times = 10;
		Processor::calc_t result;

		msg( E_INFO, E_USER, "Testing kernel execution (%zu times)", kernel_test_exec_times );

		timer = new Timer;

		double total_time = 0;
		size_t total_instructions = 0;

		size_t i = 0;

		try {
			for( i = 0; i < kernel_test_exec_times; ++i ) {
				timeops t;
				result = processor.Exec();

				time_data time = t.relative();
				Statistics stats = custom_logic->GetStatistics();

				timer->DumpResults( &time, &stats );

				total_time += time.fp();
				total_instructions += stats.all();

				processor.Reset();
			}
		}

		catch( NativeException& e ) {
			e.Handle();
			msg( E_CRITICAL, E_USER, "A native exception occurred in %zu-th test: %s", i, e.what() );
		}

		delete timer;

		msg( E_WARNING, E_USER, "Testing completed. Overall stats: %lg seconds / %zu instructions : %u c/s",
		     total_time, total_instructions, static_cast<unsigned>( total_instructions / total_time ) );
	}
};

void interpreter_cancellation( void* )
{
	delete application; application = nullptr;
}

void* interpreter_threadfunc( void* argument )
{
	pthread_cleanup_push( &interpreter_cancellation, nullptr );

// 	clockid_t clockid;
// 	pthread_getcpuclockid( interpreter_thread, &clockid );
	timeops::calibrate( timeops::CLOCK_PERTHREAD );

	ExecutionParameters* params = reinterpret_cast<ExecutionParameters*>( argument );

	application = new InterpreterClientApplication( params );
	application->LoadKernel( params->files );
	
	if( !params->no_exec ) {
		application->ExecKernelOnce();
	}

	pthread_cleanup_pop( true );
	return nullptr;
}

void Timer::ProcessSingleFrame()
{
	// request time and statistics
	time_data now;
	last_capture_stats = application->GetStatistics();

	// calculate interval
	last_interval = now - last_capture_time;
	last_capture_time = now;

	if( !last_capture_stats.all() )
		return;

	// output results
	DumpResults( &last_interval, &last_capture_stats );
}

Timer::Timer() :
last_capture_time( static_cast<unsigned long>( 0 ) ),
last_interval( static_cast<unsigned long>( 0 ) )
{
	last_capture_time = time_data();
	application->ResetStatistics();
}

Timer::~Timer()
{
	ProcessSingleFrame(); // process the last frame
}

void Timer::DumpResults( const time_data* interval, const Statistics* stats )
{
	size_t insns = stats->all();
	double time = interval->fp();

	msg( E_INFO, E_VERBOSE,
	     "Statistics: %zu [int %zu fp %zu etc %zu] + %zu/%zu: %u c/s",
	     insns,
	     stats->command_count[Processor::Value::V_INTEGER],
	     stats->command_count[Processor::Value::V_FLOAT],
	     stats->command_count[Processor::Value::V_MAX],
	     stats->syscall_count,
	     stats->user_cmd_count,
// 		 interval->tv_sec, interval->tv_nsec,
	     static_cast<unsigned>( insns / time ) );
}

void* timer_threadfunc( void* )
{
	while( 1 ) {
		timeops::sleep( 1000 );

		// protect thread from killing while we print statistics
		pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, nullptr );

		// update data and output results.
		// exit loop if interpreter had stopped.
		timer->ProcessSingleFrame();

		// allow thread killing
		pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, nullptr );
	}

	return nullptr;
}

void usage( const char* name )
{
	fprintf( stderr,
		     "Usage: %s [--use-jit] [--use-timer] [--quiet|--debug]\n"
			           "[--asm <assembly files...>] [--bytecode <bytecode files...>] [--dump-to <target bytecode file>]\n"
					   "\n"
					   "* --use-jit                        : enable JIT compilation\n"
					   "* --use-timer                      : enable periodic statistics dump\n"
					   "* --quiet, --debug                 : manipulate log verbosity (NOTE: timer output is not visible with --quiet)\n"
					   "* --asm <assembly files...>        : any number of input files in assembly\n"
					   "* --bytecode <bytecode files...>   : any number of input files in binary form\n"
					   "* --dump-to <target bytecode file> : dump the byte-code (after loading and combining) to a file\n"
					   "* --no-exec                        : do not execute code (only load and dump, if asked)\n",
			 name );

	exit( 1 );
}

int main( int argc, char** argv )
{
	if( argc < 2 ) {
		usage( argv[0] );
	}

	ExecutionParameters params;
	params.debug_level = Debug::E_VERBOSE;
	params.use_jit = false;
	params.use_timer = false;
	params.dump_bytecode_to = nullptr;
	params.no_exec = false;

	bool current_is_bytecode = false;
	for( int i = 1; i < argc; ++i ) {
		const char* parameter = argv[i];

		if( !strcmp( parameter, "--debug" ) ) {
			params.debug_level = Debug::E_DEBUG;
		} else if( !strcmp( parameter, "--quiet" ) ) {
			params.debug_level = Debug::E_USER;
		} else if( !strcmp( parameter, "--use-jit" ) ) {
			params.use_jit = true;
		} else if( !strcmp( parameter, "--use-timer" ) ) {
			params.use_timer = true;
		} else if( !strcmp( parameter, "--asm" ) ) {
			current_is_bytecode = false;
		} else if( !strcmp( parameter, "--bytecode" ) ) {
			current_is_bytecode = true;
		} else if( !strcmp( parameter, "--dump-to" ) ) {
			params.dump_bytecode_to = argv[++i];
		} else if( !strcmp(parameter, "--no-exec") ) {
			params.no_exec = true;
		} else if( !strcmp( parameter, "--help" ) ) {
			usage( argv[0] );
		} else if( !strncmp( parameter, "--", 2 ) ) {
			usage( argv[0] );
		} else {
			InputFile file;
			file.filename = parameter;
			file.context_assigned = 0;
			file.is_bytecode = current_is_bytecode;
			params.files.push_back( file );
		}
	}

	Debug::API::SetDefaultVerbosity( params.debug_level );

	pthread_attr_init( &global_detached_attrs );
	pthread_attr_setdetachstate( &global_detached_attrs, PTHREAD_CREATE_DETACHED );

	smsg( E_INFO, E_DEBUG, "Starting interpreter thread" );
	pthread_create( &interpreter_thread, nullptr, &interpreter_threadfunc, &params );

	pthread_join( interpreter_thread, nullptr );

	smsg( E_INFO, E_DEBUG, "Thread execution finished" );

	pthread_attr_destroy( &global_detached_attrs );
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
