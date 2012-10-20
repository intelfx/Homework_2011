#include <stdio.h>

#include "../API.h"
#include "../x86Backend.h"

#include <uXray/fxlog_console.h>
#include <uXray/fxjitruntime.h>
#include <uXray/time_ops.h>
#include <uXray/fxmath.h>

#include "ssb.h"

DeclareDescriptor( ICA, , );
ImplementDescriptor( ICA, "interpreter client", MOD_APPMODULE );

DeclareDescriptor( SDLApplication, , );
ImplementDescriptor( SDLApplication, "SDL-based UI", MOD_APPMODULE );

DeclareDescriptor( Timer, , );
ImplementDescriptor( Timer, "timer helper", MOD_APPMODULE );

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
InterpreterClientApplication* application = 0;

void* interpreter_threadfunc( void* argument );
void interpreter_cancellation( void* );

// ---- GUI thread
class SDLApplication;

pthread_t gui_thread;
pthread_mutex_t gui_init_mutex;
SDLApplication* gui = 0;
bool gui_thread_need_to_join = 0;

void* gui_threadfunc( void* );
void gui_cancellation( void* );

// ---- Timer thread
class Timer;

pthread_t timer_thread;
Timer* timer = 0;

void* timer_threadfunc( void* );
void timer_cancellation( void* );
// ----


class SDLApplication : LogBase( SDLApplication )
{
	static const size_t size_x = 800, size_y = 600;
	SDL_Surface* video_buffer;

	int center_x, center_y;
	double scale_x, scale_y;
	int last_mouse_x, last_mouse_y;

	long main_color, aux_color, current_color;

public:
	SDLApplication() {
		ssb_init();
		video_buffer = ssb_video_init( size_x, size_y, 0 );

		center_x = size_x / 2;
		center_y = size_y / 2;

		scale_x = 1;
		scale_y = 1;

		last_mouse_x = 0;
		last_mouse_y = 0;


		main_color = 0x66FF6680;
		aux_color = 0x666666FF;

		current_color = main_color;
	}

	~SDLApplication() {
		// Close window
		ssb_video_deinit( video_buffer );
		ssb_deinit();
	}

	void EventLoop() {
		SDL_Event* event;
		bool running = 1;

		while( running ) {
			event = ssb_get_event();

			if( !event )
				continue;

			pthread_mutex_lock( &gui_init_mutex );

			switch( event->type ) {
			case SDL_QUIT:
				smsg( E_INFO, E_USER, "GUI has been closed - exiting event loop" );
				running = 0;
				break;

			case SDL_MOUSEMOTION:
// 					smsg (E_INFO, E_DEBUG, "Mouse motion: %d:%d", event->motion.x, event->motion.y);
				last_mouse_x = event->motion.x;
				last_mouse_y = event->motion.y;
				break;

			default:
				break;
			}

			pthread_mutex_unlock( &gui_init_mutex );
		}
	}

	SDL_Surface* Screen() {
		return video_buffer;
	}

	void SetLimits( double lim_x, double lim_y ) {
		scale_x = ( size_x / 2 ) / lim_x;
		scale_y = ( size_y / 2 ) / lim_y;
	}

	void GetLimits( double* lim_x, double* lim_y ) {
		*lim_x = ( ( size_x / 2 ) / scale_x );
		*lim_y = ( ( size_y / 2 ) / scale_y );
	}

	void GetSteps( double* sx, double* sy ) {
		*sx = ( 1 / scale_x );
		*sy = ( 1 / scale_y );
	}

	long GetPixels( double x, double y ) {
		unsigned px = x * scale_x + center_x, py = y * scale_y + center_y;
		return PACKWORD( py, px );
	}

	void MouseCoords( double* x, double* y ) {
		*x = ( ( last_mouse_x - center_x ) / scale_x );
		*y = ( ( last_mouse_y - center_y ) / scale_y );
	}

	void SetAuxColor() {
		current_color = ssb_map_rgb_inner( video_buffer, aux_color );
	}

	void SetMainColor() {
		current_color = main_color;
	}

	void SetUserColor( float r, float g, float b ) {
		current_color = ssb_map_rgb( video_buffer,
		                             r * 0xFF,
		                             g * 0xFF,
		                             b * 0xFF );
	}

