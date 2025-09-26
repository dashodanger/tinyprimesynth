//------------------------------------------------------------------------------------------------
//  tpsplayer.cc
//  sokol_args + sokol_audio + tinyprimesynth
//-------------------------------------------------------------------------------------------------
//
// Copyright (c) 2025 dashodanger
// Copyright (c) 2017 Andre Weissflog (original player example code)
//
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//------------------------------------------------------------------------------------------------

#define SOKOL_IMPL
#include "sokol/sokol_args.h"
#include "sokol/sokol_audio.h"
#include "sokol/sokol_log.h"
#define TINYPRIMESYNTH_FLAC_SUPPORT
#define TINYPRIMESYNTH_IMPLEMENTATION
#include "../tinyprimesynth.hpp"
#include <math.h>
#include <stdint.h>
#include <string.h>

static bool midi_playing = false;
static tinyprimesynth::Synthesizer *midi_synth = nullptr;
static tinyprimesynth::FileAndMemReader *midi_soundfont = nullptr;
static tinyprimesynth::FileAndMemReader *midi_track = nullptr;
static size_t midi_voices = 64;

// stream callback, called by sokol_audio when new samples are needed,
// on most platforms, this runs on a separate thread
static void stream_cb(float *buffer, int num_frames, int num_channels) {
	if (midi_playing) {
		midi_synth->play_stream((uint8_t *)buffer, num_frames * num_channels * sizeof(float));
		if (midi_synth->at_end()) {
			midi_synth->rewind();
		}
	} else {
		memset(buffer, 0, num_frames * num_channels * sizeof(float));
	}
}

static void print_help() {
	printf("\nUsage: tpsplayer soundfont=file song=file [voices=count]\n"
		   "\n"
		   "Supported soundfont formats:\n"
		   "- SF2\n"
		   "\n"
		   "Supported song formats:\n"
		   "- MIDI\n"
		   "- MUS (DMX/Doom format)\n"
		   "- MUS (Electronic Arts)\n"
		   "- RMI\n"
		   "- GMF\n"
		   "\n"
		   "Recommend voice count of at least 24 to meet General MIDI I requirements\n"
		   "\n");
}

int main(int argc, char *argv[]) {
	sargs_desc init_args;
	memset(&init_args, 0, sizeof(sargs_desc));
	init_args.argc = argc;
	init_args.argv = argv;
	sargs_setup(init_args);

	if (sargs_num_args() == 1 || sargs_exists("help")) {
		print_help();
		sargs_shutdown();
		exit(EXIT_SUCCESS);
	}
	const char *soundfont = sargs_value("soundfont");
	if (*soundfont == '\0') {
		print_help();
		sargs_shutdown();
		exit(EXIT_FAILURE);
	}
	const char *song = sargs_value("song");
	if (*song == '\0') {
		print_help();
		sargs_shutdown();
		exit(EXIT_FAILURE);
	}
	const char *voices = sargs_value("voices");
	if (*voices != '\0') {
		int num_voices = atoi(voices);
		if (num_voices <= 0) {
			printf("Must have more than 0 voices!\n");
			sargs_shutdown();
			exit(EXIT_FAILURE);
		} else {
			midi_voices = (size_t)num_voices;
		}
	}

	// setup sokol_audio (default sample rate is 44100Hz)
	saudio_desc init_saudio;
	memset(&init_saudio, 0, sizeof(saudio_desc));
	init_saudio.num_channels = 2;
	init_saudio.stream_cb = stream_cb;
	init_saudio.logger.func = slog_func;
	saudio_setup(&init_saudio);

	midi_synth = new tinyprimesynth::Synthesizer(saudio_sample_rate(), midi_voices);

	if (!midi_synth->load_soundfont(soundfont)) {
		delete midi_synth;
		saudio_shutdown();
		printf("TPSPlayer: Error loading soundfont\n");
		sargs_shutdown();
		exit(EXIT_FAILURE);
	}

	if (!midi_synth->load_song(song)) {
		delete midi_synth;
		saudio_shutdown();
		printf("TPSPlayer: Error loading song\n");
		sargs_shutdown();
		exit(EXIT_FAILURE);
	}

	midi_playing = true;

	printf("Playing %s....press enter to exit\n", sargs_value("song"));
	fflush(stdout);
	getchar();

	midi_playing = false;
	midi_synth->stop();
	delete midi_synth;
	saudio_shutdown();
	sargs_shutdown();

	return EXIT_SUCCESS;
}
