//------------------------------------------------------------------------------
//  tpsplayer-sapp.c
//  sokol_app + sokol_audio + tinyprimesynth
//  This uses the user-data callback model both for sokol_app.h and
//  sokol_audio.h
//------------------------------------------------------------------------------

#define SOKOL_IMPL
#include "sokol/sokol_app.h"
#include "sokol/sokol_args.h"
#include "sokol/sokol_audio.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
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

static void read_samples(float *buffer, int num_samples) {
	// NOTE: rendered samples are interleaved
	// (e.g. left/right/left/right/...)
	int played = midi_synth->play_stream((uint8_t *)buffer, num_samples * sizeof(float));
	if (midi_synth->at_end())
		midi_synth->rewind();
}

// stream callback, called by sokol_audio when new samples are needed,
// on most platforms, this runs on a separate thread
static void stream_cb(float *buffer, int num_frames, int num_channels, void *user_data) {
	(void)user_data;
	if (midi_playing) {
		const int num_samples = num_frames * num_channels;
		read_samples(buffer, num_samples);
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
		   "- MUS (Electronic Arts)\n"
		   "- RMI\n"
		   "- GMF\n"
		   "\n"
		   "Recommend voice count of at least 24 to meet General MIDI I requirements\n"
		   "\n");
}

static void init(void *user_data) {
	if (sargs_num_args() == 1 || sargs_exists("help")) {
		print_help();
		exit(EXIT_SUCCESS);
	}
	const char *parm_check = sargs_value("soundfont");
	if (*parm_check == '\0') {
		print_help();
		exit(EXIT_FAILURE);
	} else {
		midi_soundfont = new tinyprimesynth::FileAndMemReader;
		midi_soundfont->open_file(parm_check);
		if (!midi_soundfont->is_valid()) {
			delete midi_soundfont;
			printf("Error opening soundfont %s\n", parm_check);
			exit(EXIT_FAILURE);
		}
	}
	parm_check = sargs_value("song");
	if (*parm_check == '\0') {
		print_help();
		exit(EXIT_FAILURE);
	} else {
		midi_track = new tinyprimesynth::FileAndMemReader;
		midi_track->open_file(parm_check);
		if (!midi_track->is_valid()) {
			delete midi_soundfont;
			delete midi_track;
			printf("Error opening song %s\n", parm_check);
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
			exit(EXIT_FAILURE);
		} else {
			midi_voices = (size_t)voices;
		}
	}

	// setup sokol_gfx
	sg_desc init_sg;
	memset(&init_sg, 0, sizeof(sg_desc));
	init_sg.context = sapp_sgcontext();
	init_sg.logger.func = slog_func;
	sg_setup(init_sg);

	// setup sokol_audio (default sample rate is 44100Hz)
	saudio_desc init_saudio;
	memset(&init_saudio, 0, sizeof(saudio_desc));
	init_saudio.num_channels = 2;
	init_saudio.stream_userdata_cb = stream_cb;
	init_saudio.logger.func = slog_func;
	saudio_setup(&init_saudio);

	midi_synth = new tinyprimesynth::Synthesizer(saudio_sample_rate(), midi_voices);

	if (!midi_synth->load_soundfont(midi_soundfont)) {
		delete midi_track;
		delete midi_soundfont;
		delete midi_synth;
		printf("TPSPlayer: Error loading soundfont\n");
		exit(EXIT_FAILURE);
	} else {
		delete midi_soundfont; // no longer needed; soundfont is processed and loaded
	}

	if (!midi_synth->load_song(midi_track)) {
		delete midi_track;
		delete midi_synth;
		printf("TPSPlayer: Error loading song\n");
		exit(EXIT_FAILURE);
	} else {
		delete midi_track; // no longer needed; track is parsed and events are loaded
	}

	midi_playing = true;
}

static void frame(void *user_data) {
	(void)user_data;
	sg_pass_action pass_action;
	memset(&pass_action, 0, sizeof(sg_pass_action));
	pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
	pass_action.colors[0].clear_value = { 0.4f, 0.7f, 1.0f, 1.0f };
	sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
	sg_end_pass();
	sg_commit();
}

static void cleanup(void *user_data) {
	(void)user_data;
	midi_playing = false;
	midi_synth->stop();
	delete midi_synth;
	saudio_shutdown();
	sg_shutdown();
}

sapp_desc sokol_main(int argc, char *argv[]) {
	sargs_desc init_args;
	memset(&init_args, 0, sizeof(sargs_desc));
	init_args.argc = argc;
	init_args.argv = argv;
	sargs_setup(init_args);
	sapp_desc init_sapp;
	memset(&init_sapp, 0, sizeof(sapp_desc));
	init_sapp.init_userdata_cb = init;
	init_sapp.frame_userdata_cb = frame;
	init_sapp.cleanup_userdata_cb = cleanup;
	init_sapp.user_data = NULL;
	init_sapp.width = 400;
	init_sapp.height = 300;
	init_sapp.window_title = "Sokol Audio + TinyPrimeSynth";
	init_sapp.icon.sokol_default = true;
	init_sapp.logger.func = slog_func;
#ifdef _WIN32
	init_sapp.win32_console_attach = true;
#endif
	return init_sapp;
}
