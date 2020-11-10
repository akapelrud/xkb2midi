#ifndef JACK_MIDIPROCESS_HPP_
#define JACK_MIDIPROCESS_HPP_

#include <string>
#include <jack/jack.h>
#include <jack/ringbuffer.h>

struct midi_packet
{
	union{
		struct{ char type, note, velocity; };
		char v[3];
	};

	midi_packet(bool on, char n, char velocity);
};

class MidiProcess
{
	/* Just to allow partially constructed object, so that jack
	   initialization can be done in the other ctors.
	  */
	MidiProcess();

public:
	MidiProcess(const std::string& name);
	~MidiProcess();
	
	bool activate();

	bool enqueue_packet(const midi_packet& packet);

	static int jack_process_wrap(jack_nframes_t nframes, void* arg);

public:
	static const size_t RINGBUF_SIZE;

private:
	int process(jack_nframes_t nframes);

private:
	jack_ringbuffer_t* _ringbuf;
	jack_client_t* _jclient;
	jack_port_t* _port;
};


#endif

