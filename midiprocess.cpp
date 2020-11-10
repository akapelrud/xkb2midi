#include "midiprocess.hpp"
#include <stdexcept>
#include <cassert>

#include <jack/midiport.h>

midi_packet::midi_packet(bool on, char n, char v)
	: type(on?0x90:0x80), note(n), velocity(v)
{}

const size_t MidiProcess::RINGBUF_SIZE = 1024;

MidiProcess::MidiProcess()
	: _ringbuf(nullptr), _jclient(nullptr), _port(nullptr)
{}

MidiProcess::MidiProcess(const std::string& name)
	: MidiProcess()
{
	if(name == "")
		throw std::invalid_argument("jack client name cannot be the empty string \"\"");

	_jclient = jack_client_open(name.c_str(), JackNullOption, nullptr);

	if(!_jclient)
		throw std::invalid_argument("Could not connect to jack server.");

	_port = jack_port_register(_jclient, "out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
	if(!_port)
		throw std::runtime_error("Could not create jack midi output port.");
	
	jack_set_process_callback(_jclient, MidiProcess::jack_process_wrap, this);
	
	_ringbuf = jack_ringbuffer_create(RINGBUF_SIZE);
	if(!_ringbuf)
		throw std::runtime_error("Unable to create ringbuffer.");
}

bool MidiProcess::activate()
{
	int res = -1;
	if(_jclient)
		res = jack_activate(_jclient);
	return (res==0);
}

MidiProcess::~MidiProcess()
{
	if(_ringbuf) {
		jack_ringbuffer_free(_ringbuf);
		_ringbuf=nullptr;
	}

	_port = nullptr;

	if(_jclient) {
		jack_deactivate(_jclient);
		jack_client_close(_jclient);
		_jclient=nullptr;
	}
}

bool MidiProcess::enqueue_packet(const midi_packet& packet)
{
	size_t wrote = jack_ringbuffer_write(_ringbuf, packet.v, sizeof(packet.v));
	return (wrote==sizeof(packet.v));
}

int MidiProcess::jack_process_wrap(jack_nframes_t nframes, void* arg)
{
	MidiProcess* mp = static_cast<MidiProcess*>(arg);
	assert(mp!=nullptr);
	return mp->process(nframes);
}

int MidiProcess::process(jack_nframes_t nframes)
{
	void* outBuf = jack_port_get_buffer(_port, nframes);
	jack_midi_clear_buffer(outBuf);
	
	size_t numRead = jack_ringbuffer_read_space(_ringbuf)/sizeof(midi_packet);
	numRead *= sizeof(midi_packet);

	if(numRead > 0) {
		jack_midi_data_t* writeBuf = jack_midi_event_reserve(outBuf, 0, numRead);
		size_t actRead = jack_ringbuffer_read(_ringbuf, reinterpret_cast<char*>(writeBuf), numRead);
		assert(actRead == numRead);
	}

	return 0;
}

