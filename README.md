# TinyPrimeSynth

## About
TinyPrimeSynth is an MIT-licensed, header-only C++11 MIDI soundfont synthesizer that attempts to abstract away many of the details of rendering MIDI files, providing an interface closer to that of a simple decoder. It is intended for use in game engines or other applications in which real-time playback without granular controls is sufficient. It is similar to the TinySoundFont project (https://github.com/schellingb/TinySoundFont), but provides a more robust implementation (for example, modulator support) at the expense of a somewhat higher overhead and the lack of signed 16-bit output.

It integrates the PrimeSynth (https://github.com/mosmeh/primesynth) and BW_Midi_Sequencer (https://github.com/Wohlstand/BW_Midi_Sequencer) libraries, albeit with substantial modifications.

- Differences from upstream PrimeSynth:
  - Adjustable polyphony
  - Optional support for FLAC-encoded sf2 soundfonts
  - Works with GCC/Clang compilers in addition to MSVC
  - Platform-agnostic; renders to a buffer instead of attempting to leverage Portaudio to send to a specific device
  - Does not support real-time input from MIDI devices; messages are sent via the internal sequencer
  - Revised to be closer to the Orthodox C++ style (no use of exceptions or streams, no casts requiring RTTI, C-style I/O, etc)
  - More forgiving of soundfonts that are missing various presets but are otherwise correctly formed (i.e., no piano or drums)

- Differences from upstream BW_Midi_Sequencer:
  - DMX MUS conversion routines replaced with zlib-licensed implementation by Steve Clark (https://github.com/fawtytoo/Mus2Midi)
  - Support for IMF, CMF and XMI song formats removed
  - "Real time interface" context removed
  - Support for raw OPL events/playback removed
  - No collection or display of song metadata

## Compilation
Define `TINYPRIMESYNTH_IMPLEMENTATION` before including `tinyprimesynth.hpp` in one source file within your project. It can then be included anywhere else that it needs to be referenced.

You may also define `TINYPRIMESYNTH_FLAC_SUPPORT` before `TINYPRIMESYNTH_IMPLEMENTATION` to enable the internal FLAC decoder. This will allow for SF2FLAC (regular sf2 files which are FLAC-encoded) soundfont support. If you are already using a flac decoder in your program, you can leave this undefined and decode the SF2FLAC soundfont prior to loading into TinyPrimeSynth.

Any FLAC encoder may be used to create SF2FLAC files, but a simple encoder can be built with the files in the `sf2flac` directory. sf2flac treats the first argument passed to it as an sf2 file and attempts to encode it accordingly. If you are using your own encoder, it is recommended to treat the SF2 as a series of raw 16-bit signed samples. 

Note that sf2flac uses the tflac library, which is under the BSD0 license. This does not affect TinyPrimeSynth when compiled on its own.

To compile a test program for Windows or Linux, please use the CMakeLists file in the `example` directory. It has a command-line interface whose usage can be shown with the 'help' parameter. You can pass either the bundled song and soundfont, or paths to your own.

If both the soundfont and song are successfully loaded, playback will begin. The song will loop indefinitely. Press enter or issue a break command to exit at any time.

Note that the test program uses the Sokol libraries, which are under the zlib license. It is bundled with `sf_GMbank.sf2` (encoded and renamed to `csound.sf2flac`), a public domain soundfont provided by the CSound project (https://github.com/csound/csound). It is also bundled with the track `ant_farm_melee.mid`, composed by Lee Jackson (https://dleejackson.lbjackson.com/) and used under the CC-BY-SA 4.0 license. None of these licenses affect TinyPrimeSynth when compiled on its own.

## Usage
- Create a new instance of the Synthesizer class, passing to it your desired output rate.
  - You can additionally pass a value for the maximum number of available voices to use. This defaults to 64 if not provided.
    - If the number of voices is too low, you may hear notes being cut off as they are released for use. The recommended minimum is 24 in order to adhere to the General MIDI I standard.

- Use the `load_soundfont` function of the Synthesizer instance, passing to it either a file path or a pointer to a buffer in memory and its size.
  - SF2 is currently the only supported format
  - If this function returns false, the soundfont is invalid or malformed.
  - Subsequent calls to `load_soundfont` will delete any soundfont that was previously loaded. TinyPrimeSynth does not support loading multiple soundfonts simultaneously.

- Use the `load_song` function of the Synthesizer instance, passing to it either a file path or a pointer to a buffer in memory and its size.
  - Supported song formats are MIDI, DMX MUS ("Doom" format), EA MUS, GMF, or RMI
  - If this function returns false, the song is invalid or malformed.
  - Subsequent calls to `load_song` will delete any track that was previously processed. TinyPrimeSynth does not support loading multiple songs simultaneously.

- Note that the Synthesizer class will not free any memory upon completion of the `load_soundfont` or `load_song` functions; if used to load from a buffer instead of a file, this buffer must be freed separately (if not being used otherwise).

- In an appropriate place in your program, call the `play_stream` function of the Synthesizer class, passing a pointer to an initialized buffer to store the generated samples and the buffer's length. In general, the buffer's length should be the desired number of samples x 2 (stereo) x sizeof(float). Generated samples will be in the form of interleaved (L/R/L/R/etc) floats.

- The `pause` and `stop` functions of the Synthesizer class are for issuing the "All Notes Off" and "All Sounds Off" MIDI commands respectively; in many cases simply not calling `play_stream` until you need samples again is sufficient.

- The `at_end` and `rewind` functions of the Synthesizer class can be used to loop the track if desired.

- When finished with playback, call the `reset` function of the Synthesizer class to reset the internal sequencer, voice and channel parameters before attempting to load and play another song.