	long CurrentColor() {
		return current_color;
	}
};

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

	virtual void CheckGUI() {
		if( !gui ) {
			pthread_mutex_unlock( &gui_init_mutex );
			casshole( "GUI is not initialised" );
		}
	}

public:
	void ResetStatistics() {
		memset( &statistics, 0, sizeof( statistics ) );
	}

	StatisticsCollectorLogic() {
		pthread_mutex_init( &statistics_mutex, 0 );
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

		switch( index ) {
		case 10: {
			msg( E_INFO, E_DEBUG, "Starting GUI system/thread" );

			pthread_mutex_lock( &gui_init_mutex );

			gui = new SDLApplication;
			gui_thread_need_to_join = 1;
			pthread_create( &gui_thread, 0, &gui_threadfunc, 0 );

			double sx, sy;

			gui->GetLimits( &sx, &sy );

			pthread_mutex_unlock( &gui_init_mutex );

			proc_->MMU()->ARegister( Processor::R_A ).Set( Processor::Value::V_FLOAT, sx, 1 );
			proc_->MMU()->ARegister( Processor::R_B ).Set( Processor::Value::V_FLOAT, sy, 1 );
			break;
		}

		case 11:
			msg( E_INFO, E_DEBUG, "Stopping GUI system/thread" );

			pthread_mutex_lock( &gui_init_mutex );

			CheckGUI();
			pthread_cancel( gui_thread );

			if( gui_thread_need_to_join ) {
				pthread_mutex_unlock( &gui_init_mutex );
				pthread_join( gui_thread, 0 );
			}

			else
				pthread_mutex_unlock( &gui_init_mutex );

			break;

		case 12: {
			msg( E_INFO, E_DEBUG, "GUI scale set request" );

			pthread_mutex_lock( &gui_init_mutex );

			CheckGUI();

			double sx, sy;

			proc_->MMU()->ARegister( Processor::R_A ).Get( Processor::Value::V_FLOAT, sx, 1 );
			proc_->MMU()->ARegister( Processor::R_B ).Get( Processor::Value::V_FLOAT, sy, 1 );

			gui->SetLimits( sx, sy );
			gui->GetSteps( &sx, &sy );

			proc_->MMU()->ARegister( Processor::R_A ).Set( Processor::Value::V_FLOAT, sx, 1 );
			proc_->MMU()->ARegister( Processor::R_B ).Set( Processor::Value::V_FLOAT, sy, 1 );

			pthread_mutex_unlock( &gui_init_mutex );

			break;
		}

		case 13: {
			msg( E_INFO, E_DEBUG, "GUI screen redraw request" );

			pthread_mutex_lock( &gui_init_mutex );

			CheckGUI();

			ssb_flip( gui->Screen() );

			pthread_mutex_unlock( &gui_init_mutex );
			break;
		}

		case 14: {
			msg( E_INFO, E_DEBUG, "GUI mouse coordinates request" );

			pthread_mutex_lock( &gui_init_mutex );

			CheckGUI();

			double mx, my;

			gui->MouseCoords( &mx, &my );

			pthread_mutex_unlock( &gui_init_mutex );

			proc_->MMU()->ARegister( Processor::R_A ).Set( Processor::Value::V_FLOAT, mx, 1 );
			proc_->MMU()->ARegister( Processor::R_B ).Set( Processor::Value::V_FLOAT, my, 1 );

			break;
		}

		case 15: {
			msg( E_INFO, E_DEBUG, "GUI main color selection" );

			pthread_mutex_lock( &gui_init_mutex );

			CheckGUI();

			gui->SetMainColor();

			pthread_mutex_unlock( &gui_init_mutex );
			break;
		}

		case 16: {
			msg( E_INFO, E_DEBUG, "GUI auxiliary color selection" );

			pthread_mutex_lock( &gui_init_mutex );

			CheckGUI();

			gui->SetAuxColor();

			pthread_mutex_unlock( &gui_init_mutex );
			break;
		}

		case 17: {
			msg( E_INFO, E_DEBUG, "GUI screen clear request" );

			pthread_mutex_lock( &gui_init_mutex );

			CheckGUI();

			ssb_clear( gui->Screen() );

			pthread_mutex_unlock( &gui_init_mutex );
			break;
		}

		case 18: {
			msg( E_INFO, E_DEBUG, "GUI user color set request" );

			pthread_mutex_lock( &gui_init_mutex );

			CheckGUI();

			float r, g, b;

			proc_->MMU()->ARegister( Processor::R_A ).Get( Processor::Value::V_FLOAT, r, 1 );
			proc_->MMU()->ARegister( Processor::R_B ).Get( Processor::Value::V_FLOAT, g, 1 );
			proc_->MMU()->ARegister( Processor::R_C ).Get( Processor::Value::V_FLOAT, b, 1 );

			msg( E_INFO, E_DEBUG, "GUI set user color: %f:%f:%f", r, g, b );
			gui->SetUserColor( r, g, b );

			pthread_mutex_unlock( &gui_init_mutex );
			break;
		}

		case 20: {
			pthread_mutex_lock( &gui_init_mutex );

			CheckGUI();

			double px, py;

			proc_->MMU()->ARegister( Processor::R_A ).Get( Processor::Value::V_FLOAT, px, 1 );
			proc_->MMU()->ARegister( Processor::R_B ).Get( Processor::Value::V_FLOAT, py, 1 );

			long coords = gui->GetPixels( px, py );

			msg( E_INFO, E_DEBUG, "GUI plot: %lf:%lf = %ld:%ld", px, py, GETX( coords ), GETY( coords ) );

			ssb_pixel( gui->Screen(), coords, gui->CurrentColor() );

			pthread_mutex_unlock( &gui_init_mutex );
			break;
		}

		case 21: {
			pthread_mutex_lock( &gui_init_mutex );

			CheckGUI();

			double p1x, p1y, p2x, p2y;

			proc_->MMU()->ARegister( Processor::R_A ).Get( Processor::Value::V_FLOAT, p1x, 1 );
			proc_->MMU()->ARegister( Processor::R_B ).Get( Processor::Value::V_FLOAT, p1y, 1 );
			proc_->MMU()->ARegister( Processor::R_C ).Get( Processor::Value::V_FLOAT, p2x, 1 );
			proc_->MMU()->ARegister( Processor::R_D ).Get( Processor::Value::V_FLOAT, p2y, 1 );

			long c1 = gui->GetPixels( p1x, p1y );
			long c2 = gui->GetPixels( p2x, p2y );


			msg( E_INFO, E_DEBUG, "GUI line: %lf:%lf-> %lf:%lf = %ld:%ld-> %ld:%ld",
			     p1x, p1y, p2x, p2y, GETX( c1 ), GETY( c1 ), GETX( c2 ), GETY( c2 ) );

			ssb_line( gui->Screen(), c1, c2, gui->CurrentColor() );

			pthread_mutex_unlock( &gui_init_mutex );
			break;
		}

		case 22: {
			pthread_mutex_lock( &gui_init_mutex );

			CheckGUI();

			double px, py;
			double r;

			proc_->MMU()->ARegister( Processor::R_A ).Get( Processor::Value::V_FLOAT, px, 1 );
			proc_->MMU()->ARegister( Processor::R_B ).Get( Processor::Value::V_FLOAT, py, 1 );
			proc_->MMU()->ARegister( Processor::R_C ).Get( Processor::Value::V_FLOAT, r, 1 );

			long coords = gui->GetPixels( px, py );
			long radius = gui->GetPixels( r, r );


			msg( E_INFO, E_DEBUG, "GUI circle: %lf:%lf = %ld:%ld | radius %lf = %ld:%ld",
			     px, py, GETX( coords ), GETY( coords ), r, GETX( radius ), GETY( radius ) );

			ssb_ellipse( gui->Screen(), coords, radius, gui->CurrentColor() );

			pthread_mutex_unlock( &gui_init_mutex );
			break;
		}

		case 23: {
			pthread_mutex_lock( &gui_init_mutex );

			CheckGUI();

			double px, py;
			double rx, ry;

			proc_->MMU()->ARegister( Processor::R_A ).Get( Processor::Value::V_FLOAT, px, 1 );
			proc_->MMU()->ARegister( Processor::R_B ).Get( Processor::Value::V_FLOAT, py, 1 );
			proc_->MMU()->ARegister( Processor::R_C ).Get( Processor::Value::V_FLOAT, rx, 1 );
			proc_->MMU()->ARegister( Processor::R_D ).Get( Processor::Value::V_FLOAT, ry, 1 );

			long coords = gui->GetPixels( px, py );
			long radius = gui->GetPixels( rx, ry );


			msg( E_INFO, E_DEBUG, "GUI ellipse: %lf:%lf = %ld:%ld | radius %lf:%lf = %ld:%ld",
			     px, py, GETX( coords ), GETY( coords ), rx, ry, GETX( radius ), GETY( radius ) );

			ssb_ellipse( gui->Screen(), coords, radius, gui->CurrentColor() );

			pthread_mutex_unlock( &gui_init_mutex );
			break;
		}

		default:
			Logic::Syscall( index );
			break;
		}
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
	bool use_periodic_write;
	bool use_jit;
	const char* dump_filename;

	static void delay_command( Processor::ProcessorAPI*, Processor::Command& command ) {
		float ms = 0;
		command.arg.value.Get( Processor::Value::V_MAX, ms, 0 );

		smsg( E_INFO, E_DEBUG, "Delaying execution for %g milliseconds", ms );

		usleep( ms * 1000 );

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

		proc->LogicProvider()->StackPop().Get( Processor::Value::V_INTEGER, a, 0 );
		proc->LogicProvider()->StackPop().Get( Processor::Value::V_INTEGER, b, 0 );

		proc->LogicProvider()->StackPush( static_cast<fp_t>( ( a * b ) / gcd( a, b ) ) );
	}

	void StartTimer() {
		if( !is_running ) {
			is_running = 1;

			// Synchronously do preparation.
			timer = new Timer;

			if( use_periodic_write )
				pthread_create( &timer_thread, 0, &timer_threadfunc, 0 );
		}
	}

	void StopTimer() {
		if( is_running ) {
			is_running = 0;

			if( use_periodic_write ) {
				pthread_cancel( timer_thread );
				pthread_join( timer_thread, 0 );
			}

			delete timer; timer = 0;
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

		if( use_jit ) {
			processor.Attach( new ProcessorImplementation::x86Backend );
		}
	}

	void RegisterCustomLogic() {
		custom_logic = new StatisticsCollectorLogic;
		processor.Attach( custom_logic );
	}

	void RegisterCommandHandlers() {
		processor.CommandSet()->AddCommand( Processor::CommandTraits( "delay", "User: Delay execution for milliseconds", Processor::A_VALUE, true ) );
		processor.CommandSet()->AddCommandImplementation( "delay", 0, reinterpret_cast<void*>( &delay_command ) );

		processor.CommandSet()->AddCommand( Processor::CommandTraits( "rand", "User: Random value", Processor::A_NONE, true ) );
		processor.CommandSet()->AddCommandImplementation( "rand", 0, reinterpret_cast<void*>( &rand_c ) );

		processor.CommandSet()->AddCommand( Processor::CommandTraits( "pi", "User: Get Pi", Processor::A_NONE, false ) );
		processor.CommandSet()->AddCommandImplementation( "pi", 0, reinterpret_cast<void*>( &pi_c ) );

		processor.CommandSet()->AddCommand( Processor::CommandTraits( "lcm", "User: Least Common Multiple", Processor::A_NONE, false ) );
		processor.CommandSet()->AddCommandImplementation( "lcm", 0, reinterpret_cast<void*>( &lcm_c ) );
	}

	InterpreterClientApplication( bool periodic_write, bool jit, const char* dump_fn ) :
		custom_logic( 0 ),
		is_running( 0 ),
		use_periodic_write( periodic_write ),
		use_jit( jit ),
		dump_filename( dump_fn )
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
				msg( E_INFO, E_VERBOSE, "Loading file \"%s\" (%s)", input_file.filename, input_file.is_bytecode ? "byte-code" : "assembly" );
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

		if( dump_filename ) {
			timeops t( "Kernel dumping" );

			FILE* dump_file = fopen( dump_filename, "wb" );
			cassert( dump_file, "Could not open file \"%s\" for dumping", dump_filename );

			processor.Attach( bytecode_handler );
			processor.Dump( final_context, dump_file );
			processor.Detach( bytecode_handler );
		}

		processor.SelectContext( final_context );

		delete asm_handler;
		delete bytecode_handler;

		if( use_jit ) {
			timeops t( "Kernel JIT-compilation" );
			processor.Compile();
		}
	}

	void ExecKernelOnce() {
		Processor::calc_t exec_result;

		try {
			msg( E_INFO, E_USER, "Beginning kernel execution" );

			StartTimer();
			exec_result = processor.Exec();
		}

		catch( NativeException& e ) {
			e.Handle();
			msg( E_CRITICAL, E_USER, "Native exception while executing kernel: %s", e.what() );
			msg( E_CRITICAL, E_USER, "The backtrace follows" );
			e.DumpBacktrace();
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
			e.DumpBacktrace();
		}

		delete timer;

		msg( E_WARNING, E_USER, "Testing completed. Overall stats: %lg seconds / %zu instructions : %u c/s",
		     total_time, total_instructions, static_cast<unsigned>( total_instructions / total_time ) );
	}
};

