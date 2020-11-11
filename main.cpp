#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <exception>
#include <string>
#include <cctype>
#include <boost/program_options.hpp>
#include <signal.h>

#include "midiprocess.hpp"

typedef std::map<KeyCode,char> KeyCodeMidiMap_t;

static void signal_handler(int sig);
KeyCodeMidiMap_t parse_config(const std::string& filename);
XkbDescPtr bind_keys(Display* display, int device, const KeyCodeMidiMap_t& keycodes, bool generateEvents);

int main (int argc, char ** argv)
{
	const std::string JACK_CLIENT_NAME("xkb2midi");

	/*sigaction sigact;
	sigact.sa_handler = signal_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, nullptr);*/

	std::string homepath = getenv("HOME");
	int device;
	std::string cfgFilename;

	namespace po = boost::program_options;
	po::options_description desc("Allowed options");
	desc.add_options()
		("help,h", "produce help message")
		("config,c",
			po::value<std::string>(&cfgFilename)->default_value(homepath + "/.config/xkb2midi.cfg"),
			"Set configuration file for keycode to note mappings.")
		("device,d",
			po::value<int>(&device)->default_value(XkbUseCoreKbd),
			"Set xkb device index (c.f. `xinput list`). Default value is the core keyboard, which"
			" probably should be avoided.")
		("unmap,u", "Prevent mapped keys from generating key events as usual.")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if(vm.count("help")) {
		std::cerr << desc << std::endl;
		std::cerr <<
			"This program reads keycodes and midi note numbers from a config file\n"
			"and reconfigures a specific keyboard using the XKeyboard Extension.\n"
			"\nThe program then listens for keyboard events on the mapped keys,\n"
			"translating keystrokes to midi key press/release events outputted over\n"
                        "a jack connection. The program doesn't have to be in focus to work.\n"
			"\nPlease be aware that this program can potentially lock up your input handling,\n"
			"so don't use it on your core keyboard(!)\n" << std::endl;
		return 1;
	}

	bool generateEvents = !vm.count("unmap");

        std::map<KeyCode,char> kcNoteMap;
        try{
            std::map<KeyCode,char> kcNoteMap = parse_config(cfgFilename);
        }catch(std::invalid_argument& e)
        {
            std::cerr << "Unable to parse config file: '" << cfgFilename << "' - File does not exist." << std::endl;
            return 1;
        }

	if(device == XkbUseCoreKbd)
	{
		std::cout << "Are you sure you want to use the Core keyboard? This might make your system inoperable. (y/N) " << std::flush;
		char c;
		std::cin >> std::noskipws >> c;
		if(c!='y' && c!='Y') {
			std::cout << "Exiting ..." << std::endl;
			exit(1);
		}
		std::cout << std::endl;
	}

	// connect to X and XKeyboard Extension
	int event_base, error_base, open_result;
	Display *display = XkbOpenDisplay(nullptr, &event_base, &error_base, nullptr, nullptr, &open_result);
 
	if ( !display ) {
		std::cerr << "Failed to open X display\n" << std::endl;
		exit(1);
	}

	XkbDescPtr kbd = XkbGetMap(display, XkbKeySymsMask|XkbKeyActionsMask, device);
	if(kbd == nullptr) {
		std::cerr << "Unable to get map for device #" << device << std::endl;
		return 1;
	}
		
	kbd = bind_keys(display, device, kcNoteMap, generateEvents);

	MidiProcess mp(JACK_CLIENT_NAME);
	mp.activate();
	
	XkbSelectEvents(
		display, device,
		XkbActionMessageMask|XkbMapNotifyMask|XkbNewKeyboardNotifyMask,
		XkbActionMessageMask|XkbMapNotifyMask|XkbNewKeyboardNotifyMask);

	XEvent ev;
	while(XNextEvent(display, &ev) == Success)
	{
		if(ev.type == (event_base+XkbEventCode))
		{
			XkbEvent& xkbEvent = reinterpret_cast<XkbEvent&>(ev);
			switch(xkbEvent.any.xkb_type)
			{
				case XkbNewKeyboardNotify:
					if(xkbEvent.new_kbd.device != xkbEvent.new_kbd.old_device)
						break;// otherwise run MapNotify code - should be refactored.
				case XkbMapNotify:
					XkbRefreshKeyboardMapping(&xkbEvent.map);
					XkbSelectEvents(display, device, XkbMapNotifyMask, 0);
					kbd = bind_keys(display, device, kcNoteMap, generateEvents);
					XkbSelectEvents(display, device, XkbMapNotifyMask, XkbMapNotifyMask);
					break;
				case XkbActionMessage:
					{
						std::cout << "Got action message." << std::endl;
						// remove repeated presses.
						auto it = kcNoteMap.find(xkbEvent.message.keycode);
						if(it != kcNoteMap.end())
							mp.enqueue_packet(midi_packet(xkbEvent.message.press, it->second, 100));
					}
					break;
				default:
					std::cout << "Unknown xkb event: " << std::hex << xkbEvent.any.xkb_type << std::endl;
					break;
			}
		}
	}
 
	XCloseDisplay( display );
	return 0;
}

