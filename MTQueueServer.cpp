#include "stdafx.h"
#include "MTServerClientApp/SharedServerClientProtocol.h"
#include "MTServerClientApp/InputServerApp.h"

#include <uXray/fxlog_console.h>
#include <uXray/fxmodarch.h>

#include <pthread.h>

// -----------------------------------------------------------------------------
// Library		Antided
// File			Server.cpp
// Author		intelfx
// Description	Multithreaded application server, as per hometask assignment #2.b.
// -----------------------------------------------------------------------------

ImplementDescriptor (IReader, "reader module", MOD_APPMODULE, IReader);
ImplementDescriptor (ITransmitter, "transmitter module", MOD_APPMODULE, ITransmitter);

PluginSystem pluginsystem;

void create_mutex (pthread_mutex_t* m)
{
	int result = pthread_mutex_init (m, 0);
	__sassert (!result, "Failed to construct mutex: %s", strerror (result));
}

void destroy_mutex (pthread_mutex_t* m)
{
	int result = pthread_mutex_destroy (m);
	__sassert (!result, "Failed to destroy mutex: %s", strerror (result));
}

void lock_mutex_wait (pthread_mutex_t* m)
{
	int result = pthread_mutex_lock (m);
	__sassert (!result, "Failed to lock mutex: %s", strerror (result));
}

void unlock_mutex (pthread_mutex_t* m)
{
	int result = pthread_mutex_unlock (m);
	__sassert (!result, "Failed to release mutex lock: %s", strerror (result));
}

struct ThreadArg;
struct MessageQueue
{
	size_t queue_id;

	std::map<size_t, IReader*> readers;
	Queue<Buffer> data_queue;

	ITransmitter* transmitter;
	size_t transmitter_id;
	ThreadArg* transmitter_thread;

	pthread_mutex_t queue_mutex;

	MessageQueue (size_t id) :
	queue_id (id),
	readers(),
	data_queue(),
	transmitter (0),
	transmitter_id (0),
	transmitter_thread (0),
	queue_mutex (PTHREAD_MUTEX_INITIALIZER)
	{
		create_mutex (&queue_mutex);
	}

	MessageQueue (MessageQueue&& that) :
	queue_id (that.queue_id),
	readers (std::move (that.readers)),
	data_queue (std::move (that.data_queue)),
	transmitter (that.transmitter),
	transmitter_id (that.transmitter_id),
	transmitter_thread (0),
	queue_mutex (PTHREAD_MUTEX_INITIALIZER)
	{
		that.queue_id = 0;
		that.transmitter = 0;
		that.transmitter_id = 0;

		create_mutex (&queue_mutex);
	}

	~MessageQueue()
	{
		destroy_mutex (&queue_mutex);
	}

	MessageQueue& operator= (MessageQueue&& that)
	{
		if (&that == this)
			return *this;

		readers = std::move (that.readers);
		data_queue = std::move (that.data_queue);

		queue_id = that.queue_id;
		transmitter = that.transmitter;
		transmitter_id = that.transmitter_id;
		that.queue_id = 0;
		that.transmitter = 0;
		that.transmitter_id = 0;

		return *this;
	}
};

std::map<size_t, MessageQueue> message_queues;

struct ThreadArg
{
	MessageQueue* queue;
	size_t id;
	pthread_t thread;
};

std::vector<ThreadArg*> transmitters;

void* queue_reader_thread_func (void* arg_)
{
	ThreadArg* arg = reinterpret_cast<ThreadArg*> (arg_);

	__sassert (arg ->queue, "Invalid queue ptr");

	std::map<size_t, IReader*>::iterator r_iter = arg ->queue ->readers.find (arg ->id);
	__sassert (r_iter != arg ->queue ->readers.end(), "Invalid reader id: %zu", arg ->id);

	IReader* reader = r_iter ->second;

	size_t tp = reader ->Type();
	size_t length;
	void* data;

	for (;;)
	{
		data = 0;
		length = 0;

		if (!arg ->queue ->transmitter)
			break;

		smsg (E_INFO, E_DEBUGAPP, "Queue %zu reader %zu: reading",
		      arg ->queue ->queue_id, arg ->id);

		reader -> Read (&data, &length);

		smsg (E_INFO, E_DEBUGAPP, "Queue %zu reader %zu: read completed [%p:%zu]",
		      arg ->queue ->queue_id, arg ->id, data, length);

		if (!data || !length)
			break;

		for (;;)
		{
			lock_mutex_wait (&arg ->queue ->queue_mutex);
			bool is_overflow = arg ->queue ->data_queue.Full();
			unlock_mutex (&arg ->queue ->queue_mutex);

			if (is_overflow)
			{
				smsg (E_INFO, E_DEBUGLIB, "Queue %zu reader %zu: overflow: spin-wait loop",
				      arg ->queue ->queue_id, arg ->id);

				sleep (1);
				continue;
			}

			break;
		}

		smsg (E_INFO, E_DEBUGAPP, "Queue %zu reader %zu: write",
		      arg ->queue ->queue_id, arg ->id);

		lock_mutex_wait (&arg ->queue ->queue_mutex);
		arg ->queue ->data_queue.Add (Buffer (data, length, tp));
		unlock_mutex (&arg ->queue ->queue_mutex);

		smsg (E_INFO, E_DEBUGAPP, "Queue %zu reader %zu: write completed",
		      arg ->queue ->queue_id, arg ->id);
	}

	smsg (E_INFO, E_DEBUGAPP, "Queue %zu reader %zu: quit",
	      arg ->queue ->queue_id, arg ->id);

	sleep (1);

	lock_mutex_wait (&arg ->queue ->queue_mutex);
	arg ->queue ->readers.erase (arg ->id);
	unlock_mutex (&arg ->queue ->queue_mutex);

	free (arg_);
	return 0;
}

