//------------------------------------------------------------------------------
//  tpsplayer.cc
//  sokol_args + sokol_audio + tinyprimesynth
//------------------------------------------------------------------------------

#define SOKOL_IMPL
#include "sokol/sokol_args.h"
#include "sokol/sokol_audio.h"
#include "sokol/sokol_log.h"
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
	const char *parm_check = sargs_value("soundfont");
	if (*parm_check == '\0') {
		print_help();
		sargs_shutdown();
		exit(EXIT_FAILURE);
	} else {
		midi_soundfont = new tinyprimesynth::FileAndMemReader;
		midi_soundfont->open_file(parm_check);
		if (!midi_soundfont->is_valid()) {
			delete midi_soundfont;
			printf("Error opening soundfont %s\n", parm_check);
			sargs_shutdown();
			exit(EXIT_FAILURE);
		}
	}
	parm_check = sargs_value("song");
	if (*parm_check == '\0') {
		print_help();
		sargs_shutdown();
		exit(EXIT_FAILURE);
	} else {
		midi_track = new tinyprimesynth::FileAndMemReader;
		midi_track->open_file(parm_check);
		if (!midi_track->is_valid()) {
			delete midi_soundfont;
			delete midi_track;
			printf("Error opening song %s\n", parm_check);
			sargs_shutdown();
			exit(EXIT_FAILURE);
		}
	}
	parm_check = sargs_value("voices");
	if (*parm_check != '\0') {
		int voices = atoi(parm_check);
		if (voices <= 0) {
			delete midi_soundfont;
			delete midi_track;
			printf("Must have more than 0 voices!\n");
			sargs_shutdown();
			exit(EXIT_FAILURE);
		} else {
			midi_voices = (size_t)voices;
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

	if (!midi_synth->load_soundfont(midi_soundfont)) {
		delete midi_track;
		delete midi_soundfont;
		delete midi_synth;
		saudio_shutdown();
		printf("TPSPlayer: Error loading soundfont\n");
		sargs_shutdown();
		exit(EXIT_FAILURE);
	} else {
		delete midi_soundfont; // no longer needed; soundfont is processed and loaded
	}

	if (!midi_synth->load_song(midi_track)) {
		delete midi_track;
		delete midi_synth;
		saudio_shutdown();
		printf("TPSPlayer: Error loading song\n");
		sargs_shutdown();
		exit(EXIT_FAILURE);
	} else {
		delete midi_track; // no longer needed; track is parsed and events are loaded
	}

	midi_playing = true;

	printf("Playing %s....press enter to exit\n", sargs_value("song"));
	fflush(stdout);
	getchar();

	midi_playing = false;
	delete midi_synth;
	saudio_shutdown();
	sargs_shutdown();

	return EXIT_SUCCESS;
}