KeyCodeMidiMap_t parse_config(const std::string& filename)
{
	KeyCodeMidiMap_t res;
	std::ifstream fstr(filename);
	if(!fstr)
		throw std::invalid_argument("File: '"+filename+"' does not exist.");
	std::string line;

	std::cerr << "Parsing: '" << filename << "'" << std::endl;

	auto is_only_whitespace =
		[](const std::string& str, size_t pos=0){
			return std::all_of(std::next(str.begin(),pos), str.end(), [](char c){ return std::isspace(c);} );
		};

	unsigned int lineno = 1;
	while(!fstr.eof()) {
		std::getline(fstr, line);

		size_t found = line.find(";"); // comment
		if(found!=std::string::npos)
			line = line.substr(0, found);
		
		found = line.find("=");
		if(found!=std::string::npos) {
			std::string kcStr		= line.substr(0, found);
			std::string midiNoteStr = line.substr(found+1, std::string::npos);

			size_t pos;
			unsigned long kc, midiNote;
			try{
				kc = std::stoul(kcStr, &pos);
				if(pos!=kcStr.size() && !is_only_whitespace(kcStr, pos))
					throw std::invalid_argument(kcStr);

				midiNote = std::stoul(midiNoteStr, &pos);
				if(pos!=midiNoteStr.size() && !is_only_whitespace(midiNoteStr, pos))
					throw std::invalid_argument(midiNoteStr);

				std::cout << "kc " << kc << " :> note #" << midiNote << std::endl;
				res.emplace(std::make_pair(static_cast<KeyCode>(kc), midiNote & 0x7F));
				
			}catch(std::invalid_argument e){
				std::cerr << "Unable to parse line " << lineno << ": '" << e.what() << "'" << std::endl;
			}
		}else if(!is_only_whitespace(line)){
			std::cerr << "Unable to parse line " << lineno << ": '" << line << "'" << std::endl;
		}

		lineno++;
	}

	return res;
}

XkbDescPtr bind_keys(Display* display, int device, const KeyCodeMidiMap_t& keycodes, bool generateEvents)
{
	XkbDescPtr kbd = XkbGetMap(display, XkbKeySymsMask|XkbKeyActionsMask, device);

	for(const auto& pkc : keycodes)
	{
		auto kc = pkc.first;
		int nSyms = XkbKeyNumSyms(kbd, kc);

		if(XkbKeyActionEntry(kbd, kc, 0, 0) == nullptr)
		{
			std::cout << "Adding Xkb action to keycode 0x"
				<< std::hex << static_cast<unsigned int>(kc) << std::endl;
			assert(nSyms > 0);
			if(XkbResizeKeyActions(kbd, kc, 1) == nullptr)
			{
				std::cerr << "Unable to resize action field for keycode 0x"
					<< std::hex << static_cast<unsigned int>(kc) << std::endl;
				continue;
			}
		}

		XkbMessageAction ma;
		memset(&ma, 0, sizeof(ma));
		ma.type = XkbSA_ActionMessage;
		ma.flags = XkbSA_MessageOnPress|XkbSA_MessageOnRelease;
		if(generateEvents)
			ma.flags |= XkbSA_MessageGenKeyEvent;
		strncpy(reinterpret_cast<char*>(ma.message), "!", XkbActionMessageLength);

		XkbAction* ka = XkbKeyActionEntry(kbd, kc, 0, 0);
		assert(ka != nullptr);
		ka->msg = ma; // copy by value

		XkbMapChangesRec changes;
		memset(&changes, 0, sizeof(changes));
		changes.changed |= XkbKeyActionsMask;
		changes.first_key_act = kc;
		changes.num_key_acts = 1;

		if(!XkbChangeMap(display, kbd, &changes))
		{
			std::cerr << "Unable to add action to keycode 0x" << std::hex << static_cast<unsigned int>(kc) << std::endl;
			continue;
		}
	}

	return kbd;
}

void signal_handler(int signo)
{
	if(signo==SIGINT) exit(1);
}