void gui_cancellation( void* )
{
	pthread_mutex_lock( &gui_init_mutex );
	delete gui; gui = 0;
	pthread_mutex_unlock( &gui_init_mutex );
}

void* gui_threadfunc( void* )
{
	pthread_cleanup_push( &gui_cancellation, 0 );

	gui->EventLoop();

	// Stop interpreter only if we are stopping on our own (exited the loop)
	pthread_cancel( interpreter_thread );
	pthread_join( interpreter_thread, 0 );

	pthread_cleanup_pop( 1 );

	return 0;
}

void interpreter_cancellation( void* )
{
	delete application; application = 0;
}

void* interpreter_threadfunc( void* argument )
{
	pthread_cleanup_push( &interpreter_cancellation, 0 );

	clockid_t clockid;
	pthread_getcpuclockid( interpreter_thread, &clockid );
	timeops::calibrate_platform( clockid );

	ExecutionParameters* params = reinterpret_cast<ExecutionParameters*>( argument );

	application = new InterpreterClientApplication( params->use_timer, params->use_jit, params->dump_bytecode_to );
	application->LoadKernel( params->files );
	application->ExecKernelOnce();

	pthread_cleanup_pop( 1 );
	return 0;
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
		pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, 0 );

		// update data and output results.
		// exit loop if interpreter had stopped.
		timer->ProcessSingleFrame();

		// allow thread killing
		pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, 0 );
	}
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
					   "* --dump-to <target bytecode file> : dump the byte-code (after loading and combining) to a file\n",
			 name );

	exit( 1 );
}

