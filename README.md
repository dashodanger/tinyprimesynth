# tinyprimesynth

## About
TinyPrimeSynth is an MIT-licensed, header-only C++14 MIDI Soundfont2 synthesizer that attempts to abstract away many of the details of rendering MIDI files, providing an interface closer to that of a simple decoder. It is intended for use in game engines or other applications in which real-time playback without granular controls is sufficient. It is similar to the TinySoundFont project (https://github.com/schellingb/TinySoundFont), but provides a more robust implementation (for example, modulator support) at the expense of a somewhat higher overhead and the lack of signed 16-bit output.

It integrates the PrimeSynth (https://github.com/mosmeh/primesynth) and BW_Midi_Sequencer (https://github.com/Wohlstand/BW_Midi_Sequencer) libraries, albeit with substantial modifications.

- Differences from upstream PrimeSynth:
  - Works with GCC/Clang compilers in addition to MSVC
  - Platform-agnostic; renders to a buffer instead of attempting to leverage Portaudio to send to a specific device
  - Does not support real-time input from MIDI devices; messages are sent via the internal sequencer
  - Revised to be closer to the Orthodox C++ style (no use of exceptions or streams, no casts requiring RTTI, C-style I/O, etc)

- Differences from upstream BW_Midi_Sequencer:
  - Support for IMF, CMF, DMX MUS and XMI song formats removed
  - "Real time interface" context removed
  - Support for raw OPL events/playback removed

## Compilation
Define `TINYPRIMESYNTH_IMPLEMENTATION` before including `tinyprimesynth.hpp` in one source file within your project. It can then be included anywhere else that it needs to be referenced.

To compile a test program, please use the CMakeLists file in the `example` directory. It has a command-line interface whose usage can be shown with the 'help' parameter. You can pass either the bundled song and soundfont, or paths to your own.

If both the soundfont and song are successfully loaded, it will display a window while the song plays. The song will loop indefinitely. Close the window at any time to exit the program.

Note that the test program uses the Sokol libraries, which are under the zlib license. It is bundled with `sf_GMbank.sf2` (renamed to `csound.sf2`), a public domain soundfont provided by the CSound project (https://github.com/csound/csound). It is also bundled with the track `ant_farm_melee.mid`, composed by Lee Jackson (https://dleejackson.lbjackson.com/) and used under the CC-BY-SA 4.0 license. None of these licenses affect tinyprimesynth when compiled on its own.

## Usage
- Create a new instance of the Synthesizer class, passing to it your desired output rate.
  - You can additionally pass a value for the maximum number of available voices to use. This defaults to 64 if not provided.
    - If the number of voices is too low, you may hear notes being cutoff as they are released for use. The recommended minimum is 24 in order to adhere to the General MIDI I standard.
- Create a FileAndMemReader instance and load a supported soundfont (SF2) from either a file path, or a pointer to a memory buffer + its size.
- Use the `load_soundfont` function of the Synthesizer class, passing to it a pointer to the soundfont's FileAndMemReader instance.
  - If this function returns false, the soundfont is invalid or malformed.
  - Subsequent calls to `load_soundfont` will delete any soundfont that was previously loaded. TinyPrimeSynth does not support loading multiple soundfonts simultaneously.
  - The soundfont's FileAndMemReader instance can be safely deleted after `load_soundfont` returns.
- Create a FileAndMemReader instance and load a supported track (MIDI, EA MUS, GMF, or RMI) from either a file path or a pointer to a memory buffer + its size.
- Use the `load_song` function of the Synthesizer class, passing to it a pointer to the song's FileAndMemReader instance.
  - If this function returns false, the song is invalid or malformed.
  - Subsequent calls to `load_song` will delete any track that was previously processed. TinyPrimeSynth does not support loading multiple songs simultaneously.
  - The song's FileAndMemReader instance can be safely deleted after `load_song` returns.
- Note that the FileAndMemReader class does not free any memory; if used to load from a buffer instead of a file, this buffer must be freed separately after deleting the FileAndMemReader instance (if not being used otherwise).
- In an appropriate place in your program, call the `play_stream` function of the Synthesizer class, passing a pointer to an initialized buffer to store the generated samples and the buffer's length. In general, the buffer's length should be the desired number of samples x 2 (stereo) x sizeof(float). Generated samples will be in the form of interleaved (L/R/L/R/etc) floats.
- The `pause` and `stop` functions of the Synthesizer class are for issuing the "All Notes Off" and "All Sounds Off" MIDI commands respectively; in many cases simply not calling `play_stream` until you need samples again is sufficient.
- The `at_end` and `rewind` functions of the Synthesizer class can be used to loop the track if desired.
- When finished with playback, call the `reset` function of the Synthesizer class to reset the internal sequencer, voice and channel parameters before attempting to load and play another song. It is also recommended to call either this function or `stop` prior to deleting the Synthesizer instance.
