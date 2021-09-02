# xkb2midi
xkb (X11) to JACK midi mapping utility that allows you to remap a standard X keyboard device to output specific midi notes over JACK.

This allows you to take a specific keyboard, rip out all the unused keys and use it as a pedalboard. If the correct device id of the pedalkoard keyboard is supplied to the program, your ordinary keyboard can be kept as a normal, functioning keyboard while the one controlled by this program can be made to only output midi notes. Perfect for e.g. controlling a midi application like sooperlooper. 

## Dependencies
xkb2midi uses cmake for building:

    - boost (program_options)
    - X11 with Xkb
    - jack

## Compilation

```
$ git clone https://github.com/akapelrud/xkb2midi.git
$ mkdir -p xkb2midi/build && cd xkb2midi/build
$ cmake ..
$ make
```

## Usage
```
Allowed options:
  -h [ --help ]                         produce help message
  -c [ --config ] arg (=~/.config/xkb2midi.cfg)
                                        Set configuration file for keycode to 
                                        note mappings.
  -d [ --device ] arg (=256)            Set xkb device index (c.f. `xinput 
                                        list`). Default value is the core 
                                        keyboard, which probably should be 
                                        avoided.
  -u [ --unmap ]                        Prevent mapped keys from generating key
                                        events as usual.

This program reads keycodes and midi note numbers from a config file
and reconfigures a specific keyboard using the XKeyboard Extension.

The program then listens for keyboard events on the mapped keys,
translating keystrokes to midi key press/release events outputted over
a jack connection. The program doesn't have to be in focus to work.

Please be aware that this program can potentially lock up your input handling,
so don't use it on your core keyboard(!)

```