void* queue_transmitter_thread_func (void* arg_)
{
	ThreadArg* arg = reinterpret_cast<ThreadArg*> (arg_);

	__sassert (arg ->queue, "Invalid queue ptr");
	__sassert (arg ->id == arg ->queue ->transmitter_id, "Invalid transmitter id: %zu", arg ->id);

	ITransmitter* transmitter = arg ->queue ->transmitter;

	for (;;)
	{
		// Test for exit condition
		lock_mutex_wait (&arg ->queue ->queue_mutex);
		bool no_readers = arg ->queue ->readers.empty();

		if (no_readers)
		{
			if (arg ->queue ->data_queue.Empty())
			{
				smsg (E_INFO, E_DEBUGAPP, "Queue %zu transmitter %zu: underflow: no readers and empty queue",
				      arg ->queue ->queue_id, arg ->id);

				break;
			}
		}

		unlock_mutex (&arg ->queue ->queue_mutex);

		bool connection = transmitter ->ConnStatus();
		if (!connection)
		{
			smsg (E_CRITICAL, E_USER, "Queue %zu transmitter %zu: transmitter connection aborted",
				  arg ->queue ->queue_id, arg ->id);

			break;
		}

		for (;;)
		{
			lock_mutex_wait (&arg ->queue ->queue_mutex);
			bool empty = arg ->queue ->data_queue.Empty();
			unlock_mutex (&arg ->queue ->queue_mutex);

			if (empty)
			{
				smsg (E_INFO, E_DEBUGLIB, "Queue %zu transmitter %zu: underflow: spin-wait loop",
				      arg ->queue ->queue_id, arg ->id);

				sleep (1);
				continue;
			}

			break;
		}

		smsg (E_INFO, E_DEBUGAPP, "Queue %zu transmitter %zu: read from queue",
		      arg ->queue ->queue_id, arg ->id);

		lock_mutex_wait (&arg ->queue ->queue_mutex);
		Buffer inp_buf = std::move (arg ->queue ->data_queue.Remove());
		unlock_mutex (&arg ->queue ->queue_mutex);

		smsg (E_INFO, E_DEBUGAPP, "Queue %zu transmitter %zu: read completed, transmit",
		      arg ->queue ->queue_id, arg ->id);

		transmitter ->Tx (&inp_buf);

		smsg (E_INFO, E_DEBUGAPP, "Queue %zu transmitter %zu: transmit completed",
		      arg ->queue ->queue_id, arg ->id);
	}

	smsg (E_INFO, E_DEBUGAPP, "Queue %zu transmitter %zu: quit",
		  arg ->queue ->queue_id, arg ->id);

	sleep (1);
	arg ->queue ->transmitter = 0;
	return 0;
}