int main( int argc, char** argv )
{
	Debug::System::Instance().SetTargetProperties( Debug::CreateTarget( "stderr",
	                                                                    // FUCK MY BRAIN!!! @ 31.01.12 13:45, bug finding time was 01h,
	                                                                    // used "&&" instead of "&".
	                                                                    EVERYTHING & ~( MASK( Debug::E_OBJDESTRUCTION ) | MASK (Debug::E_OBJCREATION) ),
	                                                                    EVERYTHING ),
	                                               &FXConLog::Instance() );


	if( argc < 2 ) {
		usage( argv[0] );
	}

	ExecutionParameters params;
	params.debug_level = Debug::E_VERBOSE;
	params.use_jit = false;
	params.use_timer = false;
	params.dump_bytecode_to = nullptr;

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

	pthread_mutex_init( &gui_init_mutex, 0 );
	pthread_attr_init( &global_detached_attrs );
	pthread_attr_setdetachstate( &global_detached_attrs, PTHREAD_CREATE_DETACHED );

	smsg( E_INFO, E_DEBUG, "Starting interpreter thread" );
	pthread_create( &interpreter_thread, 0, &interpreter_threadfunc, &params );

	pthread_join( interpreter_thread, 0 );

	pthread_mutex_lock( &gui_init_mutex );

	if( gui_thread_need_to_join ) {
		pthread_mutex_unlock( &gui_init_mutex );
		pthread_join( gui_thread, 0 );
	}

	else
		pthread_mutex_unlock( &gui_init_mutex );


	smsg( E_INFO, E_DEBUG, "Thread execution finished" );

	pthread_attr_destroy( &global_detached_attrs );
	pthread_mutex_destroy( &gui_init_mutex );

	Debug::System::Instance.ForceDelete();
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4;