int main (int argc, char** argv)
{
	Debug::System::Instance().SetTargetProperties (Debug::CreateTarget ("stderr", EVERYTHING, EVERYTHING & ~MASK (Debug::E_DEBUGLIB)),
												   &FXConLog::Instance());


	if (argc == 1)
	{
		fprintf (stderr, "Invalid parameters. Usage: %s\n"
				 "\t -load <module file>:<queue>:<module id> ...\n"
				 "\t -param <parameter>:<queue>:<module id> ...\n\n", argv[0]);
		exit (-1);
	}

	for (int a = 1; a < argc; ++a)
	{
		if (!strcmp (argv[a], "-load"))
		{
			char *file;
			size_t queue_id, module_id;

			int result = sscanf (argv[a + 1], "%a[^:]:%zu:%zu", &file, &queue_id, &module_id);
			__sassert (result == 3, "Invalid parameter syntax: \"%s\"", argv[a + 1]);

			smsg (E_INFO, E_USER, "Adding plugin to system: \"%s\" -> [%zu:%zu]",
				 file, queue_id, module_id);

			PluginSystem::Plugin* handle = pluginsystem.LoadPlugin (file);
			__sassert (handle, "Failure when loading plugin");

			MessageQueue* mq = 0;

			if (!message_queues.count (queue_id))
			{
				smsg (E_INFO, E_DEBUGAPP, "Creating new queue: %zu", queue_id);
				mq = &message_queues.insert (std::make_pair (queue_id, MessageQueue (queue_id))).first ->second;
			}

			else
			{
				smsg (E_INFO, E_DEBUGAPP, "Using queue: %zu", queue_id);
				mq = &message_queues.at (queue_id);
			}

			smsg (E_INFO, E_DEBUGAPP, "Initializing plugin API");

			if (IReader* rinterface = pluginsystem.AttemptInitPlugin<IReader> (handle, reader_init_func))
			{
				__sverify (!mq ->transmitter || (module_id != mq ->transmitter_id),
						   "ID %zu [queue %zu] is owned by transmitter",
						   module_id, queue_id);
				bool ins_ok = mq ->readers.insert (std::make_pair (module_id, rinterface)).second;
				__sverify (ins_ok, "Duplicate reader ID: %zu [queue %zu]", module_id, queue_id);
			}

			else if (ITransmitter* tinterface = pluginsystem.AttemptInitPlugin<ITransmitter> (handle, transmitter_init_func))
			{
				__sverify (!mq ->transmitter, "Only one transmitter per queue is supported!");
				__sverify (!mq ->readers.count (module_id), "ID %zu [queue %zu] is owned by reader",
						   module_id, queue_id);
				mq ->transmitter = tinterface;
				mq ->transmitter_id = module_id;
			}

			else
				__sverify (0, "Unsupported plugin API in module [\"%s\"]", file);

			++a;
		}

		else if (!strcmp (argv[a], "-param"))
		{
			char* par;
			size_t queue_id, module_id;

			int result = sscanf (argv[a + 1], "%a[^:]:%zu:%zu", &par, &queue_id, &module_id);
			__sverify (result == 3, "Invalid parameter syntax: \"%s\"", argv[a + 1]);

			smsg (E_INFO, E_USER, "Attaching parameter to plugin: \"%s\" -> [%zu:%zu]",
				  par, queue_id, module_id);

			auto iter = message_queues.find (queue_id);
			__sverify (iter != message_queues.end(), "Undefined queue: %zu", queue_id);

			auto r_iter = iter ->second.readers.find (module_id);

			if (r_iter != iter ->second.readers.end())
			{
				r_iter ->second ->Set (par);
			}

			else if (iter ->second.transmitter_id == module_id)
			{
				iter ->second.transmitter ->Set (par);
			}

			else
				__sverify (0, "Invalid module ID: %zu [queue %zu]", module_id, queue_id);

			free (par);

			++a;
		}

		else
			__sverify (0, "Invalid parameter \"%s\"", argv[a]);
	} // for (args)

	smsg (E_INFO, E_DEBUGAPP, "Parameters processing completed.");

	for (std::map<size_t, MessageQueue>::value_type& qupair: message_queues)
	{
		size_t readers_count = 0, transmitters_count = 0;
		smsg (E_INFO, E_DEBUGAPP, "Starting queue #%zu", qupair.first);

		MessageQueue& queue = qupair.second;

		if (!queue.transmitter || queue.readers.empty())
		{
			smsg (E_CRITICAL, E_USER,
			      "Failed to start up queue - configuration is not completed: [%zu trans | %zu read]",
			      static_cast<bool> (queue.transmitter),
			      queue.readers.size());

			continue;
		}

		// Start a thread per reader.
		for (std::map<size_t, IReader*>::value_type& rd: queue.readers)
		{
			ThreadArg* arg = reinterpret_cast<ThreadArg*> (malloc (sizeof (ThreadArg)));
			arg ->id = rd.first;
			arg ->queue = &queue;

			pthread_create (&arg ->thread, 0, &queue_reader_thread_func, arg);
			++readers_count;
		}

		// Start a thread for transmitter.
		queue.transmitter ->Connect();

		ThreadArg* t_arg = reinterpret_cast<ThreadArg*> (malloc (sizeof (ThreadArg)));
		t_arg ->id = queue.transmitter_id;
		t_arg ->queue = &queue;

		pthread_create (&t_arg ->thread, 0, &queue_transmitter_thread_func, t_arg);
		queue.transmitter_thread = t_arg;
		++transmitters_count;

		smsg (E_INFO, E_VERBOSELIB, "For queue #%zu: %zu reader threads and %zu transmitter threads",
			  qupair.first, readers_count, transmitters_count);
	}

	smsg (E_INFO, E_USER, "Threads created");

	for (std::map<size_t, MessageQueue>::value_type& qupair: message_queues)
	{
		MessageQueue& queue = qupair.second;

		if (queue.transmitter_thread)
			pthread_join (queue.transmitter_thread ->thread, 0);
	}

	smsg (E_INFO, E_USER, "Threads closed");

	Debug::System::Instance().CloseTargets();
}