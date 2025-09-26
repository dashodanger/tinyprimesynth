//------------------------------------------------------------------------------------------------
//  TinyPrimeSynth
//------------------------------------------------------------------------------------------------
//
// Copyright (c) 2025 dashodanger
// Copyright (c) 2015-2025 Vitaly Novichkov "Wohlstand" (original BW_Midi_Sequencer implementation)
// Copyright (c) 2018 mosm (original PrimeSynth implementation)
// Copyright (c) 2017 Project Nayuki.(Original FLAC decoder implementation)
//                    https://www.nayuki.io/page/simple-flac-implementation)
// Copyright (c) 2014 Miro Samek (portable CLZ fallback implementation)
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
//
// MUS-to-MIDI conversion adapted from code Copyright (c) 2021-2022 Steve Clark
// with the following license (zlib):
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
//------------------------------------------------------------------------------------------------

#ifndef TINYPRIMESYNTH_HEADER
#define TINYPRIMESYNTH_HEADER

#include <stdint.h>
#include <stdio.h>
#include <vector>

namespace tinyprimesynth {
class Synthesizer {
public:
	Synthesizer(float p_rate, size_t p_voices = 64);
	~Synthesizer();

	bool load_soundfont(const char *p_filename);
	bool load_soundfont(const uint8_t *p_data, size_t p_length);
	bool load_song(const char *p_filename);
	bool load_song(const uint8_t *p_data, size_t p_length);
	int play_stream(uint8_t *p_stream, size_t p_length);
	void set_volume(float p_volume);
	void pause();
	void stop();
	void reset();
	bool at_end() const;
	void rewind();
	bool get_load_error() const;
	void set_load_error(bool p_error);

private:
	enum class Standard {
		GM,
		GS,
		XG
	};

	class Channel;
	struct Preset;
	class Sequencer;
	class SoundFont;
	class Voice;

	Standard standard;
	bool no_drums, no_piano;
	float volume;
	bool load_error;
	std::vector<Channel *> channels;
	std::vector<Voice *> voices;
	SoundFont *soundfont;
	Sequencer *sequencer;

	const Preset *find_preset(uint16_t p_bank, uint16_t p_id);
};

} // namespace tinyprimesynth

#endif // TINYPRIMESYNTH_HEADER

#ifdef TINYPRIMESYNTH_IMPLEMENTATION

#include <limits.h>
#include <math.h>
#include <string.h>
#include <list>
#include <set>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif
namespace tinyprimesynth {

static constexpr char MUS_MAGIC[4] = { 'M', 'U', 'S', 0x1a };
static constexpr char MID_MAGIC[4] = { 'M', 'T', 'h', 'd' };
static constexpr char TRACK_MAGIC[4] = { 'M', 'T', 'r', 'k' };
static constexpr char FLAC_MAGIC[4] = { 'f', 'L', 'a', 'C' };
static constexpr int MUS_CONTROLLER_MAP[16] = { -1, 0, 1, 7, 10, 11, 91, 93, 64, 67, 120, 123, 126, 127, 121, -1 };
static constexpr size_t MIDI_PARSE_HEADER_SIZE = 14;
static constexpr uint8_t PERCUSSION_CHANNEL = 9;
static constexpr uint8_t MAX_KEY = 127;
static constexpr size_t NUM_CHANNELS = 16;
static constexpr size_t NUM_CONTROLLERS = 128;
static constexpr uint16_t PERCUSSION_BANK = 128;
static constexpr float PAN_FACTOR = 3.141592653589793f / 2000.0f;
static constexpr unsigned int CALC_INTERVAL = 64;
static constexpr float ATTEN_FACTOR = 0.4f;
static constexpr uint32_t COARSE_UNIT = 32768;
static constexpr size_t NUM_GENERATORS = 62;
static constexpr uint32_t FOUR_CC_RIFF = 1179011410;
static constexpr uint32_t FOUR_CC_SFBK = 1801610867;
static constexpr uint32_t FOUR_CC_LIST = 1414744396;
static constexpr uint32_t FOUR_CC_INFO = 1330007625;
static constexpr uint32_t FOUR_CC_SDTA = 1635017843;
static constexpr uint32_t FOUR_CC_PDTA = 1635017840;
static constexpr uint32_t FOUR_CC_IFIL = 1818846825;
static constexpr uint32_t FOUR_CC_INAM = 1296125513;
static constexpr uint32_t FOUR_CC_SMPL = 1819307379;
static constexpr uint32_t FOUR_CC_PHDR = 1919182960;
static constexpr uint32_t FOUR_CC_PBAG = 1734435440;
static constexpr uint32_t FOUR_CC_PMOD = 1685024112;
static constexpr uint32_t FOUR_CC_PGEN = 1852139376;
static constexpr uint32_t FOUR_CC_INST = 1953721961;
static constexpr uint32_t FOUR_CC_IBAG = 1734435433;
static constexpr uint32_t FOUR_CC_IMOD = 1685024105;
static constexpr uint32_t FOUR_CC_IGEN = 1852139369;
static constexpr uint32_t FOUR_CC_SHDR = 1919182963;
static constexpr int16_t DEFAULT_GENERATOR_VALUES[NUM_GENERATORS] = {
	0, 0, 0, 0, 0, 0, 0, 0, 13500, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, -12000, 0, -12000, 0, -12000, -12000, -12000, -12000, 0, -12000, 0,
	0, -12000, -12000, -12000, -12000, 0, -12000, 0, 0, 0, 0, 0, 0, 0, -1, -1,
	0, 0, 0, 0, 0, 0, 0, 0, 100, 0, -1, 0, 0, 0
};
static constexpr unsigned char GM_SYSTEM_ON[6] = { 0xf0, 0x7e, 0, 0x09, 0x01, 0xf7 };
static constexpr unsigned char GM_SYSTEM_OFF[6] = { 0xf0, 0x7e, 0, 0x09, 0x02, 0xf7 };
static constexpr unsigned char GS_RESET[11] = { 0xf0, 0x41, 0, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41, 0xf7 };
static constexpr unsigned char GS_SYSTEM_MODE_SET1[11] = { 0xf0, 0x41, 0, 0x42, 0x12, 0x00,
	0x00, 0x7f, 0x00, 0x01, 0xf7 };
static constexpr unsigned char GS_SYSTEM_MODE_SET2[11] = { 0xf0, 0x41, 0, 0x42, 0x12, 0x00,
	0x00, 0x7f, 0x01, 0x00, 0xf7 };
static constexpr unsigned char XG_SYSTEM_ON[9] = { 0xf0, 0x43, 0, 0x4c, 0x00, 0x00, 0x7e, 0x00, 0xf7 };
static constexpr uint16_t ATTEN_TABLE_SIZE = 1441;
static constexpr uint16_t CENT_TABLE_SIZE = 1200;
static float attenuation_to_amp_table[ATTEN_TABLE_SIZE];
static float cent_to_hertz_table[CENT_TABLE_SIZE];
static uint8_t *mus_to_midi_data = NULL;
static int mus_to_midi_size;
static uint8_t *mus_to_midi_pos = NULL;
static int mus_to_midi_eot;
static uint8_t mus_to_midi_delta_bytes[4];
static int mus_to_midi_delta_count;
static uint8_t mus_to_midi_channels[16];

enum class SF2Generator : uint16_t {
	START_ADDRESS_OFFSET = 0,
	END_ADDRESS_OFFSET = 1,
	START_LOOP_ADDRESS_OFFSET = 2,
	END_LOOP_ADDRESS_OFFSET = 3,
	START_ADDRESS_COARSE_OFFSET = 4,
	MOD_LFO_TO_PITCH = 5,
	VIB_LFO_TO_PITCH = 6,
	MOD_ENV_TO_PITCH = 7,
	INITIAL_FILTER_FC = 8,
	INITIAL_FILTER_Q = 9,
	MOD_LFO_TO_FILTER_FC = 10,
	MOD_ENV_TO_FILTER_FC = 11,
	END_ADDRESS_COARSE_OFFSET = 12,
	MOD_LFO_TO_VOLUME = 13,
	CHORUS_EFFECTS_SEND = 15,
	REVERB_EFFECTS_SEND = 16,
	PAN = 17,
	DELAY_MOD_LFO = 21,
	FREQ_MOD_LFO = 22,
	DELAY_VIB_LFO = 23,
	FREQ_VIB_LFO = 24,
	DELAY_MOD_ENV = 25,
	ATTACK_MOD_ENV = 26,
	HOLD_MOD_ENV = 27,
	DECAY_MOD_ENV = 28,
	SUSTAIN_MOD_ENV = 29,
	RELEASE_MOD_ENV = 30,
	KEY_NUM_TO_MOD_ENV_HOLD = 31,
	KEY_NUM_TO_MOD_ENV_DECAY = 32,
	DELAY_VOL_ENV = 33,
	ATTACK_VOL_ENV = 34,
	HOLD_VOL_ENV = 35,
	DECAY_VOL_ENV = 36,
	SUSTAIN_VOL_ENV = 37,
	RELEASE_VOL_ENV = 38,
	KEY_NUM_TO_VOL_ENV_HOLD = 39,
	KEY_NUM_TO_VOL_ENV_DECAY = 40,
	INSTRUMENT = 41,
	KEY_RANGE = 43,
	VELOCITY_RANGE = 44,
	START_LOOP_ADDRESS_COARSE_OFFSET = 45,
	KEY_NUMBER = 46,
	VELOCITY = 47,
	INITIAL_ATTENUATION = 48,
	END_LOOP_ADDRESS_COARSE_OFFSET = 50,
	COARSE_TUNE = 51,
	FINE_TUNE = 52,
	SAMPLE_ID = 53,
	SAMPLE_MODES = 54,
	SCALE_TUNING = 56,
	EXCLUSIVE_CLASS = 57,
	OVERRIDING_ROOT_KEY = 58,
	END_OPERATOR = 60,
	PITCH, // non-standard generator, used as a destination of default pitch bend modulator
	LAST
};

enum class GeneralController : uint8_t {
	NO_CONTROLLER = 0,
	NOTE_ON_VELOCITY = 2,
	NOTE_ON_KEY_NUMBER = 3,
	POLYPHONIC_PRESSURE = 10,
	CHANNEL_PRESSURE = 13,
	PITCH_WHEEL = 14,
	PITCH_WHEEL_SENSITIVITY = 16,
	LINK = 127
};

enum class ControllerPalette {
	GENERAL = 0,
	MIDI = 1
};

enum class SourceDirection {
	POSITIVE = 0,
	NEGATIVE = 1
};

enum class SourcePolarity {
	UNIPOLAR = 0,
	BIPOLAR = 1
};

enum class SourceType {
	LINEAR = 0,
	CONCAVE = 1,
	CONVEX = 2,
	SWITCH = 3
};

enum class Transform : uint16_t {
	LINEAR = 0,
	ABSOLUTE_VALUE = 2
};

#pragma pack(push, 1)
struct MUSHeader {
	char id[4];
	uint16_t score_length;
	uint16_t score_start;
};

struct MIDHeader {
	char id[4];
	int length;
	uint16_t type;
	uint16_t ntracks;
	uint16_t ticks;
};
#pragma pack(pop)

#if defined(__GNUC__) || defined(__clang__)
static inline uint16_t byte_swap_16(uint16_t n) {
	return __builtin_bswap16(n);
}
static inline uint32_t byte_swap_32(uint32_t n) {
	return __builtin_bswap32(n);
}
#elif defined(_MSC_VER)
static inline uint16_t byte_swap_16(uint16_t n) {
	return _byteswap_ushort(n);
}
static inline uint32_t byte_swap_32(uint32_t n) {
	return _byteswap_ulong(n);
}
#else
static inline uint16_t byte_swap_16(uint16_t n) {
	uint16_t a;
	a = (n & 0xFF) << 8;
	a |= (n >> 8) & 0xFF;
	return a;
}
static inline uint32_t byte_swap_32(uint32_t n) {
	uint32_t a;
	a = (n & 0xFFU) << 24;
	a |= (n & 0xFF00U) << 8;
	a |= (n >> 8) & 0xFF00U;
	a |= (n >> 24) & 0xFFU;
	return a;
}
#endif

static inline uint64_t read_int_big_endian(const void *p_buffer, size_t p_nbytes) {
	uint64_t result = 0;
	const uint8_t *data = (const uint8_t *)(p_buffer);

	for (size_t n = 0; n < p_nbytes; ++n) {
		result = (result << 8) + data[n];
	}

	return result;
}

static inline uint64_t read_variable_length_value(const uint8_t **p_ptr, const uint8_t *p_end, bool &p_ok) {
	uint64_t result = 0;
	p_ok = false;

	for (;;) {
		if (*p_ptr >= p_end) {
			return 2;
		}
		unsigned char byte = *((*p_ptr)++);
		result = (result << 7) + (byte & 0x7F);
		if (!(byte & 0x80)) {
			break;
		}
	}

	p_ok = true;
	return result;
}

static void initialize_conversion_tables() {
	static bool initialized = false;
	if (!initialized) {
		initialized = true;

		for (size_t i = 0; i < ATTEN_TABLE_SIZE; ++i) {
			// -200 instead of -100 for compatibility
			attenuation_to_amp_table[i] = pow(10.0f, i / -200.0f);
		}
		for (size_t i = 0; i < CENT_TABLE_SIZE; i++) {
			cent_to_hertz_table[i] = 6.875 * exp2f(i / 1200.0f);
		}
	}
}

static inline float attenuation_to_amplitude(float p_atten) {
	if (p_atten <= 0.0f) {
		return 1.0f;
	} else if (p_atten >= ATTEN_TABLE_SIZE) {
		return 0.0f;
	} else {
		return attenuation_to_amp_table[(size_t)p_atten];
	}
}

static inline float amplitude_to_attenuation(float p_amp) {
	return -200.0f * log10f(p_amp);
}

static float key_to_hertz(float p_key) {
	if (p_key < 0.0f) {
		return 1.0f;
	}

	int offset = 300;
	int ratio = 1;
	for (int threshold = 900; threshold <= 14100; threshold += 1200) {
		if (p_key * 100.0f < threshold) {
			return ratio * cent_to_hertz_table[(int)(p_key * 100.0f) + offset];
		}
		offset -= 1200;
		ratio *= 2;
	}

	return 1.0f;
}

static inline float time_cent_to_second(float p_tc) {
	return exp2f(p_tc / 1200.0f);
}

static inline float absolute_cent_to_hertz(float p_ac) {
	return 8.176f * exp2f(p_ac / 1200.0f);
}

static inline float concave_curve(float p_x) {
	if (p_x <= 0.0f) {
		return 0.0f;
	} else if (p_x >= 1.0f) {
		return 1.0f;
	} else {
		return 2.0f * amplitude_to_attenuation(1.0f - p_x) / 960.0f;
	}
}

static inline float convex_curve(float p_x) {
	if (p_x <= 0.0f) {
		return 0.0f;
	} else if (p_x >= 1.0f) {
		return 1.0f;
	} else {
		return 1.0f - 2.0f * amplitude_to_attenuation(p_x) / 960.0f;
	}
}

class FileAndMemReader {
public:
	FileAndMemReader() :
			fp(NULL),
			mp(NULL),
			mp_size(0),
			mp_tell(0) {}

	~FileAndMemReader() {
		close();
	}

	void seeku(uint64_t p_pos, int p_rel_to) {
		this->seek((long)p_pos, p_rel_to);
	}

	inline bool is_valid() const {
		return (fp) || (mp);
	}

	size_t tell() const {
		if (!this->is_valid()) {
			return 0;
		}
		if (fp) { //If a file
			return (size_t)ftell(fp);
		} else { //If a memory block
			return mp_tell;
		}
	}

	bool eof() const {
		if (!this->is_valid()) {
			return true;
		}
		if (fp) {
			return (feof(fp) != 0);
		} else {
			return mp_tell >= mp_size;
		}
	}

	int get_character() {
		if (!this->is_valid()) {
			return -1;
		}
		if (fp) //If a file
		{
			return getc(fp);
		} else //If a memory block
		{
			if (mp_tell >= mp_size) {
				return -1;
			}
			const uint8_t *block = (const uint8_t *)mp;
			int x = block[mp_tell];
			mp_tell++;
			return x;
		}
	}

	void close(bool p_free_memory = false) {
		if (fp) {
			fclose(fp);
		}
		fp = NULL;
		if (mp && p_free_memory) {
			void *free_me = (void *)mp;
			free(free_me);
		}
		mp = NULL;
		mp_size = 0;
		mp_tell = 0;
	}

	size_t file_size() {
		if (!this->is_valid()) {
			return 0;
		}
		if (!fp) {
			return mp_size; //Size of memory block is well known
		}
		size_t old_pos = this->tell();
		seek(0l, SEEK_END);
		size_t file_size = this->tell();
		seek((long)old_pos, SEEK_SET);
		return file_size;
	}

	size_t read(void *p_buf, size_t p_num, size_t p_size) {
		if (!this->is_valid()) {
			return 0;
		}
		if (fp) {
			return fread(p_buf, p_num, p_size, fp);
		} else {
			size_t pos = 0;
			size_t maxSize = (size_t)p_size * p_num;

			while ((pos < maxSize) && (mp_tell < mp_size)) {
				uint8_t *dest = (uint8_t *)p_buf;
				const uint8_t *src = (const uint8_t *)mp;
				dest[pos] = src[mp_tell];
				mp_tell++;
				pos++;
			}

			return pos / p_num;
		}
	}

	void seek(long p_pos, int p_rel_to) {
		if (!this->is_valid()) {
			return;
		}

		if (fp) //If a file
		{
			fseek(fp, p_pos, p_rel_to);
		} else //If a memory block
		{
			switch (p_rel_to) {
				default:
				case SEEK_SET:
					mp_tell = (size_t)p_pos;
					break;

				case SEEK_END:
					mp_tell = mp_size - (size_t)p_pos;
					break;

				case SEEK_CUR:
					mp_tell = mp_tell + (size_t)p_pos;
					break;
			}

			if (mp_tell > mp_size) {
				mp_tell = mp_size;
			}
		}
	}

	void open_file(const char *p_path) {
		if (fp) {
			this->close(); //Close previously opened file first!
		}
#if !defined(_WIN32)
		fp = fopen(p_path, "rb");
#else
		wchar_t widePath[MAX_PATH];
		int size = MultiByteToWideChar(CP_UTF8, 0, p_path, (int)strlen(p_path), widePath, MAX_PATH);
		widePath[size] = '\0';
		fp = _wfopen(widePath, L"rb");
#endif
		mp = NULL;
		mp_size = 0;
		mp_tell = 0;
	}

	void open_data(const void *p_mem, size_t p_length) {
		if (fp) {
			this->close(); //Close previously opened file first!
		}
		fp = NULL;
		mp = p_mem;
		mp_size = p_length;
		mp_tell = 0;
	}

	inline const void *get_data() const {
		return mp;
	}

private:
	FILE *fp;
	const void *mp;
	size_t mp_size;
	size_t mp_tell;
};

static void mus_event_convert() {
	uint8_t data, last, channel;
	uint8_t event[3];
	int count;

	data = *mus_to_midi_pos++;
	last = data & 0x80;
	channel = data & 0xf;

	switch (data & 0x70) {
		case 0x00:
			event[0] = 0x80;
			event[1] = *mus_to_midi_pos++ & 0x7f;
			event[2] = mus_to_midi_channels[channel];
			count = 3;
			break;

		case 0x10:
			event[0] = 0x90;
			data = *mus_to_midi_pos++;
			event[1] = data & 0x7f;
			event[2] = data & 0x80 ? *mus_to_midi_pos++ : mus_to_midi_channels[channel];
			mus_to_midi_channels[channel] = event[2];
			count = 3;
			break;

		case 0x20:
			event[0] = 0xe0;
			event[1] = (*mus_to_midi_pos & 0x01) << 6;
			event[2] = *mus_to_midi_pos++ >> 1;
			count = 3;
			break;

		case 0x30:
			event[0] = 0xb0;
			event[1] = MUS_CONTROLLER_MAP[*mus_to_midi_pos++ & 0xf];
			event[2] = 0x7f;
			count = 3;
			break;

		case 0x40:
			data = *mus_to_midi_pos++;
			if (data == 0) {
				event[0] = 0xc0;
				event[1] = *mus_to_midi_pos++;
				count = 2;
				break;
			}
			event[0] = 0xb0;
			event[1] = MUS_CONTROLLER_MAP[data & 0xf];
			event[2] = *mus_to_midi_pos++;
			count = 3;
			break;

		case 0x50:
			return;

		case 0x60:
			event[0] = 0xff;
			event[1] = 0x2f;
			event[2] = 0x00;
			count = 3;

			// this prevents mus_to_midi_delta_bytes being read past the end of the MUS data
			last = 0;

			mus_to_midi_eot = 1;
			break;

		case 0x70:
			mus_to_midi_pos++;
			return;
	}

	if (channel == 9) {
		channel = 15;
	} else if (channel == 15) {
		channel = 9;
	}

	event[0] |= channel;

	mus_to_midi_data = (uint8_t *)realloc(mus_to_midi_data, mus_to_midi_size + mus_to_midi_delta_count + count);

	memcpy(mus_to_midi_data + mus_to_midi_size, &mus_to_midi_delta_bytes, mus_to_midi_delta_count);
	mus_to_midi_size += mus_to_midi_delta_count;
	memcpy(mus_to_midi_data + mus_to_midi_size, &event, count);
	mus_to_midi_size += count;

	if (last) {
		mus_to_midi_delta_count = 0;
		do {
			data = *mus_to_midi_pos++;
			mus_to_midi_delta_bytes[mus_to_midi_delta_count] = data;
			mus_to_midi_delta_count++;
		} while (data & 128);
	} else {
		mus_to_midi_delta_bytes[0] = 0;
		mus_to_midi_delta_count = 1;
	}
}

static FileAndMemReader *mus_to_midi(FileAndMemReader *p_fmr) {
	MUSHeader *mus_header = (MUSHeader *)p_fmr->get_data();
	MIDHeader mid_header;
	int mid_track_length_offset;
	int converted_track_length;
	int i;

	if (strncmp(mus_header->id, MUS_MAGIC, 4) != 0) {
		return nullptr;
	}

	if (p_fmr->file_size() != mus_header->score_start + mus_header->score_length) {
		return nullptr;
	}

	mus_to_midi_size = sizeof(MIDHeader);
	memcpy(mid_header.id, MID_MAGIC, 4);
	mid_header.length = byte_swap_32(6);
	mid_header.type = byte_swap_16(0);
	mid_header.ntracks = byte_swap_16(1);
	mid_header.ticks = byte_swap_16(70);
	mus_to_midi_data = (uint8_t *)malloc(mus_to_midi_size);
	memcpy(mus_to_midi_data, &mid_header, mus_to_midi_size);

	mus_to_midi_data = (uint8_t *)realloc(mus_to_midi_data, mus_to_midi_size + 8);
	memcpy(mus_to_midi_data + mus_to_midi_size, TRACK_MAGIC, 4);
	mus_to_midi_size += 4;
	mid_track_length_offset = mus_to_midi_size;
	mus_to_midi_size += 4;

	converted_track_length = 0;

	mus_to_midi_pos = (uint8_t *)p_fmr->get_data() + mus_header->score_start;
	mus_to_midi_eot = 0;
	mus_to_midi_delta_bytes[0] = 0;
	mus_to_midi_delta_count = 1;

	for (i = 0; i < 16; i++) {
		mus_to_midi_channels[i] = 0;
	}

	while (!mus_to_midi_eot) {
		mus_event_convert();
	}
	converted_track_length = byte_swap_32(mus_to_midi_size - sizeof(MIDHeader) - 8);
	memcpy(mus_to_midi_data + mid_track_length_offset, &converted_track_length, 4);

	FileAndMemReader *converted = new FileAndMemReader;
	converted->open_data(mus_to_midi_data, mus_to_midi_size);

	return converted;
}

struct SF2Modulator {
	union {
		GeneralController general;
		uint8_t midi;
	} index;
	ControllerPalette palette;
	SourceDirection direction;
	SourcePolarity polarity;
	SourceType type;
};

static bool operator==(const SF2Modulator &p_a, const SF2Modulator &p_b) {
	return p_a.index.midi == p_b.index.midi && p_a.palette == p_b.palette && p_a.direction == p_b.direction &&
			p_a.polarity == p_b.polarity && p_a.type == p_b.type;
}
struct RangesType {
	int8_t lo;
	int8_t hi;
};

union GenAmountType {
	RangesType ranges;
	int16_t sh_amount;
	uint16_t w_amount;
};

struct VersionTag {
	uint16_t major;
	uint16_t minor;
};

#pragma pack(push, 1)
struct PresetHeader {
	char preset_name[20];
	uint16_t preset;
	uint16_t bank;
	uint16_t preset_bag_index;
	uint32_t library;
	uint32_t genre;
	uint32_t morphology;
};

struct Bag {
	uint16_t gen_index;
	uint16_t mod_index;
};

struct ModList {
	SF2Modulator mod_src_oper;
	SF2Generator mod_dest_oper;
	int16_t mod_amount;
	SF2Modulator mod_amount_src_oper;
	Transform mod_trans_oper;
};
struct GenList {
	SF2Generator gen_oper;
	GenAmountType gen_amount;
};

struct Inst {
	char inst_name[20];
	uint16_t inst_bag_index;
};

struct SF2Sample {
	char sample_name[20];
	uint32_t start;
	uint32_t end;
	uint32_t start_loop;
	uint32_t end_loop;
	uint32_t sample_rate;
	int8_t original_key;
	int8_t correction;
	uint16_t sample_link;
	uint16_t sample_type;
};
#pragma pack(pop)
class Envelope {
public:
	enum class Phase {
		DELAY,
		ATTACK,
		HOLD,
		DECAY,
		SUSTAIN,
		RELEASE,
		FINISHED
	};

	Envelope() :
			effective_output_rate(0.0f), params(), phase(Phase::DELAY), phase_steps(0), value(1.0f) {
	}

	Envelope(float p_output_rate, unsigned int p_interval) :
			effective_output_rate(p_output_rate / p_interval), params(), phase(Phase::DELAY), phase_steps(0), value(1.0f) {
	}

	inline Phase get_phase() const {
		return phase;
	}

	inline float get_value() const {
		return value;
	}

	inline void set_parameter(Phase p_phase, float p_param) {
		if (p_phase == Phase::SUSTAIN) {
			params[(size_t)Phase::SUSTAIN] = 1.0f - 0.001f * p_param;
		} else if (phase < Phase::FINISHED) {
			params[(size_t)p_phase] = effective_output_rate * time_cent_to_second(p_param);
		}
	}

	inline void release() {
		if (phase < Phase::RELEASE) {
			change_phase(Phase::RELEASE);
		}
	}

	void update() {
		if (phase == Phase::FINISHED) {
			return;
		}

		++phase_steps;

		size_t i = (size_t)phase;
		while (phase < Phase::FINISHED && phase != Phase::SUSTAIN && phase_steps >= params[i]) {
			change_phase((Phase)++i);
		}

		const float &sustain = params[(size_t)Phase::SUSTAIN];
		switch (phase) {
			case Phase::DELAY:
			case Phase::FINISHED:
				value = 0.0f;
				return;
			case Phase::ATTACK:
				value = phase_steps / params[i];
				return;
			case Phase::HOLD:
				value = 1.0f;
				return;
			case Phase::DECAY:
				value = 1.0f - phase_steps / params[i];
				if (value <= sustain) {
					value = sustain;
					change_phase(Phase::SUSTAIN);
				}
				return;
			case Phase::SUSTAIN:
				value = sustain;
				return;
			case Phase::RELEASE:
				value -= 1.0f / params[i];
				if (value <= 0.0f) {
					value = 0.0f;
					change_phase(Phase::FINISHED);
				}
				return;
		}
	}

private:
	float effective_output_rate;
	float params[(size_t)Phase::FINISHED];
	Phase phase;
	unsigned int phase_steps;
	float value;

	inline void change_phase(Phase p_phase) {
		phase = p_phase;
		phase_steps = 0;
	}
};

class LFO {
public:
	LFO() :
			steps(0), delay(0), delta(0.0f), value(0.0f), up(true) {
	}

	LFO(float p_output_rate, unsigned int p_interval) :
			output_rate(p_output_rate), interval(p_interval), steps(0), delay(0), delta(0.0f), value(0.0f), up(true) {
	}

	inline float get_value() const {
		return value;
	}

	inline void set_delay(float p_delay) {
		delay = (unsigned int)(output_rate * time_cent_to_second(p_delay));
	}

	inline void set_frequency(float p_freq) {
		delta = 4.0f * interval * absolute_cent_to_hertz(p_freq) / output_rate;
	}

	inline void update() {
		if (steps <= delay) {
			++steps;
			return;
		}
		if (up) {
			value += delta;
			if (value > 1.0f) {
				value = 2.0f - value;
				up = false;
			}
		} else {
			value -= delta;
			if (value < -1.0f) {
				value = -2.0f - value;
				up = true;
			}
		}
	}

private:
	float output_rate;
	unsigned int interval;
	unsigned int steps, delay;
	float delta, value;
	bool up;
};

class Modulator {
public:
	Modulator() {
	}

	Modulator(const ModList &p_param) :
			param(&p_param), source(0.0f), amount_source(1.0f), value(0.0f) {
	}

	inline SF2Generator get_destination() const {
		return param->mod_dest_oper;
	}

	inline int16_t get_amount() const {
		return param->mod_amount;
	}

	bool can_be_negative() const {
		if (param->mod_trans_oper == Transform::ABSOLUTE_VALUE || param->mod_amount == 0) {
			return false;
		}

		if (param->mod_amount > 0) {
			const bool no_src = param->mod_src_oper.palette == ControllerPalette::GENERAL &&
					param->mod_src_oper.index.general == GeneralController::NO_CONTROLLER;
			const bool uni_src = param->mod_src_oper.polarity == SourcePolarity::UNIPOLAR;
			const bool no_amt = param->mod_amount_src_oper.palette == ControllerPalette::GENERAL &&
					param->mod_amount_src_oper.index.general == GeneralController::NO_CONTROLLER;
			const bool uni_amt = param->mod_amount_src_oper.polarity == SourcePolarity::UNIPOLAR;

			if ((uni_src && uni_amt) || (uni_src && no_amt) || (no_src && uni_amt) || (no_src && no_amt)) {
				return false;
			}
		}

		return true;
	}

	inline float get_value() const {
		return value;
	}

	bool update_sf2_controller(GeneralController p_controller, float p_value) {
		bool updated = false;
		if (param->mod_src_oper.palette == ControllerPalette::GENERAL && p_controller == param->mod_src_oper.index.general) {
			source = map(p_value, param->mod_src_oper);
			updated = true;
		}
		if (param->mod_amount_src_oper.palette == ControllerPalette::GENERAL &&
				p_controller == param->mod_amount_src_oper.index.general) {
			amount_source = map(p_value, param->mod_amount_src_oper);
			updated = true;
		}

		if (updated) {
			calculate_value();
		}
		return updated;
	}

	bool update_midi_controller(uint8_t p_controller, uint8_t p_value) {
		bool updated = false;
		if (param->mod_src_oper.palette == ControllerPalette::MIDI && p_controller == param->mod_src_oper.index.midi) {
			source = map(p_value, param->mod_src_oper);
			updated = true;
		}
		if (param->mod_amount_src_oper.palette == ControllerPalette::MIDI && p_controller == param->mod_amount_src_oper.index.midi) {
			amount_source = map(p_value, param->mod_amount_src_oper);
			updated = true;
		}

		if (updated) {
			calculate_value();
		}
		return updated;
	}

private:
	const ModList *param;
	float source, amount_source, value;

	inline void calculate_value() {
		if (param->mod_trans_oper == Transform::ABSOLUTE_VALUE) {
			value = fabs(param->mod_amount * source * amount_source);
		} else {
			value = param->mod_amount * source * amount_source;
		}
	}

	static float map(float p_value, const SF2Modulator &p_mod) {
		if (p_mod.palette == ControllerPalette::GENERAL && p_mod.index.general == GeneralController::PITCH_WHEEL) {
			p_value /= 1 << 14;
		} else {
			p_value /= 1 << 7;
		}

		if (p_mod.type == SourceType::SWITCH) {
			const float off = p_mod.polarity == SourcePolarity::UNIPOLAR ? 0.0f : -1.0f;
			const float x = p_mod.direction == SourceDirection::POSITIVE ? p_value : 1.0f - p_value;
			return x >= 0.5f ? 1.0f : off;
		} else if (p_mod.polarity == SourcePolarity::UNIPOLAR) {
			const float x = p_mod.direction == SourceDirection::POSITIVE ? p_value : 1.0f - p_value;
			switch (p_mod.type) {
				case SourceType::LINEAR:
					return x;
				case SourceType::CONCAVE:
					return concave_curve(x);
				case SourceType::CONVEX:
					return convex_curve(x);
				default:
					break;
			}
		} else {
			const int dir = p_mod.direction == SourceDirection::POSITIVE ? 1 : -1;
			const int sign = p_value > 0.5f ? 1 : -1;
			const float x = 2.0f * p_value - 1.0f;
			switch (p_mod.type) {
				case SourceType::LINEAR:
					return dir * x;
				case SourceType::CONCAVE:
					return sign * dir * concave_curve(sign * x);
				case SourceType::CONVEX:
					return sign * dir * convex_curve(sign * x);
				default:
					break;
			}
		}
		return 0.0f;
	}
};

struct Sample {
	uint32_t start, end, start_loop, end_loop, sample_rate;
	int8_t key, correction;
	float min_atten;
	const std::vector<int16_t> *buffer;

	Sample() {
	}

	Sample(const SF2Sample &p_sample, const std::vector<int16_t> &p_sample_buffer, Synthesizer *p_synth) :
			start(p_sample.start), end(p_sample.end), start_loop(p_sample.start_loop), end_loop(p_sample.end_loop), sample_rate(p_sample.sample_rate), key(p_sample.original_key), correction(p_sample.correction), buffer(&p_sample_buffer) {
		if (start >= p_sample_buffer.size() || end >= p_sample_buffer.size()) {
			printf("Generator extends sample range beyond end\n");
			p_synth->set_load_error(true);
			return;
		}
		if (start < end) {
			int sample_max = 0;
			for (size_t i = start; i < end; ++i) {
				sample_max = std::max(sample_max, abs(p_sample_buffer[i]));
			}
			min_atten = amplitude_to_attenuation((float)sample_max / INT16_MAX);
		} else { // "Disable" the sample; this is consistent with Fluidsynth/TinySoundFont
			start = end = start_loop = end_loop = 0;
		}
	}
};

class GeneratorSet {
public:
	GeneratorSet() {
		for (size_t i = 0; i < NUM_GENERATORS; ++i) {
			generators[i] = { false, DEFAULT_GENERATOR_VALUES[i] };
		}
	}
	inline int16_t get_or_default(SF2Generator p_type) const {
		return generators[(size_t)p_type].amount;
	}
	inline void set(SF2Generator p_type, int16_t p_amount) {
		generators[(size_t)p_type] = { true, p_amount };
	}
	void merge(const GeneratorSet &p_b) {
		for (size_t i = 0; i < NUM_GENERATORS; ++i) {
			if (!generators[i].used && p_b.generators[i].used) {
				generators[i] = p_b.generators[i];
			}
		}
	}
	void add(const GeneratorSet &p_b) {
		for (size_t i = 0; i < NUM_GENERATORS; ++i) {
			if (p_b.generators[i].used) {
				generators[i].amount += p_b.generators[i].amount;
				generators[i].used = true;
			}
		}
	}

private:
	struct Generator {
		bool used;
		int16_t amount;
	};
	Generator generators[NUM_GENERATORS];
};
class ModulatorParameterSet {
public:
	static const ModulatorParameterSet &get_default_parameters() {
		static ModulatorParameterSet def_params;
		static bool initialized = false;
		if (!initialized) {
			initialized = true;

			// See "SoundFont Technical Specification" Version 2.04
			// p.41 "8.4 Default Modulators"
			{
				// 8.4.1 MIDI Note-On Velocity to Initial Attenuation
				ModList param;
				param.mod_src_oper.index.general = GeneralController::NOTE_ON_VELOCITY;
				param.mod_src_oper.palette = ControllerPalette::GENERAL;
				param.mod_src_oper.direction = SourceDirection::NEGATIVE;
				param.mod_src_oper.polarity = SourcePolarity::UNIPOLAR;
				param.mod_src_oper.type = SourceType::CONCAVE;
				param.mod_dest_oper = SF2Generator::INITIAL_ATTENUATION;
				param.mod_amount = 960;
				param.mod_amount_src_oper.index.general = GeneralController::NO_CONTROLLER;
				param.mod_amount_src_oper.palette = ControllerPalette::GENERAL;
				param.mod_trans_oper = Transform::LINEAR;
				def_params.append(param);
			}
			{
				// 8.4.2 MIDI Note-On Velocity to Filter Cutoff
				ModList param;
				param.mod_src_oper.index.general = GeneralController::NOTE_ON_VELOCITY;
				param.mod_src_oper.palette = ControllerPalette::GENERAL;
				param.mod_src_oper.direction = SourceDirection::NEGATIVE;
				param.mod_src_oper.polarity = SourcePolarity::UNIPOLAR;
				param.mod_src_oper.type = SourceType::LINEAR;
				param.mod_dest_oper = SF2Generator::INITIAL_FILTER_FC;
				param.mod_amount = -2400;
				param.mod_amount_src_oper.index.general = GeneralController::NO_CONTROLLER;
				param.mod_amount_src_oper.palette = ControllerPalette::GENERAL;
				param.mod_trans_oper = Transform::LINEAR;
				def_params.append(param);
			}
			{
				// 8.4.3 MIDI Channel Pressure to Vibrato LFO Pitch Depth
				ModList param;
				param.mod_src_oper.index.midi = 13;
				param.mod_src_oper.palette = ControllerPalette::MIDI;
				param.mod_src_oper.direction = SourceDirection::POSITIVE;
				param.mod_src_oper.polarity = SourcePolarity::UNIPOLAR;
				param.mod_src_oper.type = SourceType::LINEAR;
				param.mod_dest_oper = SF2Generator::VIB_LFO_TO_PITCH;
				param.mod_amount = 50;
				param.mod_amount_src_oper.index.general = GeneralController::NO_CONTROLLER;
				param.mod_amount_src_oper.palette = ControllerPalette::GENERAL;
				param.mod_trans_oper = Transform::LINEAR;
				def_params.append(param);
			}
			{
				// 8.4.4 MIDI Continuous Controller 1 to Vibrato LFO Pitch Depth
				ModList param;
				param.mod_src_oper.index.midi = 1;
				param.mod_src_oper.palette = ControllerPalette::MIDI;
				param.mod_src_oper.direction = SourceDirection::POSITIVE;
				param.mod_src_oper.polarity = SourcePolarity::UNIPOLAR;
				param.mod_src_oper.type = SourceType::LINEAR;
				param.mod_dest_oper = SF2Generator::VIB_LFO_TO_PITCH;
				param.mod_amount = 50;
				param.mod_amount_src_oper.index.general = GeneralController::NO_CONTROLLER;
				param.mod_amount_src_oper.palette = ControllerPalette::GENERAL;
				param.mod_trans_oper = Transform::LINEAR;
				def_params.append(param);
			}
			{
				// 8.4.5 MIDI Continuous Controller 7 to Initial Attenuation Source
				ModList param;
				param.mod_src_oper.index.midi = 7;
				param.mod_src_oper.palette = ControllerPalette::MIDI;
				param.mod_src_oper.direction = SourceDirection::NEGATIVE;
				param.mod_src_oper.polarity = SourcePolarity::UNIPOLAR;
				param.mod_src_oper.type = SourceType::CONCAVE;
				param.mod_dest_oper = SF2Generator::INITIAL_ATTENUATION;
				param.mod_amount = 960;
				param.mod_amount_src_oper.index.general = GeneralController::NO_CONTROLLER;
				param.mod_amount_src_oper.palette = ControllerPalette::GENERAL;
				param.mod_trans_oper = Transform::LINEAR;
				def_params.append(param);
			}
			{
				// 8.4.6 MIDI Continuous Controller 10 to Pan Position
				ModList param;
				param.mod_src_oper.index.midi = 10;
				param.mod_src_oper.palette = ControllerPalette::MIDI;
				param.mod_src_oper.direction = SourceDirection::POSITIVE;
				param.mod_src_oper.polarity = SourcePolarity::BIPOLAR;
				param.mod_src_oper.type = SourceType::LINEAR;
				param.mod_dest_oper = SF2Generator::PAN;
				param.mod_amount = 500;
				param.mod_amount_src_oper.index.general = GeneralController::NO_CONTROLLER;
				param.mod_amount_src_oper.palette = ControllerPalette::GENERAL;
				param.mod_trans_oper = Transform::LINEAR;
				def_params.append(param);
			}
			{
				// 8.4.7 MIDI Continuous Controller 11 to Initial Attenuation
				ModList param;
				param.mod_src_oper.index.midi = 11;
				param.mod_src_oper.palette = ControllerPalette::MIDI;
				param.mod_src_oper.direction = SourceDirection::NEGATIVE;
				param.mod_src_oper.polarity = SourcePolarity::UNIPOLAR;
				param.mod_src_oper.type = SourceType::CONCAVE;
				param.mod_dest_oper = SF2Generator::INITIAL_ATTENUATION;
				param.mod_amount = 960;
				param.mod_amount_src_oper.index.general = GeneralController::NO_CONTROLLER;
				param.mod_amount_src_oper.palette = ControllerPalette::GENERAL;
				param.mod_trans_oper = Transform::LINEAR;
				def_params.append(param);
			}
			{
				// 8.4.8 MIDI Continuous Controller 91 to Reverb Effects Send
				ModList param;
				param.mod_src_oper.index.midi = 91;
				param.mod_src_oper.palette = ControllerPalette::MIDI;
				param.mod_src_oper.direction = SourceDirection::POSITIVE;
				param.mod_src_oper.polarity = SourcePolarity::UNIPOLAR;
				param.mod_src_oper.type = SourceType::LINEAR;
				param.mod_dest_oper = SF2Generator::REVERB_EFFECTS_SEND;
				param.mod_amount = 200;
				param.mod_amount_src_oper.index.general = GeneralController::NO_CONTROLLER;
				param.mod_amount_src_oper.palette = ControllerPalette::GENERAL;
				param.mod_trans_oper = Transform::LINEAR;
				def_params.append(param);
			}
			{
				// 8.4.9 MIDI Continuous Controller 93 to Chorus Effects Send
				ModList param;
				param.mod_src_oper.index.midi = 93;
				param.mod_src_oper.palette = ControllerPalette::MIDI;
				param.mod_src_oper.direction = SourceDirection::POSITIVE;
				param.mod_src_oper.polarity = SourcePolarity::UNIPOLAR;
				param.mod_src_oper.type = SourceType::LINEAR;
				param.mod_dest_oper = SF2Generator::CHORUS_EFFECTS_SEND;
				param.mod_amount = 200;
				param.mod_amount_src_oper.index.general = GeneralController::NO_CONTROLLER;
				param.mod_amount_src_oper.palette = ControllerPalette::GENERAL;
				param.mod_trans_oper = Transform::LINEAR;
				def_params.append(param);
			}
			{
				// 8.4.10 MIDI Pitch Wheel to Initial Pitch Controlled by MIDI Pitch Wheel Sensitivity
				ModList param;
				param.mod_src_oper.index.general = GeneralController::PITCH_WHEEL;
				param.mod_src_oper.palette = ControllerPalette::GENERAL;
				param.mod_src_oper.direction = SourceDirection::POSITIVE;
				param.mod_src_oper.polarity = SourcePolarity::BIPOLAR;
				param.mod_src_oper.type = SourceType::LINEAR;
				param.mod_dest_oper = SF2Generator::PITCH;
				param.mod_amount = 12700;
				param.mod_amount_src_oper.index.general = GeneralController::PITCH_WHEEL_SENSITIVITY;
				param.mod_amount_src_oper.palette = ControllerPalette::GENERAL;
				param.mod_amount_src_oper.direction = SourceDirection::POSITIVE;
				param.mod_amount_src_oper.polarity = SourcePolarity::UNIPOLAR;
				param.mod_amount_src_oper.type = SourceType::LINEAR;
				param.mod_trans_oper = Transform::LINEAR;
				def_params.append(param);
			}
		}
		return def_params;
	}

	inline const std::vector<ModList> &get_parameters() const {
		return params;
	}

	void append(const ModList &p_param) {
		for (const ModList &p : params) {
			if (modulators_are_identical(p, p_param)) {
				return;
			}
		}
		params.push_back(p_param);
	}

	void add_or_append(const ModList &p_param) {
		for (ModList &p : params) {
			if (modulators_are_identical(p, p_param)) {
				p.mod_amount += p_param.mod_amount;
				return;
			}
		}
		params.push_back(p_param);
	}

	void merge(const ModulatorParameterSet &p_b) {
		for (const ModList &param : p_b.params) {
			append(param);
		}
	}

	void merge_and_add(const ModulatorParameterSet &p_b) {
		for (const ModList &param : p_b.params) {
			add_or_append(param);
		}
	}

private:
	inline bool modulators_are_identical(const ModList &p_a, const ModList &p_b) {
		return p_a.mod_src_oper == p_b.mod_src_oper && p_a.mod_dest_oper == p_b.mod_dest_oper && p_a.mod_amount_src_oper == p_b.mod_amount_src_oper &&
				p_a.mod_trans_oper == p_b.mod_trans_oper;
	}

	std::vector<ModList> params;
};

struct Zone {
	struct Range {
		Range() :
				min(0), max(127) {}

		Range(int8_t p_min, int8_t p_max) {
			min = p_min;
			max = p_max;
		}

		int8_t min, max;

		inline bool contains(int8_t p_value) const {
			return min <= p_value && p_value <= max;
		}
	};

	Range key_range, velocity_range;
	GeneratorSet generators;
	ModulatorParameterSet modulator_parameters;

	inline bool is_in_range(int8_t p_key, int8_t p_velocity) const {
		return key_range.contains(p_key) && velocity_range.contains(p_velocity);
	}
};

static void read_bags(std::vector<Zone> &p_zones, std::vector<Bag>::const_iterator p_bag_begin,
		std::vector<Bag>::const_iterator p_bag_end, const std::vector<ModList> &p_mods,
		const std::vector<GenList> &p_gens, SF2Generator p_index_gen, Synthesizer *p_synth) {
	if (&(*p_bag_begin) > &(*p_bag_end)) {
		printf("bag indices not monotonically increasing");
		p_synth->set_load_error(true);
		return;
	}

	Zone global_zone;

	for (std::vector<Bag>::const_iterator it_bag = p_bag_begin; it_bag != p_bag_end; ++it_bag) {
		Zone zone;

		std::vector<Bag>::const_iterator next_bag = it_bag;
		++next_bag;

		std::vector<ModList>::const_iterator begin_mod = p_mods.begin();
		for (uint16_t i = 0; i < it_bag->mod_index; ++i) {
			++begin_mod;
		}
		std::vector<ModList>::const_iterator end_mod = p_mods.begin();
		for (uint16_t i = 0; i < next_bag->mod_index; ++i) {
			++end_mod;
		}
		if (&(*begin_mod) > &(*end_mod)) {
			printf("modulator indices not monotonically increasing");
			p_synth->set_load_error(true);
			return;
		}
		for (std::vector<ModList>::const_iterator it_mod = begin_mod; it_mod != end_mod; ++it_mod) {
			zone.modulator_parameters.append(*it_mod);
		}

		std::vector<GenList>::const_iterator begin_gen = p_gens.begin();
		for (uint16_t i = 0; i < it_bag->gen_index; ++i) {
			++begin_gen;
		}
		std::vector<GenList>::const_iterator end_gen = p_gens.begin();
		for (uint16_t i = 0; i < next_bag->gen_index; ++i) {
			++end_gen;
		}
		if (&(*begin_gen) > &(*end_gen)) {
			printf("generator indices not monotonically increasing");
			p_synth->set_load_error(true);
			return;
		}
		for (std::vector<GenList>::const_iterator it_gen = begin_gen; it_gen != end_gen; ++it_gen) {
			const RangesType &range = it_gen->gen_amount.ranges;
			switch (it_gen->gen_oper) {
				case SF2Generator::KEY_RANGE:
					zone.key_range = { range.lo, range.hi };
					break;
				case SF2Generator::VELOCITY_RANGE:
					zone.velocity_range = { range.lo, range.hi };
					break;
				default:
					if (it_gen->gen_oper < SF2Generator::END_OPERATOR) {
						zone.generators.set(it_gen->gen_oper, it_gen->gen_amount.sh_amount);
					}
					break;
			}
		}

		std::vector<GenList>::const_iterator prev_gen = end_gen;
		--prev_gen;

		if (begin_gen != end_gen && prev_gen->gen_oper == p_index_gen) {
			p_zones.push_back(zone);
		} else if (it_bag == p_bag_begin && (begin_gen != end_gen || begin_mod != end_mod)) {
			global_zone = zone;
		}
	}

	for (Zone &zone : p_zones) {
		zone.generators.merge(global_zone.generators);
		zone.modulator_parameters.merge(global_zone.modulator_parameters);
	}
}
struct Instrument {
	std::vector<Zone> zones;

	Instrument() {
	}
	Instrument(std::vector<Inst>::iterator p_inst_iter, const std::vector<Bag> &p_ibag,
			const std::vector<ModList> &p_imod, const std::vector<GenList> &p_igen,
			Synthesizer *p_synth) {
		std::vector<Bag>::const_iterator bag_begin = p_ibag.begin();
		for (uint16_t i = 0; i < p_inst_iter->inst_bag_index; ++i) {
			++bag_begin;
		}
		std::vector<Inst>::iterator next_inst = p_inst_iter;
		++next_inst;
		std::vector<Bag>::const_iterator bag_end = p_ibag.begin();
		for (uint16_t i = 0; i < next_inst->inst_bag_index; ++i) {
			++bag_end;
		}
		read_bags(zones, bag_begin, bag_end, p_imod, p_igen, SF2Generator::SAMPLE_ID, p_synth);
	}
};

struct Synthesizer::Preset {
	uint16_t bank, preset_id;
	std::vector<Zone> zones;
	const SoundFont *soundfont;

	Preset() {
	}
	Preset(std::vector<PresetHeader>::iterator p_phdr_iter, const std::vector<Bag> &p_pbag,
			const std::vector<ModList> &p_pmod, const std::vector<GenList> &p_pgen,
			const SoundFont *p_sfont, Synthesizer *p_synth) :
			bank(p_phdr_iter->bank), preset_id(p_phdr_iter->preset), soundfont(p_sfont) {
		std::vector<Bag>::const_iterator bag_begin = p_pbag.begin();
		for (uint16_t i = 0; i < p_phdr_iter->preset_bag_index; ++i) {
			++bag_begin;
		}
		std::vector<PresetHeader>::iterator next_preset = p_phdr_iter;
		++next_preset;
		std::vector<Bag>::const_iterator bag_end = p_pbag.begin();
		for (uint16_t i = 0; i < next_preset->preset_bag_index; ++i) {
			++bag_end;
		}
		read_bags(zones, bag_begin, bag_end, p_pmod, p_pgen, SF2Generator::INSTRUMENT, p_synth);
	}
};

struct RIFFHeader {
	uint32_t id;
	uint32_t size;
};

static RIFFHeader read_header(FileAndMemReader *p_file) {
	RIFFHeader header;
	p_file->read(&header, 1, sizeof(header));
	return header;
}

static uint32_t read_four_cc(FileAndMemReader *p_file) {
	uint32_t id;
	p_file->read(&id, 1, sizeof(id));
	return id;
}

static const uint32_t to_four_cc(const char p_str[5]) {
	uint32_t four_cc = 0;
	for (int i = 0; i < 4; ++i) {
		four_cc |= p_str[i] << CHAR_BIT * i;
	}
	return four_cc;
}
class Synthesizer::SoundFont {
public:
	explicit SoundFont(FileAndMemReader *p_file, Synthesizer *p_synth) {
		if (!p_file) {
			p_synth->set_load_error(true);
			return;
		}

		const RIFFHeader riff_header = read_header(p_file);
		const uint32_t riff_type = read_four_cc(p_file);
		if (riff_header.id != FOUR_CC_RIFF || riff_type != FOUR_CC_SFBK) {
			printf("not a SoundFont file");
			p_synth->set_load_error(true);
			return;
		}

		for (size_t s = 0; s < riff_header.size - sizeof(riff_type);) {
			const RIFFHeader chunk_header = read_header(p_file);
			s += sizeof(chunk_header) + chunk_header.size;
			switch (chunk_header.id) {
				case FOUR_CC_LIST: {
					const uint32_t chunk_type = read_four_cc(p_file);
					const size_t chunk_size = chunk_header.size - sizeof(chunk_type);
					switch (chunk_type) {
						case FOUR_CC_INFO:
							read_info_chunk(p_file, chunk_size, p_synth);
							break;
						case FOUR_CC_SDTA:
							read_sdta_chunk(p_file, chunk_size, p_synth);
							break;
						case FOUR_CC_PDTA:
							read_pdta_chunk(p_file, chunk_size, p_synth);
							break;
						default:
							p_file->seek(chunk_size, SEEK_CUR);
							break;
					}
					break;
				}
				default:
					p_file->seek(chunk_header.size, SEEK_CUR);
					break;
			}
			if (p_synth->get_load_error()) {
				break;
			}
		}
	}

	~SoundFont() {
		for (const Preset *preset : presets) {
			delete preset;
		}
	}

	inline const std::vector<Sample> &get_samples() const {
		return samples;
	}

	inline const std::vector<Instrument> &get_instruments() const {
		return instruments;
	}

	inline const std::vector<const Synthesizer::Preset *> &get_preset_pointers() const {
		return presets;
	}

private:
	std::vector<int16_t> sample_buffer;
	std::vector<Sample> samples;
	std::vector<Instrument> instruments;
	std::vector<const Preset *> presets;

	void read_info_chunk(FileAndMemReader *p_file, size_t p_size, Synthesizer *p_synth) {
		for (size_t s = 0; s < p_size;) {
			const RIFFHeader subchunk_header = read_header(p_file);
			s += sizeof(subchunk_header) + subchunk_header.size;
			switch (subchunk_header.id) {
				case FOUR_CC_IFIL: {
					VersionTag ver;
					p_file->read((char *)&ver, 1, subchunk_header.size);
					if (ver.major > 2 || ver.minor > 4) {
						printf("SoundFont later than 2.04 not supported");
						p_synth->set_load_error(true);
						return;
					}
					break;
				}
				case FOUR_CC_INAM: // Skip the name, we don't do anything with it
				default:
					p_file->seek(subchunk_header.size, SEEK_CUR);
					break;
			}
		}
	}

	void read_sdta_chunk(FileAndMemReader *p_file, size_t p_size, Synthesizer *p_synth) {
		for (size_t s = 0; s < p_size;) {
			const RIFFHeader subchunk_header = read_header(p_file);
			s += sizeof(subchunk_header) + subchunk_header.size;
			switch (subchunk_header.id) {
				case FOUR_CC_SMPL:
					if (subchunk_header.size == 0) {
						printf("no sample data found");
						p_synth->set_load_error(true);
						return;
					}
					sample_buffer.resize(subchunk_header.size / sizeof(int16_t));
					p_file->read((char *)sample_buffer.data(), 1, subchunk_header.size);
					break;
				default:
					p_file->seek(subchunk_header.size, SEEK_CUR);
					break;
			}
		}
	}

	void read_modulator(FileAndMemReader *p_file, SF2Modulator &p_mod) {
		uint16_t data;
		p_file->read((char *)&data, 1, sizeof(uint16_t));

		p_mod.index.midi = data & 127;
		p_mod.palette = (ControllerPalette)((data >> 7) & 1);
		p_mod.direction = (SourceDirection)((data >> 8) & 1);
		p_mod.polarity = (SourcePolarity)((data >> 9) & 1);
		p_mod.type = (SourceType)((data >> 10) & 63);
	}

	void read_mod_list(FileAndMemReader *p_file, std::vector<ModList> &p_list, uint32_t p_total_size, Synthesizer *p_synth) {
		static const size_t STRUCT_SIZE = 10;
		if (p_total_size % STRUCT_SIZE != 0) {
			printf("invalid chunk size");
			p_synth->set_load_error(true);
			return;
		}
		p_list.reserve(p_total_size / STRUCT_SIZE);
		for (size_t i = 0; i < p_total_size / STRUCT_SIZE; ++i) {
			ModList mod;
			read_modulator(p_file, mod.mod_src_oper);
			p_file->read((char *)&mod.mod_dest_oper, 1, 2);
			p_file->read((char *)&mod.mod_amount, 1, 2);
			read_modulator(p_file, mod.mod_amount_src_oper);
			p_file->read((char *)&mod.mod_trans_oper, 1, 2);
			p_list.push_back(mod);
		}
	}

	template <typename T>
	void read_pdta_list(FileAndMemReader *p_file, std::vector<T> &p_list, uint32_t p_total_size, Synthesizer *p_synth) {
		if (p_total_size % sizeof(T) != 0) {
			printf("invalid chunk size");
			p_synth->set_load_error(true);
			return;
		}
		size_t num_members = p_total_size / sizeof(T);
		p_list.resize(num_members);
		for (size_t i = 0; i < num_members; ++i) {
			p_file->read((char *)&p_list[i], 1, sizeof(T));
		}
	}

	void read_pdta_chunk(FileAndMemReader *p_file, size_t p_size, Synthesizer *p_synth) {
		std::vector<PresetHeader> phdr;
		std::vector<Inst> inst;
		std::vector<Bag> pbag, ibag;
		std::vector<ModList> pmod, imod;
		std::vector<GenList> pgen, igen;
		std::vector<SF2Sample> shdr;

		for (size_t s = 0; s < p_size;) {
			const RIFFHeader subchunk_header = read_header(p_file);
			s += sizeof(subchunk_header) + subchunk_header.size;
			switch (subchunk_header.id) {
				case FOUR_CC_PHDR:
					read_pdta_list(p_file, phdr, subchunk_header.size, p_synth);
					break;
				case FOUR_CC_PBAG:
					read_pdta_list(p_file, pbag, subchunk_header.size, p_synth);
					break;
				case FOUR_CC_PMOD:
					read_mod_list(p_file, pmod, subchunk_header.size, p_synth);
					break;
				case FOUR_CC_PGEN:
					read_pdta_list(p_file, pgen, subchunk_header.size, p_synth);
					break;
				case FOUR_CC_INST:
					read_pdta_list(p_file, inst, subchunk_header.size, p_synth);
					break;
				case FOUR_CC_IBAG:
					read_pdta_list(p_file, ibag, subchunk_header.size, p_synth);
					break;
				case FOUR_CC_IMOD:
					read_mod_list(p_file, imod, subchunk_header.size, p_synth);
					break;
				case FOUR_CC_IGEN:
					read_pdta_list(p_file, igen, subchunk_header.size, p_synth);
					break;
				case FOUR_CC_SHDR:
					read_pdta_list(p_file, shdr, subchunk_header.size, p_synth);
					break;
				default:
					p_file->seek(subchunk_header.size, SEEK_CUR);
					break;
			}
			if (p_synth->get_load_error()) {
				return;
			}
		}

		// last records of inst, phdr, and shdr sub-chunks indicate end of records, and are ignored

		if (inst.size() < 2) {
			printf("no instrument found");
			p_synth->set_load_error(true);
			return;
		}
		instruments.reserve(inst.size() - 1);
		for (std::vector<Inst>::iterator it_inst = inst.begin(), it_end = --inst.end(); it_inst != it_end; ++it_inst) {
			instruments.push_back({ it_inst, ibag, imod, igen, p_synth });
			if (p_synth->get_load_error()) {
				return;
			}
		}

		if (phdr.size() < 2) {
			printf("no preset found");
			p_synth->set_load_error(true);
			return;
		}
		presets.reserve(phdr.size() - 1);
		for (std::vector<PresetHeader>::iterator it_phdr = phdr.begin(), it_end = --phdr.end(); it_phdr != it_end; ++it_phdr) {
			presets.push_back(new Preset(it_phdr, pbag, pmod, pgen, this, p_synth));
			if (p_synth->get_load_error()) {
				return;
			}
		}

		if (shdr.size() < 2) {
			printf("no sample found");
			p_synth->set_load_error(true);
			return;
		}
		samples.reserve(shdr.size() - 1);
		for (std::vector<SF2Sample>::iterator it_shdr = shdr.begin(), it_end = --shdr.end(); it_shdr != it_end; ++it_shdr) {
			samples.push_back({ *it_shdr, sample_buffer, p_synth });
			if (p_synth->get_load_error()) {
				return;
			}
		}
	}
};

struct StereoValue {
	float left, right;

	StereoValue() {
		left = 0.0f;
		right = 0.0f;
	}
	StereoValue(float p_l, float p_r) :
			left(p_l), right(p_r) {}

	inline StereoValue operator*(float p_b) const {
		return { left * p_b, right * p_b };
	}

	inline StereoValue &operator+=(const StereoValue &p_b) {
		left += p_b.left;
		right += p_b.right;
		return *this;
	}
};

static inline StereoValue operator*(float p_a, const StereoValue &p_b) {
	return { p_a * p_b.left, p_a * p_b.right };
}

static inline StereoValue calculate_panned_volume(float p_pan) {
	if (p_pan <= -500.0f) {
		return { 1.0f, 0.0f };
	} else if (p_pan >= 500.0f) {
		return { 0.0f, 1.0f };
	} else {
		return { sinf(PAN_FACTOR * (-p_pan + 500.0f)), sinf(PAN_FACTOR * (p_pan + 500.0f)) };
	}
}

class Synthesizer::Voice {
public:
	enum class State {
		PLAYING,
		SUSTAINED,
		RELEASED,
		FINISHED,
		UNUSED
	};

	Voice() :
			status(State::UNUSED) {
	}

	inline size_t get_channel() const {
		return channel;
	}

	inline size_t get_note_id() const {
		return note_id;
	}

	inline float get_amp() const {
		return amp;
	}

	inline unsigned int get_steps() const {
		return steps;
	}

	inline uint8_t get_actual_key() const {
		return actual_key;
	}

	inline int16_t get_exclusive_class() const {
		return generators.get_or_default(SF2Generator::EXCLUSIVE_CLASS);
	}

	inline const State &get_status() const {
		return status;
	}

	inline StereoValue render() const {
		const uint32_t i = index.get_integer_part();
		const float r = index.get_fractional_part();
		const float interpolated = (1.0f - r) * sample_buffer->operator[](i) + r * sample_buffer->operator[](i + 1);
		return amp * volume * (interpolated / INT16_MAX);
	}

	void init(size_t p_channel, size_t p_note_id, float p_output_rate, const Sample &p_sample, const GeneratorSet &p_generators,
			const ModulatorParameterSet &p_mod_params, uint8_t p_key, uint8_t p_velocity, bool p_percussion) {
		channel = p_channel;
		note_id = p_note_id;
		actual_key = p_key;
		sample_buffer = p_sample.buffer;
		generators = p_generators;
		percussion = p_percussion;
		fine_tuning = 0.0;
		coarse_tuning = 0.0;
		steps = 0;
		status = State::PLAYING;
		index = p_sample.start;
		delta_index = 0u;
		volume = { 1.0f, 1.0f };
		amp = 0.0;
		delta_amp = 0.0;
		vol_env = { p_output_rate, CALC_INTERVAL };
		mod_env = { p_output_rate, CALC_INTERVAL };
		vib_lfo = { p_output_rate, CALC_INTERVAL };
		mod_lfo = { p_output_rate, CALC_INTERVAL };

		rt_sample.mode = (SampleMode)(0b11 & generators.get_or_default(SF2Generator::SAMPLE_MODES));
		const int16_t overridden_sample_key = generators.get_or_default(SF2Generator::OVERRIDING_ROOT_KEY);
		rt_sample.pitch = (overridden_sample_key > 0 ? overridden_sample_key : p_sample.key) - 0.01f * p_sample.correction;

		rt_sample.start = p_sample.start + COARSE_UNIT * generators.get_or_default(SF2Generator::START_ADDRESS_COARSE_OFFSET) +
				generators.get_or_default(SF2Generator::START_ADDRESS_OFFSET);
		rt_sample.end = p_sample.end + COARSE_UNIT * generators.get_or_default(SF2Generator::END_ADDRESS_COARSE_OFFSET) +
				generators.get_or_default(SF2Generator::END_ADDRESS_OFFSET);
		rt_sample.start_loop = p_sample.start_loop +
				COARSE_UNIT * generators.get_or_default(SF2Generator::START_LOOP_ADDRESS_COARSE_OFFSET) +
				generators.get_or_default(SF2Generator::START_LOOP_ADDRESS_OFFSET);
		rt_sample.end_loop = p_sample.end_loop +
				COARSE_UNIT * generators.get_or_default(SF2Generator::END_LOOP_ADDRESS_COARSE_OFFSET) +
				generators.get_or_default(SF2Generator::END_LOOP_ADDRESS_OFFSET);

		// fix invalid sample range
		const uint32_t buffer_size = (uint32_t)p_sample.buffer->size();
		rt_sample.start = std::min(buffer_size - 1, rt_sample.start);
		rt_sample.end = std::max(rt_sample.start + 1, std::min(buffer_size, rt_sample.end));
		rt_sample.start_loop =
				std::max(rt_sample.start, std::min(rt_sample.end - 1, rt_sample.start_loop));
		rt_sample.end_loop =
				std::max(rt_sample.start_loop + 1, std::min(rt_sample.end, rt_sample.end_loop));

		delta_index_ratio = 1.0 / key_to_hertz(rt_sample.pitch) * p_sample.sample_rate / p_output_rate;

		modulators.clear();
		for (const ModList &mp : p_mod_params.get_parameters()) {
			modulators.push_back({ mp });
		}

		const int16_t gen_velocity = generators.get_or_default(SF2Generator::VELOCITY);
		update_sf2_controller(GeneralController::NOTE_ON_VELOCITY, gen_velocity > 0 ? gen_velocity : p_velocity);

		const int16_t gen_key = generators.get_or_default(SF2Generator::KEY_NUMBER);
		const int16_t overridden_key = gen_key > 0 ? gen_key : p_key;
		key_scaling = 60 - overridden_key;
		update_sf2_controller(GeneralController::NOTE_ON_KEY_NUMBER, overridden_key);

		float min_modulated_atten = ATTEN_FACTOR * generators.get_or_default(SF2Generator::INITIAL_ATTENUATION);
		for (const Modulator &mod : modulators) {
			if (mod.get_destination() == SF2Generator::INITIAL_ATTENUATION && mod.can_be_negative()) {
				// mod may increase volume
				min_modulated_atten -= abs(mod.get_amount());
			}
		}
		min_atten = p_sample.min_atten + fmax(0.0f, min_modulated_atten);

		for (size_t i = 0; i < NUM_GENERATORS; ++i) {
			modulated[i] = generators.get_or_default((SF2Generator)i);
		}
		static const std::initializer_list<SF2Generator> INIT_GENERATORS = {
			SF2Generator::PAN, SF2Generator::DELAY_MOD_LFO, SF2Generator::FREQ_MOD_LFO,
			SF2Generator::DELAY_VIB_LFO, SF2Generator::FREQ_VIB_LFO, SF2Generator::DELAY_MOD_ENV,
			SF2Generator::ATTACK_MOD_ENV, SF2Generator::HOLD_MOD_ENV, SF2Generator::DECAY_MOD_ENV,
			SF2Generator::SUSTAIN_MOD_ENV, SF2Generator::RELEASE_MOD_ENV, SF2Generator::DELAY_VOL_ENV,
			SF2Generator::ATTACK_VOL_ENV, SF2Generator::HOLD_VOL_ENV, SF2Generator::DECAY_VOL_ENV,
			SF2Generator::SUSTAIN_VOL_ENV, SF2Generator::RELEASE_VOL_ENV, SF2Generator::COARSE_TUNE
		};
		for (const SF2Generator &generator : INIT_GENERATORS) {
			update_modulated_params(generator);
		}
	}

	inline void set_status(State p_status) {
		status = p_status;
	}

	void update_sf2_controller(GeneralController p_controller, float p_value) {
		for (Modulator &mod : modulators) {
			if (mod.update_sf2_controller(p_controller, p_value)) {
				update_modulated_params(mod.get_destination());
			}
		}
	}

	void update_midi_controller(uint8_t p_controller, uint8_t p_value) {
		for (Modulator &mod : modulators) {
			if (mod.update_midi_controller(p_controller, p_value)) {
				update_modulated_params(mod.get_destination());
			}
		}
	}

	void update_fine_tuning(float p_fine_tuning) {
		fine_tuning = p_fine_tuning;
		update_modulated_params(SF2Generator::FINE_TUNE);
	}

	void update_coarse_tuning(float p_coarse_tuning) {
		coarse_tuning = p_coarse_tuning;
		update_modulated_params(SF2Generator::COARSE_TUNE);
	}

	void release(bool p_sustained) {
		if (status != State::PLAYING && status != State::SUSTAINED) {
			return;
		}
		if (p_sustained) {
			status = State::SUSTAINED;
		} else {
			status = State::RELEASED;
			vol_env.release();
			mod_env.release();
		}
	}

	void update() {
		const bool calc = steps++ % CALC_INTERVAL == 0;

		if (calc) {
			// dynamic range of signed 16 bit samples in centibel
			static const float DYNAMIC_RANGE = 200.0f * log10f(INT16_MAX + 1.0f);
			if (vol_env.get_phase() == Envelope::Phase::FINISHED ||
					(vol_env.get_phase() > Envelope::Phase::ATTACK &&
							min_atten + 960.0f * (1.0f - vol_env.get_value()) >= DYNAMIC_RANGE)) {
				status = State::FINISHED;
				return;
			}

			vol_env.update();
		}

		index += delta_index;

		switch (rt_sample.mode) {
			case SampleMode::LOOPED:
				if (index.get_integer_part() >= rt_sample.end_loop) {
					index -= FixedPoint(rt_sample.end_loop - rt_sample.start_loop);
				}
				break;
			case SampleMode::LOOPED_UNTIL_RELEASE:
				if (status == State::RELEASED) {
					if (index.get_integer_part() >= rt_sample.end) {
						status = State::FINISHED;
						return;
					}
				} else if (index.get_integer_part() >= rt_sample.end_loop) {
					index -= FixedPoint(rt_sample.end_loop - rt_sample.start_loop);
				}
				break;
			case SampleMode::UNLOOPED:
			case SampleMode::UNUSED:
			default:
				if (index.get_integer_part() >= rt_sample.end) {
					status = State::FINISHED;
					return;
				}
				break;
		}

		amp += delta_amp;

		if (calc) {
			mod_env.update();
			vib_lfo.update();
			mod_lfo.update();

			const float mod_env_value =
					mod_env.get_phase() == Envelope::Phase::ATTACK ? convex_curve(mod_env.get_value()) : mod_env.get_value();
			const float pitch =
					voice_pitch + 0.01f * (get_modulated_generator(SF2Generator::MOD_ENV_TO_PITCH) * mod_env_value + get_modulated_generator(SF2Generator::VIB_LFO_TO_PITCH) * vib_lfo.get_value() + get_modulated_generator(SF2Generator::MOD_LFO_TO_PITCH) * mod_lfo.get_value());
			delta_index = FixedPoint(delta_index_ratio * key_to_hertz(pitch));

			const float atten_mod_lfo = get_modulated_generator(SF2Generator::MOD_LFO_TO_VOLUME) * mod_lfo.get_value();
			const float target_amp = vol_env.get_phase() == Envelope::Phase::ATTACK
					? vol_env.get_value() * attenuation_to_amplitude(atten_mod_lfo)
					: attenuation_to_amplitude(960.0f * (1.0f - vol_env.get_value()) + atten_mod_lfo);
			delta_amp = (target_amp - amp) / CALC_INTERVAL;
		}
	}

private:
	enum class SampleMode {
		UNLOOPED,
		LOOPED,
		UNUSED,
		LOOPED_UNTIL_RELEASE
	};

	class FixedPoint {
	public:
		FixedPoint() {
			raw = (uint64_t)0;
		}

		explicit FixedPoint(uint32_t p_integer) :
				raw((uint64_t)p_integer << 32) {
		}

		explicit FixedPoint(float p_value) :
				raw(((uint64_t)p_value << 32) | (uint32_t)((p_value - (uint32_t)p_value) * ((float)UINT32_MAX + 1.0f))) {
		}

		inline uint32_t get_integer_part() const {
			return raw >> 32;
		}

		inline float get_fractional_part() const {
			return (raw & UINT32_MAX) / ((float)UINT32_MAX + 1.0f);
		}

		inline FixedPoint &operator+=(const FixedPoint &p_b) {
			raw += p_b.raw;
			return *this;
		}

		inline FixedPoint &operator-=(const FixedPoint &p_b) {
			raw -= p_b.raw;
			return *this;
		}

		inline FixedPoint &operator=(uint32_t p_integer) {
			raw = (uint64_t)p_integer << 32;
			return *this;
		}

	private:
		uint64_t raw;
	};

	struct RuntimeSample {
		SampleMode mode;
		float pitch;
		uint32_t start, end, start_loop, end_loop;
	};

	size_t channel;
	size_t note_id;
	uint8_t actual_key;
	const std::vector<int16_t> *sample_buffer;
	GeneratorSet generators;
	RuntimeSample rt_sample;
	int key_scaling;
	std::vector<Modulator> modulators;
	float min_atten;
	float modulated[NUM_GENERATORS];
	bool percussion;
	float fine_tuning, coarse_tuning;
	float delta_index_ratio;
	unsigned int steps;
	State status;
	float voice_pitch;
	FixedPoint index, delta_index;
	StereoValue volume;
	float amp, delta_amp;
	Envelope vol_env, mod_env;
	LFO vib_lfo, mod_lfo;

	inline float get_modulated_generator(SF2Generator p_type) const {
		return modulated[(size_t)p_type];
	}

	void update_modulated_params(SF2Generator p_destination) {
		float &new_modulated = modulated[(size_t)p_destination];
		new_modulated = generators.get_or_default(p_destination);
		if (p_destination == SF2Generator::INITIAL_ATTENUATION) {
			new_modulated *= ATTEN_FACTOR;
		}
		for (const Modulator &mod : modulators) {
			if (mod.get_destination() == p_destination) {
				new_modulated += mod.get_value();
			}
		}

		switch (p_destination) {
			case SF2Generator::PAN:
			case SF2Generator::INITIAL_ATTENUATION:
				volume = attenuation_to_amplitude(get_modulated_generator(SF2Generator::INITIAL_ATTENUATION)) *
						calculate_panned_volume(get_modulated_generator(SF2Generator::PAN));
				break;
			case SF2Generator::DELAY_MOD_LFO:
				mod_lfo.set_delay(new_modulated);
				break;
			case SF2Generator::FREQ_MOD_LFO:
				mod_lfo.set_frequency(new_modulated);
				break;
			case SF2Generator::DELAY_VIB_LFO:
				vib_lfo.set_delay(new_modulated);
				break;
			case SF2Generator::FREQ_VIB_LFO:
				vib_lfo.set_frequency(new_modulated);
				break;
			case SF2Generator::DELAY_MOD_ENV:
				mod_env.set_parameter(Envelope::Phase::DELAY, new_modulated);
				break;
			case SF2Generator::ATTACK_MOD_ENV:
				mod_env.set_parameter(Envelope::Phase::ATTACK, new_modulated);
				break;
			case SF2Generator::HOLD_MOD_ENV:
			case SF2Generator::KEY_NUM_TO_MOD_ENV_HOLD:
				mod_env.set_parameter(Envelope::Phase::HOLD,
						get_modulated_generator(SF2Generator::HOLD_MOD_ENV) +
								get_modulated_generator(SF2Generator::KEY_NUM_TO_MOD_ENV_HOLD) * key_scaling);
				break;
			case SF2Generator::DECAY_MOD_ENV:
			case SF2Generator::KEY_NUM_TO_MOD_ENV_DECAY:
				mod_env.set_parameter(Envelope::Phase::DECAY,
						get_modulated_generator(SF2Generator::DECAY_MOD_ENV) +
								get_modulated_generator(SF2Generator::KEY_NUM_TO_MOD_ENV_DECAY) * key_scaling);
				break;
			case SF2Generator::SUSTAIN_MOD_ENV:
				mod_env.set_parameter(Envelope::Phase::SUSTAIN, new_modulated);
				break;
			case SF2Generator::RELEASE_MOD_ENV:
				mod_env.set_parameter(Envelope::Phase::RELEASE, new_modulated);
				break;
			case SF2Generator::DELAY_VOL_ENV:
				vol_env.set_parameter(Envelope::Phase::DELAY, new_modulated);
				break;
			case SF2Generator::ATTACK_VOL_ENV:
				vol_env.set_parameter(Envelope::Phase::ATTACK, new_modulated);
				break;
			case SF2Generator::HOLD_VOL_ENV:
			case SF2Generator::KEY_NUM_TO_VOL_ENV_HOLD:
				vol_env.set_parameter(Envelope::Phase::HOLD,
						get_modulated_generator(SF2Generator::HOLD_VOL_ENV) +
								get_modulated_generator(SF2Generator::KEY_NUM_TO_VOL_ENV_HOLD) * key_scaling);
				break;
			case SF2Generator::DECAY_VOL_ENV:
			case SF2Generator::KEY_NUM_TO_VOL_ENV_DECAY:
				vol_env.set_parameter(Envelope::Phase::DECAY,
						get_modulated_generator(SF2Generator::DECAY_VOL_ENV) +
								get_modulated_generator(SF2Generator::KEY_NUM_TO_VOL_ENV_DECAY) * key_scaling);
				break;
			case SF2Generator::SUSTAIN_VOL_ENV:
				vol_env.set_parameter(Envelope::Phase::SUSTAIN, new_modulated);
				break;
			case SF2Generator::RELEASE_VOL_ENV:
				vol_env.set_parameter(Envelope::Phase::RELEASE, new_modulated);
				break;
			case SF2Generator::COARSE_TUNE:
			case SF2Generator::FINE_TUNE:
			case SF2Generator::SCALE_TUNING:
			case SF2Generator::PITCH:
				voice_pitch = rt_sample.pitch + 0.01f * get_modulated_generator(SF2Generator::PITCH) +
						0.01f * generators.get_or_default(SF2Generator::SCALE_TUNING) * (actual_key - rt_sample.pitch) +
						coarse_tuning + get_modulated_generator(SF2Generator::COARSE_TUNE) +
						0.01f * (fine_tuning + get_modulated_generator(SF2Generator::FINE_TUNE));
				break;
			default:
				break;
		}
	}
};
class Synthesizer::Channel {
public:
	struct Bank {
		uint8_t msb, lsb;
	};

	explicit Channel(size_t p_index, float p_output_rate, std::vector<Voice *> *p_voices) :
			channel_index(p_index), output_rate(p_output_rate), controllers(), rpns(), key_pressures(), current_channel_pressure(0), current_pitch_bend(1 << 13), data_entry_mode(DataEntryMode::RPN), pitch_bend_sensitivity(2.0f), fine_tuning(0.0f), coarse_tuning(0.0f), current_note_id(0) {
		controllers[(size_t)ControlChange::VOLUME] = 100;
		controllers[(size_t)ControlChange::PAN] = 64;
		controllers[(size_t)ControlChange::EXPRESSION] = 127;
		controllers[(size_t)ControlChange::RPN_LSB] = 127;
		controllers[(size_t)ControlChange::RPN_MSB] = 127;
		voices = p_voices;
	}

	inline Bank get_bank() const {
		return { controllers[(size_t)ControlChange::BANK_SELECT_MSB], controllers[(size_t)ControlChange::BANK_SELECT_LSB] };
	}

	inline bool has_preset() const {
		return (bool)preset;
	}

	void note_off(uint8_t p_key) {
		const bool sustained = controllers[(size_t)ControlChange::SUSTAIN] >= 64;

		for (Voice *voice : *voices) {
			if (voice->get_status() != Voice::State::UNUSED && voice->get_channel() == channel_index &&
					voice->get_actual_key() == p_key) {
				voice->release(sustained);
			}
		}
	}

	void note_on(uint8_t p_key, uint8_t p_velocity) {
		if (p_velocity == 0) {
			note_off(p_key);
			return;
		}

		for (const Zone &preset_zone : preset->zones) {
			if (preset_zone.is_in_range(p_key, p_velocity)) {
				const int16_t inst_id = preset_zone.generators.get_or_default(SF2Generator::INSTRUMENT);
				const Instrument &inst = preset->soundfont->get_instruments()[inst_id];
				for (const Zone &inst_zone : inst.zones) {
					if (inst_zone.is_in_range(p_key, p_velocity)) {
						const int16_t sample_id = inst_zone.generators.get_or_default(SF2Generator::SAMPLE_ID);
						const Sample &sample = preset->soundfont->get_samples()[sample_id];

						GeneratorSet generators = inst_zone.generators;
						generators.add(preset_zone.generators);

						ModulatorParameterSet modparams = inst_zone.modulator_parameters;
						modparams.merge_and_add(preset_zone.modulator_parameters);
						modparams.merge(ModulatorParameterSet::get_default_parameters());

						Voice *voice = get_voice(generators.get_or_default(SF2Generator::EXCLUSIVE_CLASS));

						voice->init(channel_index, current_note_id, output_rate, sample, generators, modparams, p_key,
								p_velocity, preset->bank == PERCUSSION_BANK);
						voice->update_sf2_controller(GeneralController::POLYPHONIC_PRESSURE,
								key_pressures[voice->get_actual_key()]);
						voice->update_sf2_controller(GeneralController::CHANNEL_PRESSURE, current_channel_pressure);
						voice->update_sf2_controller(GeneralController::PITCH_WHEEL, current_pitch_bend);
						voice->update_sf2_controller(GeneralController::PITCH_WHEEL_SENSITIVITY, pitch_bend_sensitivity);
						voice->update_fine_tuning(fine_tuning);
						voice->update_coarse_tuning(coarse_tuning);
						for (uint8_t i = 0; i < NUM_CONTROLLERS; ++i) {
							voice->update_midi_controller(i, controllers[i]);
						}
					}
				}
			}
		}
		++current_note_id;
	}

	void key_pressure(uint8_t p_key, uint8_t p_value) {
		key_pressures[p_key] = p_value;

		for (Voice *voice : *voices) {
			if (voice->get_status() != Voice::State::UNUSED && voice->get_channel() == channel_index &&
					voice->get_actual_key() == p_key) {
				voice->update_sf2_controller(GeneralController::POLYPHONIC_PRESSURE, p_value);
			}
		}
	}

	void control_change(uint8_t p_controller, uint8_t p_value) {
		controllers[p_controller] = p_value;

		switch ((ControlChange)p_controller) {
			case ControlChange::DATA_ENTRY_MSB:
			case ControlChange::DATA_ENTRY_LSB:
				if (data_entry_mode == DataEntryMode::RPN) {
					const uint16_t rpn = get_selected_rpn();
					if (rpn < (uint16_t)RPN::LAST) {
						const uint16_t data = ((uint16_t)controllers[(size_t)ControlChange::DATA_ENTRY_MSB] << 7) +
								(uint16_t)controllers[(size_t)ControlChange::DATA_ENTRY_LSB];
						rpns[rpn] = data;
						update_rpn();
					}
				}
				break;
			case ControlChange::SUSTAIN:
				if (p_value < 64) {
					for (Voice *voice : *voices) {
						if (voice->get_status() != Voice::State::UNUSED && voice->get_channel() == channel_index &&
								voice->get_status() == Voice::State::SUSTAINED) {
							voice->release(false);
						}
					}
				}
				break;
			case ControlChange::DATA_INCREMENT:
				if (data_entry_mode == DataEntryMode::RPN) {
					const uint16_t rpn = get_selected_rpn();
					if (rpn < (uint16_t)RPN::LAST && rpns[rpn] >> 7 < 127) {
						rpns[rpn] += 1 << 7;
						update_rpn();
					}
				}
				break;
			case ControlChange::DATA_DECREMENT:
				if (data_entry_mode == DataEntryMode::RPN) {
					const uint16_t rpn = get_selected_rpn();
					if (rpn < (uint16_t)RPN::LAST && rpns[rpn] >> 7 > 0) {
						rpns[rpn] -= 1 << 7;
						update_rpn();
					}
				}
				break;
			case ControlChange::NRPN_MSB:
			case ControlChange::NRPN_LSB:
				data_entry_mode = DataEntryMode::NRPN;
				break;
			case ControlChange::RPN_MSB:
			case ControlChange::RPN_LSB:
				data_entry_mode = DataEntryMode::RPN;
				break;
			case ControlChange::ALL_SOUND_OFF:
				for (Voice *voice : *voices) {
					if (voice->get_status() != Voice::State::UNUSED && voice->get_channel() == channel_index) {
						voice->set_status(Voice::State::FINISHED);
					}
				}
				break;
			case ControlChange::RESET_ALL_CONTROLLERS:
				// See "General MIDI System Level 1 Developer Guidelines" Second Revision
				// p.5 'Response to "Reset All Controllers" Message'
				memset(key_pressures, 0, MAX_KEY + 1);
				current_channel_pressure = 0;
				current_pitch_bend = 1 << 13;
				for (Voice *voice : *voices) {
					if (voice->get_status() != Voice::State::UNUSED && voice->get_channel() == channel_index) {
						voice->update_sf2_controller(GeneralController::CHANNEL_PRESSURE, current_channel_pressure);
						voice->update_sf2_controller(GeneralController::PITCH_WHEEL, current_pitch_bend);
					}
				}
				for (uint8_t i = 1; i < 122; ++i) {
					if ((91 <= i && i <= 95) || (70 <= i && i <= 79)) {
						continue;
					}
					switch ((ControlChange)i) {
						case ControlChange::VOLUME:
						case ControlChange::PAN:
						case ControlChange::BANK_SELECT_LSB:
						case ControlChange::ALL_SOUND_OFF:
							break;
						case ControlChange::EXPRESSION:
						case ControlChange::RPN_LSB:
						case ControlChange::RPN_MSB:
							controllers[i] = 127;
							for (Voice *voice : *voices) {
								if (voice->get_status() != Voice::State::UNUSED && voice->get_channel() == channel_index) {
									voice->update_midi_controller(i, 127);
								}
							}
							break;
						default:
							controllers[i] = 0;
							for (Voice *voice : *voices) {
								if (voice->get_status() != Voice::State::UNUSED && voice->get_channel() == channel_index) {
									voice->update_midi_controller(i, 0);
								}
							}
							break;
					}
				}
				break;
			case ControlChange::ALL_NOTES_OFF: {
				// See "The Complete MIDI 1.0 Detailed Specification" Rev. April 2006
				// p.A-6 'The Relationship Between the Hold Pedal and "All Notes Off"'

				// All Notes Off is affected by CC 64 (Sustain)
				const bool sustained = controllers[(size_t)ControlChange::SUSTAIN] >= 64;
				for (Voice *voice : *voices) {
					if (voice->get_status() != Voice::State::UNUSED && voice->get_channel() == channel_index) {
						voice->release(sustained);
					}
				}
				break;
			}
			default:
				for (Voice *voice : *voices) {
					if (voice->get_status() != Voice::State::UNUSED && voice->get_channel() == channel_index) {
						voice->update_midi_controller(p_controller, p_value);
					}
				}
				break;
		}
	}

	void channel_pressure(uint8_t p_value) {
		current_channel_pressure = p_value;
		for (Voice *voice : *voices) {
			if (voice->get_status() != Voice::State::UNUSED && voice->get_channel() == channel_index) {
				voice->update_sf2_controller(GeneralController::CHANNEL_PRESSURE, p_value);
			}
		}
	}

	void pitch_bend(uint16_t p_value) {
		current_pitch_bend = p_value;
		for (Voice *voice : *voices) {
			if (voice->get_status() != Voice::State::UNUSED && voice->get_channel() == channel_index) {
				voice->update_sf2_controller(GeneralController::PITCH_WHEEL, p_value);
			}
		}
	}

	inline void set_preset(const Preset *p_preset) {
		preset = p_preset;
	}

private:
	enum class ControlChange {
		BANK_SELECT_MSB = 0,
		MODULATION = 1,
		DATA_ENTRY_MSB = 6,
		VOLUME = 7,
		PAN = 10,
		EXPRESSION = 11,
		BANK_SELECT_LSB = 32,
		DATA_ENTRY_LSB = 38,
		SUSTAIN = 64,
		DATA_INCREMENT = 96,
		DATA_DECREMENT = 97,
		NRPN_LSB = 98,
		NRPN_MSB = 99,
		RPN_LSB = 100,
		RPN_MSB = 101,
		ALL_SOUND_OFF = 120,
		RESET_ALL_CONTROLLERS = 121,
		ALL_NOTES_OFF = 123
	};

	enum class DataEntryMode {
		RPN,
		NRPN
	};

	enum class RPN {
		PITCH_BEND_SENSITIVITY = 0,
		FINE_TUNING = 1,
		COARSE_TUNING = 2,
		LAST
	};

	const size_t channel_index;
	const float output_rate;
	const Preset *preset;
	uint8_t controllers[NUM_CONTROLLERS];
	uint16_t rpns[(size_t)RPN::LAST];
	uint8_t key_pressures[MAX_KEY + 1];
	uint8_t current_channel_pressure;
	uint16_t current_pitch_bend;
	DataEntryMode data_entry_mode;
	float pitch_bend_sensitivity;
	float fine_tuning, coarse_tuning;
	std::vector<Voice *> *voices;
	size_t current_note_id;

	inline uint16_t get_selected_rpn() const {
		return ((uint16_t)controllers[(size_t)ControlChange::RPN_MSB] << 7) +
				(uint16_t)controllers[(size_t)ControlChange::RPN_LSB];
	}

	Voice *get_voice(int16_t p_exclusive_class) {
		if (p_exclusive_class != 0) {
			for (Voice *v : *voices) {
				if (v->get_channel() == channel_index && v->get_note_id() != current_note_id &&
						v->get_exclusive_class() == p_exclusive_class) {
					v->release(false);
				}
			}
		}
		// Track these in case all voices are in use
		Voice *to_kill = nullptr;
		int lowest_score = 0;
		for (Voice *v : *voices) {
			Voice::State status = v->get_status();
			size_t chan = v->get_channel();
			if (status == Voice::State::UNUSED || status == Voice::State::FINISHED) {
				return v;
			}
			// This model for identifying a voice to kill is similar to Fluidsynth's:
			// - A released non-drum voice can likely be killed easily
			// - A sustained voice can likely be killed without sounding too abrupt
			// - Barring the above situations, an older voice (higher "steps" value)
			//   should be prioritized
			// - Lastly, skew somewhat more towards quieter voices (lower "amp" value)
			int score = 0;
			if (chan != PERCUSSION_CHANNEL && status == Voice::State::RELEASED) {
				score -= 300;
			}
			if (status == Voice::State::SUSTAINED) {
				score -= 200;
			}
			if (to_kill && v->get_steps() > to_kill->get_steps()) {
				score -= 100;
			}
			if (to_kill && v->get_amp() < to_kill->get_amp()) {
				score -= 50;
			}
			if (!to_kill) {
				lowest_score = score;
				to_kill = v;
			} else if (score < lowest_score) {
				lowest_score = score;
				to_kill = v;
			}
		}
		to_kill->release(false);
		return to_kill;
	}

	void update_rpn() {
		const uint16_t rpn = get_selected_rpn();
		const int32_t data = (int32_t)rpns[rpn];
		switch ((RPN)rpn) {
			case RPN::PITCH_BEND_SENSITIVITY:
				pitch_bend_sensitivity = data / 128.0f;
				for (Voice *voice : *voices) {
					if (voice->get_status() != Voice::State::UNUSED && voice->get_channel() == channel_index) {
						voice->update_sf2_controller(GeneralController::PITCH_WHEEL_SENSITIVITY, pitch_bend_sensitivity);
					}
				}
				break;
			case RPN::FINE_TUNING: {
				fine_tuning = (data - 8192) / 81.92f;
				for (Voice *voice : *voices) {
					if (voice->get_status() != Voice::State::UNUSED && voice->get_channel() == channel_index) {
						voice->update_fine_tuning(fine_tuning);
					}
				}
				break;
			}
			case RPN::COARSE_TUNING: {
				coarse_tuning = (data - 8192) / 128.0f;
				for (Voice *voice : *voices) {
					if (voice->get_status() != Voice::State::UNUSED && voice->get_channel() == channel_index) {
						voice->update_coarse_tuning(coarse_tuning);
					}
				}
				break;
			}
			default:
				break;
		}
	}
};

#ifdef TINYPRIMESYNTH_FLAC_SUPPORT
static const std::vector<int32_t> FIXED_PREDICTION_COEFFICIENTS[5] = {
	{},
	{ 1 },
	{ 2, -1 },
	{ 3, -3, 1 },
	{ 4, -6, 4, -1 },
};

#if defined(__GNUC__) || defined(__clang__)
inline uint32_t count_leading_zeroes(uint32_t p_x) {
	return __builtin_clz(p_x);
}
#elif defined(_MSC_VER)
inline uint32_t count_leading_zeroes(uint32_t value) {
	unsigned long leading_zero = 0;
	if (_BitScanReverse(&leading_zero, value)) {
		return 31 - leading_zero;
	} else {
		return 32;
	}
}
#else
static inline uint32_t count_leading_zeroes(uint32_t x) {
	static constexpr uint8_t const clz_lkup[] = {
		32U, 31U, 30U, 30U, 29U, 29U, 29U, 29U,
		28U, 28U, 28U, 28U, 28U, 28U, 28U, 28U,
		27U, 27U, 27U, 27U, 27U, 27U, 27U, 27U,
		27U, 27U, 27U, 27U, 27U, 27U, 27U, 27U,
		26U, 26U, 26U, 26U, 26U, 26U, 26U, 26U,
		26U, 26U, 26U, 26U, 26U, 26U, 26U, 26U,
		26U, 26U, 26U, 26U, 26U, 26U, 26U, 26U,
		26U, 26U, 26U, 26U, 26U, 26U, 26U, 26U,
		25U, 25U, 25U, 25U, 25U, 25U, 25U, 25U,
		25U, 25U, 25U, 25U, 25U, 25U, 25U, 25U,
		25U, 25U, 25U, 25U, 25U, 25U, 25U, 25U,
		25U, 25U, 25U, 25U, 25U, 25U, 25U, 25U,
		25U, 25U, 25U, 25U, 25U, 25U, 25U, 25U,
		25U, 25U, 25U, 25U, 25U, 25U, 25U, 25U,
		25U, 25U, 25U, 25U, 25U, 25U, 25U, 25U,
		25U, 25U, 25U, 25U, 25U, 25U, 25U, 25U,
		24U, 24U, 24U, 24U, 24U, 24U, 24U, 24U,
		24U, 24U, 24U, 24U, 24U, 24U, 24U, 24U,
		24U, 24U, 24U, 24U, 24U, 24U, 24U, 24U,
		24U, 24U, 24U, 24U, 24U, 24U, 24U, 24U,
		24U, 24U, 24U, 24U, 24U, 24U, 24U, 24U,
		24U, 24U, 24U, 24U, 24U, 24U, 24U, 24U,
		24U, 24U, 24U, 24U, 24U, 24U, 24U, 24U,
		24U, 24U, 24U, 24U, 24U, 24U, 24U, 24U,
		24U, 24U, 24U, 24U, 24U, 24U, 24U, 24U,
		24U, 24U, 24U, 24U, 24U, 24U, 24U, 24U,
		24U, 24U, 24U, 24U, 24U, 24U, 24U, 24U,
		24U, 24U, 24U, 24U, 24U, 24U, 24U, 24U,
		24U, 24U, 24U, 24U, 24U, 24U, 24U, 24U,
		24U, 24U, 24U, 24U, 24U, 24U, 24U, 24U,
		24U, 24U, 24U, 24U, 24U, 24U, 24U, 24U,
		24U, 24U, 24U, 24U, 24U, 24U, 24U, 24U
	};
	uint32_t n;
	if (x >= (1U << 16)) {
		if (x >= (1U << 24)) {
			n = 24U;
		} else {
			n = 16U;
		}
	} else {
		if (x >= (1U << 8)) {
			n = 8U;
		} else {
			n = 0U;
		}
	}
	return (uint32_t)clz_lkup[x >> n] - n;
}
#endif
class FLACBitStream {
public:
	FLACBitStream(FileAndMemReader *p_in) :
			raw_stream(p_in), bit_buffer(0), bit_buffer_length(0), stream_error(false) {}

	inline void align_to_byte() {
		bit_buffer_length -= bit_buffer_length % 8;
	}

	int32_t read_one() {
		if (staging_pos >= staging_fill) {
			if (raw_stream->eof()) {
				return -1;
			}
			staging_fill = raw_stream->read(staging_buffer, 1, 4096);
			staging_pos = 0;
			if (staging_fill == 0) {
				return -1;
			}
		}
		return staging_buffer[staging_pos++];
	}

	int32_t read_byte() {
		if (bit_buffer_length >= 8) {
			return read_unsigned_int(8);
		} else {
			return read_one();
		}
	}

	int32_t read_unsigned_int(int32_t p_n) {
		while (bit_buffer_length < p_n) {
			int32_t temp = read_one();
			bit_buffer = (bit_buffer << 8) | temp;
			bit_buffer_length += 8;
		}
		bit_buffer_length -= p_n;
		int32_t result = (int32_t)(bit_buffer >> bit_buffer_length);
		if (p_n < 32) {
			result &= (1 << p_n) - 1;
		}
		return result;
	}

	int32_t read_signed_int(int32_t p_n) {
		return (read_unsigned_int(p_n) << (32 - p_n)) >> (32 - p_n);
	}

	int64_t read_rice_signed_int(int32_t p_param) {
		int64_t val = 0;
		while (read_unsigned_int(1) == 0) {
			val++;
		}
		val = (val << p_param) | read_unsigned_int(p_param);
		return (val >> 1) ^ -(val & 1);
	}

	inline bool get_stream_error() const {
		return stream_error;
	}

	inline void set_stream_error(bool p_error) {
		stream_error = p_error;
	}

	inline bool stream_at_end() const {
		return raw_stream->eof();
	}

private:
	FileAndMemReader *raw_stream;
	int64_t bit_buffer;
	int32_t bit_buffer_length;
	bool stream_error;
	uint8_t staging_buffer[4096];
	size_t staging_pos = 0;
	size_t staging_fill = 0;
};

static void write_little_int(int32_t p_num_bytes, int32_t p_val, std::vector<uint8_t> &p_out) {
	for (int32_t i = 0; i < p_num_bytes; i++) {
		p_out.push_back((uint8_t)(p_val >> (i * 8)));
	}
}

static void restore_flac_linear_prediction(std::vector<int64_t> &p_result, const std::vector<int32_t> &p_coefs, int32_t p_shift) {
	for (int32_t i = p_coefs.size(); i < p_result.size(); i++) {
		int64_t sum = 0;
		for (int32_t j = 0; j < p_coefs.size(); j++) {
			sum += p_result[i - 1 - j] * p_coefs[j];
		}
		p_result[i] += sum >> p_shift;
	}
}

static void decode_flac_residuals(FLACBitStream &p_in, int32_t p_warmup, std::vector<int64_t> &p_result) {
	int32_t method = p_in.read_unsigned_int(2);
	if (method >= 2) {
		// Reserved residual coding method
		p_in.set_stream_error(true);
		return;
	}
	int32_t param_bits = method == 0 ? 4 : 5;
	int32_t escape_param = method == 0 ? 0xF : 0x1F;

	int32_t partition_order = p_in.read_unsigned_int(4);
	int32_t num_partitions = 1 << partition_order;
	if (p_result.size() % num_partitions != 0) {
		// Block size not divisible by number of Rice partitions
		p_in.set_stream_error(true);
		return;
	}
	int32_t partition_size = p_result.size() / num_partitions;

	for (int32_t i = 0; i < num_partitions; i++) {
		int32_t start = i * partition_size + (i == 0 ? p_warmup : 0);
		int32_t end = (i + 1) * partition_size;

		int32_t param = p_in.read_unsigned_int(param_bits);
		if (param < escape_param) {
			for (int32_t j = start; j < end; j++) {
				p_result[j] = p_in.read_rice_signed_int(param);
			}
		} else {
			int32_t num_bits = p_in.read_unsigned_int(5);
			for (int32_t j = start; j < end; j++) {
				p_result[j] = p_in.read_signed_int(num_bits);
			}
		}
	}
}

static void decode_flac_lpc_subframe(FLACBitStream &p_in, int32_t p_lpc_order, int32_t p_sample_depth, std::vector<int32_t> &p_coefs, std::vector<int64_t> &p_result) {
	for (int32_t i = 0; i < p_lpc_order; i++) {
		p_result[i] = p_in.read_signed_int(p_sample_depth);
	}
	int32_t precision = p_in.read_unsigned_int(4) + 1;
	int32_t shift = p_in.read_signed_int(5);
	p_coefs.resize(p_lpc_order);
	for (int32_t i = 0; i < p_coefs.size(); i++) {
		p_coefs[i] = p_in.read_signed_int(precision);
	}
	decode_flac_residuals(p_in, p_lpc_order, p_result);
	restore_flac_linear_prediction(p_result, p_coefs, shift);
}

static void decode_flac_fp_subframe(FLACBitStream &p_in, int32_t p_pred_order, int32_t p_sample_depth, std::vector<int64_t> &p_result) {
	for (int32_t i = 0; i < p_pred_order; i++) {
		p_result[i] = p_in.read_signed_int(p_sample_depth);
	}
	decode_flac_residuals(p_in, p_pred_order, p_result);
	restore_flac_linear_prediction(p_result, FIXED_PREDICTION_COEFFICIENTS[p_pred_order], 0);
}

static void decode_flac_subframe(FLACBitStream &p_in, int32_t p_sample_depth, std::vector<int64_t> &p_result, std::vector<int32_t> &p_coefs) {
	p_in.read_unsigned_int(1);
	int32_t type = p_in.read_unsigned_int(6);
	int32_t shift = p_in.read_unsigned_int(1);
	if (shift == 1) {
		while (p_in.read_unsigned_int(1) == 0) {
			shift++;
		}
	}
	p_sample_depth -= shift;

	if (type == 0) // Constant coding
	{
		int32_t filler = p_in.read_signed_int(p_sample_depth);
		for (size_t i = 0; i < p_result.size(); i++) {
			p_result[i] = filler;
		}
	} else if (type == 1) { // Verbatim coding
		for (int32_t i = 0; i < p_result.size(); i++) {
			p_result[i] = p_in.read_signed_int(p_sample_depth);
		}
	} else if (8 <= type && type <= 12) {
		decode_flac_fp_subframe(p_in, type - 8, p_sample_depth, p_result);
	} else if (32 <= type && type <= 63) {
		decode_flac_lpc_subframe(p_in, type - 31, p_sample_depth, p_coefs, p_result);
	} else {
		// Reserved subframe type
		p_in.set_stream_error(true);
		return;
	}

	for (int32_t i = 0; i < p_result.size(); i++) {
		p_result[i] <<= shift;
	}
}

static void decode_flac_subframes(FLACBitStream &p_in, int32_t p_sample_depth, int32_t p_chan_asgn, std::vector<std::vector<int32_t>> &p_samples, std::vector<std::vector<int64_t>> &p_subframes, std::vector<int32_t> &p_coefs) {
	int32_t block_size = p_samples[0].size();
	if (0 <= p_chan_asgn && p_chan_asgn <= 7) {
		for (int32_t ch = 0; ch < p_samples.size(); ch++) {
			decode_flac_subframe(p_in, p_sample_depth, p_subframes[ch], p_coefs);
		}
	} else if (8 <= p_chan_asgn && p_chan_asgn <= 10) {
		decode_flac_subframe(p_in, p_sample_depth + (p_chan_asgn == 9 ? 1 : 0), p_subframes[0], p_coefs);
		decode_flac_subframe(p_in, p_sample_depth + (p_chan_asgn == 9 ? 0 : 1), p_subframes[1], p_coefs);
		if (p_chan_asgn == 8) {
			for (int32_t i = 0; i < block_size; i++) {
				p_subframes[1][i] = p_subframes[0][i] - p_subframes[1][i];
			}
		} else if (p_chan_asgn == 9) {
			for (int32_t i = 0; i < block_size; i++) {
				p_subframes[0][i] += p_subframes[1][i];
			}
		} else if (p_chan_asgn == 10) {
			for (int32_t i = 0; i < block_size; i++) {
				int64_t side = p_subframes[1][i];
				int64_t right = p_subframes[0][i] - (side >> 1);
				p_subframes[1][i] = right;
				p_subframes[0][i] = right + side;
			}
		}
	} else {
		// Reserved channel assignment
		p_in.set_stream_error(true);
		return;
	}
	for (int32_t ch = 0; ch < p_samples.size(); ch++) {
		for (int32_t i = 0; i < block_size; i++) {
			p_samples[ch][i] = (int32_t)p_subframes[ch][i];
		}
	}
}

static bool decode_flac_frame(FLACBitStream &p_in, int32_t p_num_channels, int32_t p_sample_depth, std::vector<std::vector<int32_t>> &p_samples, std::vector<std::vector<int64_t>> &p_subframes, std::vector<int32_t> &p_coefs, std::vector<uint8_t> &p_out) {
	// Read a ton of header fields, and ignore most of them
	int32_t temp = p_in.read_byte();
	if (temp == -1) {
		return false;
	}
	int32_t sync = temp << 6 | p_in.read_unsigned_int(6);
	if (sync != 0x3FFE) {
		// not a sync code
		if (!p_in.stream_at_end()) { // need to see why this is happening at the end consistently :/
			p_in.set_stream_error(true);
		}
		return false;
	}

	p_in.read_unsigned_int(2);
	int32_t block_size_code = p_in.read_unsigned_int(4);
	int32_t sample_rate_code = p_in.read_unsigned_int(4);
	int32_t chan_asgn = p_in.read_unsigned_int(4);
	p_in.read_unsigned_int(4);

	temp = count_leading_zeroes(~(p_in.read_unsigned_int(8) << 24)) - 1;
	for (int32_t i = 0; i < temp; i++) {
		p_in.read_unsigned_int(8);
	}

	int32_t block_size;
	if (block_size_code == 1) {
		block_size = 192;
	} else if (2 <= block_size_code && block_size_code <= 5) {
		block_size = 576 << (block_size_code - 2);
	} else if (block_size_code == 6) {
		block_size = p_in.read_unsigned_int(8) + 1;
	} else if (block_size_code == 7) {
		block_size = p_in.read_unsigned_int(16) + 1;
	} else if (8 <= block_size_code && block_size_code <= 15) {
		block_size = 256 << (block_size_code - 8);
	} else {
		// unsupported block size?
		p_in.set_stream_error(true);
		return false;
	}

	if (sample_rate_code == 12) {
		p_in.read_unsigned_int(8);
	} else if (sample_rate_code == 13 || sample_rate_code == 14) {
		p_in.read_unsigned_int(16);
	}

	p_in.read_unsigned_int(8);

	// Decode each channel's subframe, then skip footer
	for (int32_t ch = 0; ch < p_num_channels; ++ch) {
		p_samples[ch].resize(block_size);
		p_subframes[ch].resize(block_size);
	}
	decode_flac_subframes(p_in, p_sample_depth, chan_asgn, p_samples, p_subframes, p_coefs);
	p_in.align_to_byte();
	p_in.read_unsigned_int(16);

	// Write the decoded samples
	for (int32_t i = 0; i < block_size; i++) {
		for (int32_t j = 0; j < p_num_channels; j++) {
			int32_t val = p_samples[j][i];
			if (p_sample_depth == 8) {
				val += 128;
			}
			write_little_int(p_sample_depth / 8, val, p_out);
		}
	}
	return true;
}

static std::vector<uint8_t> decode_sf2_flac(FLACBitStream &p_in) {
	// Handle FLAC header and metadata blocks
	std::vector<uint8_t> out;
	if (p_in.read_unsigned_int(32) != 0x664C6143) {
		return out; // Not a FLAC
	}
	int32_t sample_rate = -1;
	int32_t num_channels = -1;
	int32_t sample_depth = -1;
	int64_t num_samples = -1;
	for (bool last = false; !last;) {
		last = p_in.read_unsigned_int(1) != 0;
		int32_t type = p_in.read_unsigned_int(7);
		int32_t length = p_in.read_unsigned_int(24);
		if (type == 0) { // Stream info block
			p_in.read_unsigned_int(80);
			sample_rate = p_in.read_unsigned_int(20);
			num_channels = p_in.read_unsigned_int(3) + 1;
			sample_depth = p_in.read_unsigned_int(5) + 1;
			num_samples = (int64_t)p_in.read_unsigned_int(18) << 18 | p_in.read_unsigned_int(18);
			for (int32_t i = 0; i < 16; i++) {
				p_in.read_unsigned_int(8);
			}
		} else {
			for (int32_t i = 0; i < length; i++) {
				p_in.read_unsigned_int(8);
			}
		}
	}
	if (sample_rate == -1) {
		return out;
	}
	if (sample_depth % 8 != 0) {
		return out;
	}

	std::vector<std::vector<int32_t>> samples(num_channels);
	std::vector<std::vector<int64_t>> subframes(num_channels);
	std::vector<int32_t> coefs;

	// Decode FLAC audio frames and write raw samples
	while (decode_flac_frame(p_in, num_channels, sample_depth, samples, subframes, coefs, out)) {
		if (p_in.get_stream_error()) {
			break;
		}
	}

	if (p_in.get_stream_error()) {
		out.clear();
	}

	return out;
}
#endif // TINYPRIMESYNTH_FLAC_SUPPORT

class Synthesizer::Sequencer {
	class FixedFraction {
	public:
		FixedFraction() :
				num1(0), num2(1) {
		}
		FixedFraction(uint64_t p_value) :
				num1(p_value), num2(1) {
		}
		FixedFraction(uint64_t p_n, uint64_t p_d) :
				num1(p_n), num2(p_d) {
		}
		inline double value() const {
			return nom() / (double)denom();
		}
		FixedFraction &operator*=(const FixedFraction &p_b) {
			num1 *= p_b.nom();
			num2 *= p_b.denom();
			optim();
			return *this;
		}
		FixedFraction operator*(const FixedFraction &p_b) const {
			FixedFraction tmp(*this);
			tmp *= p_b;
			return tmp;
		}
		FixedFraction operator*(const uint64_t p_b) const {
			FixedFraction tmp(*this);
			tmp *= p_b;
			return tmp;
		}
		const uint64_t &nom() const {
			return num1;
		}
		const uint64_t &denom() const {
			return num2;
		}

	private:
		void optim() {
			/* Euclidean algorithm */
			uint64_t n1, n2, nn1, nn2;

			nn1 = num1;
			nn2 = num2;

			if (nn1 < nn2) {
				n1 = num1, n2 = num2;
			} else {
				n1 = num2, n2 = num1;
			}

			if (!num1) {
				num2 = 1;
				return;
			}
			for (;;) {
				uint64_t tmp = n2 % n1;
				if (!tmp) {
					break;
				}
				n2 = n1;
				n1 = tmp;
			}
			num1 /= n1;
			num2 /= n1;
		}

		uint64_t num1, num2;
	};
	struct MidiEvent {
		enum Types {
			//! Unknown event
			UNKNOWN = 0x00,
			//! Note-Off event
			NOTE_OFF = 0x08, // size == 2
							 //! Note-On event
			NOTE_ON = 0x09, // size == 2
							//! Note After-Touch event
			NOTE_TOUCH = 0x0A, // size == 2
							   //! Controller change event
			CONTROL_CHANGE = 0x0B, // size == 2
								   //! Patch change event
			PATCH_CHANGE = 0x0C, // size == 1
								 //! Channel After-Touch event
			CHANNEL_TOUCH = 0x0D, // size == 1
								  //! Pitch-bend change event
			PITCH_WHEEL = 0x0E, // size == 2

			//! System Exclusive message, type 1
			SYSEX = 0xF0, // size == len
						  //! Sys Com Song Position Pntr [LSB, MSB]
			SYS_COM_SONG_POSITION_POINTER = 0xF2, // size == 2
												  //! Sys Com Song Select(Song #) [0-127]
			SYS_COM_SONG_SELECT = 0xF3, // size == 1
										//! System Exclusive message, type 2
			SYSEX2 = 0xF7, // size == len
						   //! Special event
			SPECIAL = 0xFF
		};

		enum SubTypes {
			//! Sequension number
			SEQUENSION_NUMBER = 0x00, // size == 2
									  //! Text label
			TEXT = 0x01, // size == len
						 //! Copyright notice
			COPYRIGHT = 0x02, // size == len
							  //! Sequence track title
			SEQUENCE_TRACK_TITLE = 0x03, // size == len
										 //! Instrument title
			INSTRUMENT_TITLE = 0x04, // size == len
									 //! Lyrics text fragment
			LYRICS = 0x05, // size == len
						   //! MIDI Marker
			MARKER = 0x06, // size == len
						   //! Cue Point
			CUE_POINT = 0x07, // size == len
							  //! [Non-Standard] Device Switch
			DEVICE_SWITCH = 0x09, // size == len <CUSTOM>
								  //! MIDI Channel prefix
			MIDI_CHANNEL_PREFIX = 0x20, // size == 1

			//! End of Track event
			END_TRACK = 0x2F, // size == 0
							  //! Tempo change event
			TEMPO_CHANGE = 0x51, // size == 3
								 //! SMPTE offset
			SMPTE_OFFSET = 0x54, // size == 5
								 //! Time signature
			TIME_SIGNATURE = 0x55, // size == 4
								   //! Key signature
			KEY_SIGNATURE = 0x59, // size == 2
								  //! Sequencer specs
			SEQUENCER_SPEC = 0x7F, // size == len

			/* Non-standard, internal usage only */
			//! [Non-Standard] Loop Start point
			LOOP_START = 0xE1, // size == 0 <CUSTOM>
							   //! [Non-Standard] Loop End point
			LOOP_END = 0xE2, // size == 0 <CUSTOM>

			//! [Non-Standard] Loop Start point with support of multi-loops
			LOOP_STACK_BEGIN = 0xE4, // size == 1 <CUSTOM>
									 //! [Non-Standard] Loop End point with
									 //! support of multi-loops
			LOOP_STACK_END = 0xE5, // size == 0 <CUSTOM>
								   //! [Non-Standard] Loop End point with
								   //! support of multi-loops
			LOOP_STACK_BREAK = 0xE6, // size == 0 <CUSTOM>

			// Built-in hooks
			SONG_BEGIN_HOOK = 0x101
		};
		//! Main type of event
		uint_fast16_t type = MidiEvent::UNKNOWN;
		//! Sub-type of the event
		uint_fast16_t sub_type = MidiEvent::UNKNOWN;
		//! Targeted MIDI channel
		uint_fast16_t channel = 0;
		//! Is valid event
		uint_fast16_t is_valid = 1;
		//! Reserved 5 bytes padding
		uint_fast16_t padding[4] = { 0, 0, 0, 0 };
		//! Absolute tick position (Used for the tempo calculation only)
		uint64_t absolute_tick_position = 0;
		//! Raw data of this event
		std::vector<uint8_t> data;
	};
	class MidiTrackRow {
	public:
		MidiTrackRow() :
				time(0.0), delay(0), absolute_position(0), time_delay(0.0) {
		}
		//! Clear MIDI row data
		void Clear() {
			time = 0.0;
			delay = 0;
			absolute_position = 0;
			time_delay = 0.0;
			events.clear();
		}
		//! Absolute time position in seconds
		double time;
		//! Delay to next event in ticks
		uint64_t delay;
		//! Absolute position in ticks
		uint64_t absolute_position;
		//! Delay to next event in seconds
		double time_delay;
		//! List of MIDI events in the current row
		std::vector<MidiEvent> events;

		void SortEvents(bool *p_note_states) {
			typedef std::vector<MidiEvent> EvtArr;
			EvtArr sysex;
			EvtArr metas;
			EvtArr note_offs;
			EvtArr controllers;
			EvtArr any_other;

			for (size_t i = 0; i < events.size(); i++) {
				if (events[i].type == MidiEvent::NOTE_OFF) {
					if (note_offs.capacity()) {
						note_offs.reserve(events.size());
					}
					note_offs.push_back(events[i]);
				} else if (events[i].type == MidiEvent::SYSEX || events[i].type == MidiEvent::SYSEX2) {
					if (sysex.capacity() == 0) {
						sysex.reserve(events.size());
					}
					sysex.push_back(events[i]);
				} else if ((events[i].type == MidiEvent::CONTROL_CHANGE) ||
						(events[i].type == MidiEvent::PATCH_CHANGE) || (events[i].type == MidiEvent::PITCH_WHEEL) ||
						(events[i].type == MidiEvent::CHANNEL_TOUCH)) {
					if (controllers.capacity() == 0) {
						controllers.reserve(events.size());
					}
					controllers.push_back(events[i]);
				} else if ((events[i].type == MidiEvent::SPECIAL) &&
						((events[i].sub_type == MidiEvent::MARKER) ||
								(events[i].sub_type == MidiEvent::DEVICE_SWITCH) ||
								(events[i].sub_type == MidiEvent::SONG_BEGIN_HOOK) ||
								(events[i].sub_type == MidiEvent::LOOP_START) ||
								(events[i].sub_type == MidiEvent::LOOP_END) ||
								(events[i].sub_type == MidiEvent::LOOP_STACK_BEGIN) ||
								(events[i].sub_type == MidiEvent::LOOP_STACK_END) ||
								(events[i].sub_type == MidiEvent::LOOP_STACK_BREAK))) {
					if (metas.capacity() == 0) {
						metas.reserve(events.size());
					}
					metas.push_back(events[i]);
				} else {
					if (any_other.capacity() == 0) {
						any_other.reserve(events.size());
					}
					any_other.push_back(events[i]);
				}
			}

			/*
			 * If Note-Off and it's Note-On is on the same row - move this damned note
			 * off down!
			 */
			if (p_note_states) {
				std::set<size_t> mark_as_on;
				for (size_t i = 0; i < any_other.size(); i++) {
					const MidiEvent e = any_other[i];
					if (e.type == MidiEvent::NOTE_ON) {
						const size_t note_i = (size_t)(e.channel * 255) + (e.data[0] & 0x7F);
						// Check, was previously note is on or off
						bool wasOn = p_note_states[note_i];
						mark_as_on.insert(note_i);
						// Detect zero-length notes are following previously pressed
						// note
						int note_offs_on_same_note = 0;
						for (EvtArr::iterator j = note_offs.begin(); j != note_offs.end();) {
							// If note was off, and note-off on same row with note-on - move it down!
							if (((*j).channel == e.channel) && ((*j).data[0] == e.data[0])) {
								// If note is already off OR more than one note-off on same row and same note
								if (!wasOn || (note_offs_on_same_note != 0)) {
									any_other.push_back(*j);
									j = note_offs.erase(j);
									mark_as_on.erase(note_i);
									continue;
								} else {
									// When same row has many note-offs on same row
									// that means a zero-length note follows previous note
									// it must be shuted down
									note_offs_on_same_note++;
								}
							}
							j++;
						}
					}
				}

				// Mark other notes as released
				for (EvtArr::iterator j = note_offs.begin(); j != note_offs.end(); ++j) {
					size_t note_i = (size_t)(j->channel * 255) + (j->data[0] & 0x7F);
					p_note_states[note_i] = false;
				}

				for (std::set<size_t>::iterator j = mark_as_on.begin(); j != mark_as_on.end(); ++j) {
					p_note_states[*j] = true;
				}
			}

			events.clear();
			if (!sysex.empty()) {
				for (MidiEvent ev : sysex) {
					events.push_back(ev);
				}
			}
			if (!note_offs.empty()) {
				for (MidiEvent ev : note_offs) {
					events.push_back(ev);
				}
			}
			if (!metas.empty()) {
				for (MidiEvent ev : metas) {
					events.push_back(ev);
				}
			}
			if (!controllers.empty()) {
				for (MidiEvent ev : controllers) {
					events.push_back(ev);
				}
			}
			if (!any_other.empty()) {
				for (MidiEvent ev : any_other) {
					events.push_back(ev);
				}
			}
		}
	};
	struct TempoChangePoint {
		uint64_t absolute_position;
		FixedFraction tempo;
	};
	struct Position {
		//! Was track began playing
		bool began = false;
		//! Reserved
		char padding[7] = { 0, 0, 0, 0, 0, 0, 0 };
		//! Waiting time before next event in seconds
		double wait = 0.0;
		//! Absolute time position on the track in seconds
		double absolute_time_position = 0.0;
		//! Track information
		struct TrackInfo {
			//! Delay to next event in a track
			uint64_t delay = 0;
			//! Last handled event type
			int32_t last_handled_event = 0;
			//! Reserved
			char padding2[4];
			//! MIDI Events queue position iterator
			std::list<MidiTrackRow>::iterator pos;
		};
		std::vector<TrackInfo> track;
	};

	void build_smf_setup_reset(size_t p_track_count) {
		midi_full_song_time_length = 0.0;
		midi_loop_start_time = -1.0;
		midi_loop_end_time = -1.0;
		midi_loop_format = LoopFormat::DEFAULT;
		memset(channel_disabled, 0, 16 * sizeof(bool));
		midi_track_data.clear();
		midi_track_data.resize(p_track_count);

		midi_loop.reset();
		midi_loop.invalid_loop = false;
		midi_time.reset();

		midi_current_position.began = false;
		midi_current_position.absolute_time_position = 0.0;
		midi_current_position.wait = 0.0;
		midi_current_position.track.clear();
		midi_current_position.track.resize(p_track_count);
	}

	bool build_smf_track_data(const std::vector<std::vector<uint8_t>> &p_track_data) {
		const size_t track_count = p_track_data.size();
		build_smf_setup_reset(track_count);

		bool got_global_loop_start = false, got_global_loop_end = false, got_stack_loop_start = false,
			 got_loop_event_in_this_row = false;

		//! tick position of loop start tag
		uint64_t loop_start_ticks = 0;
		//! tick position of loop end tag
		uint64_t loop_end_ticks = 0;
		//! Full length of song in ticks
		uint64_t ticks_song_length = 0;

		//! Caches note on/off states.
		bool note_states[16 * 255];
		/* This is required to carefully detect zero-length notes           *
		 * and avoid a move of "note-off" event over "note-on" while sort.  *
		 * Otherwise, after sort those notes will play infinite sound       */

		//! Tempo change events list
		std::vector<MidiEvent> tempos_list;

		/*
		 * TODO: Make this be safer for memory in case of broken input data
		 * which may cause going away of available track data (and then give a
		 * crash!)
		 *
		 * POST: Check this more carefully for possible vulnuabilities are can crash
		 * this
		 */
		for (size_t tk = 0; tk < track_count; ++tk) {
			uint64_t abs_position = 0;
			int status = 0;
			MidiEvent event;
			bool ok = false;
			const uint8_t *end = p_track_data[tk].data() + p_track_data[tk].size();
			const uint8_t *track_ptr = p_track_data[tk].data();
			memset(note_states, 0, 4080 * sizeof(bool));

			// Time delay that follows the first event in the track
			{
				MidiTrackRow evt_pos;
				if (midi_format == FileFormat::RSXX) {
					ok = true;
				} else {
					evt_pos.delay = read_variable_length_value(&track_ptr, end, ok);
				}
				if (!ok) {
					return false;
				}

				// HACK: Begin every track with "Reset all controllers" event to
				// avoid controllers state break came from end of song
				if (tk == 0) {
					MidiEvent reset_event;
					reset_event.type = MidiEvent::SPECIAL;
					reset_event.sub_type = MidiEvent::SONG_BEGIN_HOOK;
					evt_pos.events.push_back(reset_event);
				}

				evt_pos.absolute_position = abs_position;
				abs_position += evt_pos.delay;
				midi_track_data[tk].push_back(evt_pos);
			}

			MidiTrackRow evt_pos;
			do {
				event = parse_event(&track_ptr, end, status);
				if (!event.is_valid) {
					return false;
				}

				evt_pos.events.push_back(event);
				if (event.type == MidiEvent::SPECIAL) {
					if (event.sub_type == MidiEvent::TEMPO_CHANGE) {
						event.absolute_tick_position = abs_position;
						tempos_list.push_back(event);
					} else if (!midi_loop.invalid_loop && (event.sub_type == MidiEvent::LOOP_START)) {
						/*
						 * loopStart is invalid when:
						 * - starts together with loopEnd
						 * - appears more than one time in same MIDI file
						 */
						if (got_global_loop_start || got_loop_event_in_this_row) {
							midi_loop.invalid_loop = true;
						} else {
							got_global_loop_start = true;
							loop_start_ticks = abs_position;
						}
						// In this row we got loop event, register this!
						got_loop_event_in_this_row = true;
					} else if (!midi_loop.invalid_loop && (event.sub_type == MidiEvent::LOOP_END)) {
						/*
						 * loopEnd is invalid when:
						 * - starts before loopStart
						 * - starts together with loopStart
						 * - appars more than one time in same MIDI file
						 */
						if (got_global_loop_end || got_loop_event_in_this_row) {
							midi_loop.invalid_loop = true;
						} else {
							got_global_loop_end = true;
							loop_end_ticks = abs_position;
						}
						// In this row we got loop event, register this!
						got_loop_event_in_this_row = true;
					} else if (!midi_loop.invalid_loop && (event.sub_type == MidiEvent::LOOP_STACK_BEGIN)) {
						if (!got_stack_loop_start) {
							if (!got_global_loop_start) {
								loop_start_ticks = abs_position;
							}
							got_stack_loop_start = true;
						}

						midi_loop.stack_up();
						if (midi_loop.stack_level >= (int)(midi_loop.stack.size())) {
							LoopStackEntry e;
							e.loops = event.data[0];
							e.infinity = (event.data[0] == 0);
							e.start = abs_position;
							e.end = abs_position;
							midi_loop.stack.push_back(e);
						}
					} else if (!midi_loop.invalid_loop && ((event.sub_type == MidiEvent::LOOP_STACK_END) || (event.sub_type == MidiEvent::LOOP_STACK_BREAK))) {
						if (midi_loop.stack_level <= -1) {
							midi_loop.invalid_loop = true; // Caught loop end without of loop start!
						} else {
							if (loop_end_ticks < abs_position) {
								loop_end_ticks = abs_position;
							}
							midi_loop.get_current_stack().end = abs_position;
							midi_loop.stack_down();
						}
					}
				}

				if (event.sub_type != MidiEvent::END_TRACK) // Don't try to read delta after
															// EndOfTrack event!
				{
					evt_pos.delay = read_variable_length_value(&track_ptr, end, ok);
					if (!ok) {
						/* End of track has been reached! However, there is no EOT
						 * event presented */
						event.type = MidiEvent::SPECIAL;
						event.sub_type = MidiEvent::END_TRACK;
					}
				}

				if ((evt_pos.delay > 0) || (event.sub_type == MidiEvent::END_TRACK)) {
					evt_pos.absolute_position = abs_position;
					abs_position += evt_pos.delay;
					evt_pos.SortEvents(note_states);
					midi_track_data[tk].push_back(evt_pos);
					evt_pos.Clear();
					got_loop_event_in_this_row = false;
				}
			} while ((track_ptr <= end) && (event.sub_type != MidiEvent::END_TRACK));

			if (ticks_song_length < abs_position) {
				ticks_song_length = abs_position;
			}
			// Set the chain of events begin
			if (midi_track_data[tk].size() > 0) {
				midi_current_position.track[tk].pos = midi_track_data[tk].begin();
			}
		}

		if (got_global_loop_start && !got_global_loop_end) {
			got_global_loop_end = true;
			loop_end_ticks = ticks_song_length;
		}

		// loopStart must be located before loopEnd!
		if (loop_start_ticks >= loop_end_ticks) {
			midi_loop.invalid_loop = true;
		}

		build_timeline(tempos_list, loop_start_ticks, loop_end_ticks);

		return true;
	}

	void build_timeline(const std::vector<MidiEvent> &p_tempos, uint64_t p_loop_start_ticks, uint64_t p_loop_end_ticks) {
		const size_t track_count = midi_track_data.size();
		/********************************************************************************/
		// Calculate time basing on collected tempo events
		/********************************************************************************/
		for (size_t tk = 0; tk < track_count; ++tk) {
			FixedFraction current_tempo = midi_tempo;
			double time = 0.0;
			size_t tempo_change_index = 0;
			std::list<MidiTrackRow> &track = midi_track_data[tk];
			if (track.empty()) {
				continue; // Empty track is useless!
			}

			MidiTrackRow *pos_prev = &(*(track.begin())); // First element
			for (std::list<MidiTrackRow>::iterator it = track.begin(); it != track.end(); ++it) {
				MidiTrackRow &pos = *it;
				if ((pos_prev != &pos) && // Skip first event
						(!p_tempos.empty()) && // Only when in-track tempo events are
											   // available
						(tempo_change_index < p_tempos.size())) {
					// If tempo event is going between of current and previous event
					if (p_tempos[tempo_change_index].absolute_tick_position <= pos.absolute_position) {
						// Stop points: begin point and tempo change points are
						// before end point
						std::vector<TempoChangePoint> points;
						FixedFraction t;
						TempoChangePoint first_point = { pos_prev->absolute_position, current_tempo };
						points.push_back(first_point);

						// Collect tempo change points between previous and current
						// events
						do {
							TempoChangePoint tempo_marker;
							const MidiEvent &tempo_point = p_tempos[tempo_change_index];
							tempo_marker.absolute_position = tempo_point.absolute_tick_position;
							tempo_marker.tempo =
									midi_individual_tick_delta *
									FixedFraction(read_int_big_endian(tempo_point.data.data(), tempo_point.data.size()));
							points.push_back(tempo_marker);
							tempo_change_index++;
						} while ((tempo_change_index < p_tempos.size()) &&
								(p_tempos[tempo_change_index].absolute_tick_position <= pos.absolute_position));

						// Re-calculate time delay of previous event
						time -= pos_prev->time_delay;
						pos_prev->time_delay = 0.0;

						for (size_t i = 0, j = 1; j < points.size(); i++, j++) {
							/* If one or more tempo events are appears between of
							 * two events, calculate delays between each tempo
							 * point, begin and end */
							uint64_t mid_delay = 0;
							// Delay between points
							mid_delay = points[j].absolute_position - points[i].absolute_position;
							// Time delay between points
							t = current_tempo * mid_delay;
							pos_prev->time_delay += t.value();

							// Apply next tempo
							current_tempo = points[j].tempo;
						}
						// Then calculate time between last tempo change point and
						// end point
						TempoChangePoint tail_tempo = points.back();
						uint64_t post_delay = pos.absolute_position - tail_tempo.absolute_position;
						t = current_tempo * post_delay;
						pos_prev->time_delay += t.value();

						// Store Common time delay
						pos_prev->time = time;
						time += pos_prev->time_delay;
					}
				}

				FixedFraction t = current_tempo * pos.delay;
				pos.time_delay = t.value();
				pos.time = time;
				time += pos.time_delay;

				// Capture loop points time positions
				if (!midi_loop.invalid_loop) {
					// Set loop points times
					if (p_loop_start_ticks == pos.absolute_position) {
						midi_loop_start_time = pos.time;
					} else if (p_loop_end_ticks == pos.absolute_position) {
						midi_loop_end_time = pos.time;
					}
				}
				pos_prev = &pos;
			}

			if (time > midi_full_song_time_length) {
				midi_full_song_time_length = time;
			}
		}

		midi_full_song_time_length += midi_post_song_wait_delay;
		// Set begin of the music
		midi_track_begin_position = midi_current_position;
		// Initial loop position will begin at begin of track until passing of the
		// loop point
		midi_loop_begin_position = midi_current_position;
		// Set lowest level of the loop stack
		midi_loop.stack_level = -1;

		// Set the count of loops
		midi_loop.loops_count = midi_loop_count;
		midi_loop.loops_left = midi_loop_count;

		/********************************************************************************/
		// Find and set proper loop points
		/********************************************************************************/
		if (!midi_loop.invalid_loop && !midi_current_position.track.empty()) {
			unsigned caught_loop_starts = 0;
			bool scan_done = false;
			const size_t ctrack_count = midi_current_position.track.size();
			Position row_position(midi_current_position);

			while (!scan_done) {
				const Position row_begin_position(row_position);

				for (size_t tk = 0; tk < ctrack_count; ++tk) {
					Position::TrackInfo &track = row_position.track[tk];
					if ((track.last_handled_event >= 0) && (track.delay <= 0)) {
						// Check is an end of track has been reached
						if (track.pos == midi_track_data[tk].end()) {
							track.last_handled_event = -1;
							continue;
						}

						for (size_t i = 0; i < track.pos->events.size(); i++) {
							const MidiEvent &evt = track.pos->events[i];
							if (evt.type == MidiEvent::SPECIAL && evt.sub_type == MidiEvent::LOOP_START) {
								caught_loop_starts++;
								scan_done = true;
								break;
							}
						}

						if (track.last_handled_event >= 0) {
							track.delay += track.pos->delay;
							++track.pos;
						}
					}
				}

				// Find a shortest delay from all track
				uint64_t shortest_delay = 0;
				bool shortest_delay_not_found = true;

				for (size_t tk = 0; tk < ctrack_count; ++tk) {
					Position::TrackInfo &track = row_position.track[tk];
					if ((track.last_handled_event >= 0) && (shortest_delay_not_found || track.delay < shortest_delay)) {
						shortest_delay = track.delay;
						shortest_delay_not_found = false;
					}
				}

				// Schedule the next playevent to be processed after that delay
				for (size_t tk = 0; tk < ctrack_count; ++tk) {
					row_position.track[tk].delay -= shortest_delay;
				}

				if (caught_loop_starts > 0) {
					midi_loop_begin_position = row_begin_position;
					midi_loop_begin_position.absolute_time_position = midi_loop_start_time;
					scan_done = true;
				}

				if (shortest_delay_not_found) {
					break;
				}
			}
		}
	}

	MidiEvent parse_event(const uint8_t **p_pptr, const uint8_t *p_end, int &p_status) {
		const uint8_t *&ptr = *p_pptr;
		Sequencer::MidiEvent evt;

		if (ptr + 1 > p_end) {
			// When track doesn't ends on the middle of event data, it's must be
			// fine
			evt.type = MidiEvent::SPECIAL;
			evt.sub_type = MidiEvent::END_TRACK;
			return evt;
		}

		unsigned char byte = *(ptr++);
		bool ok = false;

		if (byte == MidiEvent::SYSEX || byte == MidiEvent::SYSEX2) // Ignore SysEx
		{
			uint64_t length = read_variable_length_value(p_pptr, p_end, ok);
			if (!ok || (ptr + length > p_end)) {
				evt.is_valid = 0;
				return evt;
			}
			evt.type = MidiEvent::SYSEX;
			evt.data.clear();
			evt.data.push_back(byte);
			for (uint64_t i = 0; i < length; ++i) {
				evt.data.push_back(ptr[i]);
			}
			ptr += (size_t)length;
			return evt;
		}

		if (byte == MidiEvent::SPECIAL) {
			// Special event FF
			uint8_t evtype = *(ptr++);
			uint64_t length = read_variable_length_value(p_pptr, p_end, ok);
			if (!ok || (ptr + length > p_end)) {
				evt.is_valid = 0;
				return evt;
			}
			std::string data;
			if (length) {
				for (uint64_t i = 0; i < length; ++i) {
					data += (char)ptr[i];
				}
			}
			ptr += (size_t)length;

			evt.type = byte;
			evt.sub_type = evtype;
			for (int64_t i = 0; i < data.length(); ++i) {
				evt.data.push_back((uint8_t)data[i]);
			}

			if (evt.sub_type == MidiEvent::MARKER) {
				for (char &c : data) {
					c = tolower(c);
				}

				if (data == "loopstart") {
					// Return a custom Loop Start event instead of Marker
					evt.sub_type = MidiEvent::LOOP_START;
					evt.data.clear(); // Data is not needed
					return evt;
				}

				if (data == "loopend") {
					// Return a custom Loop End event instead of Marker
					evt.sub_type = MidiEvent::LOOP_END;
					evt.data.clear(); // Data is not needed
					return evt;
				}

				if (data.substr(0, 10) == "loopstart=") {
					evt.type = MidiEvent::SPECIAL;
					evt.sub_type = MidiEvent::LOOP_STACK_BEGIN;
					uint8_t loops = (uint8_t)stoi(data.substr(10));
					evt.data.clear();
					evt.data.push_back(loops);

					return evt;
				}

				if (data.substr(0, 8) == "loopend=") {
					evt.type = MidiEvent::SPECIAL;
					evt.sub_type = MidiEvent::LOOP_STACK_END;
					evt.data.clear();

					return evt;
				}
			}

			if (evtype == MidiEvent::END_TRACK) {
				p_status = -1; // Finalize track
			}

			return evt;
		}

		// Any normal event (80..EF)
		if (byte < 0x80) {
			byte = (uint8_t)(p_status | 0x80);
			ptr--;
		}

		// Sys Com Song Select(Song #) [0-127]
		if (byte == MidiEvent::SYS_COM_SONG_SELECT) {
			if (ptr + 1 > p_end) {
				evt.is_valid = 0;
				return evt;
			}
			evt.type = byte;
			evt.data.push_back(*(ptr++));
			return evt;
		}

		// Sys Com Song Position Pntr [LSB, MSB]
		if (byte == MidiEvent::SYS_COM_SONG_POSITION_POINTER) {
			if (ptr + 2 > p_end) {
				evt.is_valid = 0;
				return evt;
			}
			evt.type = byte;
			evt.data.push_back(*(ptr++));
			evt.data.push_back(*(ptr++));
			return evt;
		}

		uint8_t mid_ch = byte & 0x0F, ev_type = (byte >> 4) & 0x0F;
		p_status = byte;
		evt.channel = mid_ch;
		evt.type = ev_type;

		switch (ev_type) {
			case MidiEvent::NOTE_OFF: // 2 byte length
			case MidiEvent::NOTE_ON:
			case MidiEvent::NOTE_TOUCH:
			case MidiEvent::CONTROL_CHANGE:
			case MidiEvent::PITCH_WHEEL:
				if (ptr + 2 > p_end) {
					evt.is_valid = 0;
					return evt;
				}

				evt.data.push_back(*(ptr++));
				evt.data.push_back(*(ptr++));

				if ((ev_type == MidiEvent::NOTE_ON) && (evt.data[1] == 0)) {
					evt.type = MidiEvent::NOTE_OFF; // Note ON with zero velocity
													// is Note OFF!
				} else if (ev_type == MidiEvent::CONTROL_CHANGE) {
					// 111'th loopStart controller (RPG Maker and others)
					if (midi_format == FileFormat::MIDI) {
						switch (evt.data[0]) {
							case 110:
								if (midi_loop_format == LoopFormat::DEFAULT) {
									// Change event type to custom Loop Start event
									// and clear data
									evt.type = MidiEvent::SPECIAL;
									evt.sub_type = MidiEvent::LOOP_START;
									evt.data.clear();
									midi_loop_format = LoopFormat::HMI;
								} else if (midi_loop_format == LoopFormat::HMI) {
									// Repeating of 110'th point is BAD practice,
									// treat as EMIDI
									midi_loop_format = LoopFormat::EMIDI;
								}
								break;

							case 111:
								if (midi_loop_format == LoopFormat::HMI) {
									// Change event type to custom Loop End event
									// and clear data
									evt.type = MidiEvent::SPECIAL;
									evt.sub_type = MidiEvent::LOOP_END;
									evt.data.clear();
								} else if (midi_loop_format != LoopFormat::EMIDI) {
									// Change event type to custom Loop Start event
									// and clear data
									evt.type = MidiEvent::SPECIAL;
									evt.sub_type = MidiEvent::LOOP_START;
									evt.data.clear();
								}
								break;

							case 113:
								if (midi_loop_format == LoopFormat::EMIDI) {
									// EMIDI does using of CC113 with same purpose
									// as CC7
									evt.data[0] = 7;
								}
								break;
						}
					}
				}

				return evt;
			case MidiEvent::PATCH_CHANGE: // 1 byte length
			case MidiEvent::CHANNEL_TOUCH:
				if (ptr + 1 > p_end) {
					evt.is_valid = 0;
					return evt;
				}
				evt.data.push_back(*(ptr++));
				return evt;
			default:
				break;
		}

		return evt;
	}

	bool process_events() {
		if (midi_current_position.track.empty()) {
			midi_at_end = true; // No MIDI track data to play
		}
		if (midi_at_end) {
			return false; // No more events in the queue
		}

		midi_loop.caught_end = false;
		const size_t track_count = midi_current_position.track.size();
		const Position row_begin_position(midi_current_position);
		bool do_loop_jump = false;
		unsigned caught_loop_starts = 0;
		unsigned caught_loop_stack_starts = 0;
		unsigned caught_loop_stack_ends = 0;
		unsigned caught_loop_stack_breaks = 0;

		for (size_t tk = 0; tk < track_count; ++tk) {
			Position::TrackInfo &track = midi_current_position.track[tk];
			if ((track.last_handled_event >= 0) && (track.delay <= 0)) {
				// Check is an end of track has been reached
				if (track.pos == midi_track_data[tk].end()) {
					track.last_handled_event = -1;
					break;
				}

				// Handle event
				for (size_t i = 0; i < track.pos->events.size(); i++) {
					const MidiEvent &evt = track.pos->events[i];

					handle_event(tk, evt, track.last_handled_event);

					if (midi_loop.caught_start) {
						caught_loop_starts++;
						midi_loop.caught_start = false;
					}

					if (midi_loop.caught_stack_start) {
						caught_loop_stack_starts++;
						midi_loop.caught_stack_start = false;
					}

					if (midi_loop.caught_stack_break) {
						caught_loop_stack_breaks++;
						midi_loop.caught_stack_break = false;
					}

					if (midi_loop.caught_end || midi_loop.is_stack_end()) {
						if (midi_loop.caught_stack_end) {
							midi_loop.caught_stack_end = false;
							caught_loop_stack_ends++;
						}
						do_loop_jump = true;
						break; // Stop event handling on catching loopEnd event!
					}
				}

				// Read next event time (unless the track just ended)
				if (track.last_handled_event >= 0) {
					track.delay += track.pos->delay;
					++track.pos;
				}

				if (do_loop_jump) {
					break;
				}
			}
		}

		// Find a shortest delay from all track
		uint64_t shortest_delay = 0;
		bool shortest_delay_not_found = true;

		for (size_t tk = 0; tk < track_count; ++tk) {
			Position::TrackInfo &track = midi_current_position.track[tk];
			if ((track.last_handled_event >= 0) && (shortest_delay_not_found || track.delay < shortest_delay)) {
				shortest_delay = track.delay;
				shortest_delay_not_found = false;
			}
		}

		// Schedule the next playevent to be processed after that delay
		for (size_t tk = 0; tk < track_count; ++tk) {
			midi_current_position.track[tk].delay -= shortest_delay;
		}

		FixedFraction t = midi_tempo * shortest_delay;

		midi_current_position.wait += t.value();

		if (caught_loop_starts > 0 && midi_loop_begin_position.absolute_time_position <= 0.0) {
			midi_loop_begin_position = row_begin_position;
		}

		if (caught_loop_stack_starts > 0) {
			while (caught_loop_stack_starts > 0) {
				midi_loop.stack_up();
				LoopStackEntry &s = midi_loop.get_current_stack();
				s.start_position = row_begin_position;
				caught_loop_stack_starts--;
			}
			return true;
		}

		if (caught_loop_stack_breaks > 0) {
			while (caught_loop_stack_breaks > 0) {
				LoopStackEntry &s = midi_loop.get_current_stack();
				s.loops = 0;
				s.infinity = false;
				// Quit the loop
				midi_loop.stack_down();
				caught_loop_stack_breaks--;
			}
		}

		if (caught_loop_stack_ends > 0) {
			while (caught_loop_stack_ends > 0) {
				LoopStackEntry &s = midi_loop.get_current_stack();
				if (s.infinity) {
					midi_current_position = s.start_position;
					midi_loop.skip_stack_start = true;

					for (Synthesizer::Channel *channel : midi_synth->channels) {
						channel->control_change(123, 0);
					}

					return true;
				} else if (s.loops >= 0) {
					s.loops--;
					if (s.loops > 0) {
						midi_current_position = s.start_position;
						midi_loop.skip_stack_start = true;

						for (Channel *channel : midi_synth->channels) {
							channel->control_change(123, 0);
						}

						return true;
					} else {
						// Quit the loop
						midi_loop.stack_down();
					}
				} else {
					// Quit the loop
					midi_loop.stack_down();
				}
				caught_loop_stack_ends--;
			}

			return true;
		}

		if (shortest_delay_not_found || midi_loop.caught_end) {
			for (Synthesizer::Channel *channel : midi_synth->channels) {
				channel->control_change(123, 0);
			}

			// Loop if song end or loop end point has reached
			midi_loop.caught_end = false;
			shortest_delay = 0;

			if (!midi_loop_enabled ||
					(shortest_delay_not_found && midi_loop.loops_count >= 0 && midi_loop.loops_left < 1)) {
				midi_at_end = true; // Don't handle events anymore
				midi_current_position.wait += midi_post_song_wait_delay; // One second delay until stop
																		 // playing
				return true; // We have caugh end here!
			}

			if (midi_loop.temporary_broken) {
				midi_current_position = midi_track_begin_position;
				midi_loop.temporary_broken = false;
			} else if (midi_loop.loops_count < 0 || midi_loop.loops_left >= 1) {
				midi_current_position = midi_loop_begin_position;
				if (midi_loop.loops_count >= 1) {
					midi_loop.loops_left--;
				}
			}
		}

		return true; // Has events in queue
	}

	bool match_sysex(const char *p_data, size_t p_length, const unsigned char *p_sysex, size_t p_sysex_length) {
		if (p_length != p_sysex_length) {
			return false;
		}

		for (size_t i = 0; i < p_sysex_length; ++i) {
			if (i == 2) {
				// respond to all device IDs
				continue;
			} else if (p_data[i] != (char)p_sysex[i]) {
				return false;
			}
		}
		return true;
	}

	void handle_event(size_t p_track, const Sequencer::MidiEvent &p_evt, int32_t &p_status) {
		if (p_evt.type == MidiEvent::SYSEX || p_evt.type == MidiEvent::SYSEX2) // Ignore SysEx
		{
			const char *data = (const char *)p_evt.data.data();
			size_t length = (size_t)p_evt.data.size();
			if (match_sysex(data, length, GM_SYSTEM_ON, 6)) {
				midi_synth->standard = Synthesizer::Standard::GM;
			} else if (match_sysex(data, length, GM_SYSTEM_OFF, 6)) {
				midi_synth->standard = Synthesizer::Standard::GM; // Our default is GM, so set it here too
			} else if (match_sysex(data, length, GS_RESET, 11) || match_sysex(data, length, GS_SYSTEM_MODE_SET1, 11) ||
					match_sysex(data, length, GS_SYSTEM_MODE_SET2, 11)) {
				midi_synth->standard = Synthesizer::Standard::GS;
			} else if (match_sysex(data, length, XG_SYSTEM_ON, 9)) {
				midi_synth->standard = Synthesizer::Standard::XG;
			}
			return;
		}

		if (p_evt.type == MidiEvent::SPECIAL) {
			// Special event FF
			uint_fast16_t evtype = p_evt.sub_type;
			uint64_t length = (uint64_t)(p_evt.data.size());
			const char *data(length ? (const char *)(p_evt.data.data()) : "\0\0\0\0\0\0\0\0");

			if (evtype == MidiEvent::END_TRACK) // End Of Track
			{
				p_status = -1;
				return;
			}

			if (evtype == MidiEvent::TEMPO_CHANGE) // Tempo change
			{
				midi_tempo =
						midi_individual_tick_delta * FixedFraction(read_int_big_endian(p_evt.data.data(), p_evt.data.size()));
				return;
			}

			if (evtype == MidiEvent::MARKER) // Meta event
			{
				// Do nothing! :-P
				return;
			}

			// Turn on Loop handling when loop is enabled
			if (midi_loop_enabled && !midi_loop.invalid_loop) {
				if (evtype == MidiEvent::LOOP_START) // Special non-spec MIDI
													 // loop Start point
				{
					midi_loop.caught_start = true;
					return;
				}

				if (evtype == MidiEvent::LOOP_END) // Special non-spec MIDI loop End point
				{
					midi_loop.caught_end = true;
					return;
				}

				if (evtype == MidiEvent::LOOP_STACK_BEGIN) {
					if (midi_loop.skip_stack_start) {
						midi_loop.skip_stack_start = false;
						return;
					}

					char x = data[0];
					size_t slevel = (size_t)(midi_loop.stack_level + 1);
					while (slevel >= midi_loop.stack.size()) {
						LoopStackEntry e;
						e.loops = x;
						e.infinity = (x == 0);
						e.start = 0;
						e.end = 0;
						midi_loop.stack.push_back(e);
					}

					LoopStackEntry &s = midi_loop.stack[slevel];
					s.loops = (int)(x);
					s.infinity = (x == 0);
					midi_loop.caught_stack_start = true;
					return;
				}

				if (evtype == MidiEvent::LOOP_STACK_END) {
					midi_loop.caught_stack_end = true;
					return;
				}

				if (evtype == MidiEvent::LOOP_STACK_BREAK) {
					midi_loop.caught_stack_break = true;
					return;
				}
			}

			return;
		}

		if (p_evt.type == MidiEvent::SYS_COM_SONG_SELECT || p_evt.type == MidiEvent::SYS_COM_SONG_POSITION_POINTER) {
			return;
		}

		size_t mid_ch = p_evt.channel;
		if (mid_ch >= NUM_CHANNELS) {
			return;
		}

		p_status = p_evt.type;

		switch (p_evt.type) {
			case MidiEvent::NOTE_OFF: // Note off
			{
				if (channel_disabled[mid_ch]) {
					break; // Disabled channel
				}
				midi_synth->channels[mid_ch]->note_off(p_evt.data[0]);
				break;
			}

			case MidiEvent::NOTE_ON: // Note on
			{
				if (channel_disabled[mid_ch]) {
					break; // Disabled channel
				}
				midi_synth->channels[mid_ch]->note_on(p_evt.data[0], p_evt.data[1]);
				break;
			}

			case MidiEvent::NOTE_TOUCH: // Note touch
			{
				midi_synth->channels[mid_ch]->key_pressure(p_evt.data[0], p_evt.data[1]);
				break;
			}

			case MidiEvent::CONTROL_CHANGE: // Controller change
			{
				midi_synth->channels[mid_ch]->control_change(p_evt.data[0], p_evt.data[1]);
				break;
			}

			case MidiEvent::PATCH_CHANGE: // Patch change
			{
				const Channel::Bank midiBank = midi_synth->channels[mid_ch]->get_bank();
				uint16_t sfBank = 0;
				switch (midi_synth->standard) {
					case Synthesizer::Standard::GS:
						sfBank = midiBank.msb;
						break;
					case Synthesizer::Standard::XG:
						// assuming no one uses XG voices bank MSBs of which overlap normal voices' bank LSBs
						// e.g. SFX voice (MSB=64)
						sfBank = midiBank.msb == 127 ? PERCUSSION_BANK : midiBank.lsb;
						break;
					case Synthesizer::Standard::GM:
					default:
						break;
				}
				midi_synth->channels[mid_ch]->set_preset(
						midi_synth->find_preset(mid_ch == PERCUSSION_CHANNEL ? PERCUSSION_BANK : sfBank, p_evt.data[0]));
				break;
			}

			case MidiEvent::CHANNEL_TOUCH: // Channel after-touch
			{
				midi_synth->channels[mid_ch]->channel_pressure(p_evt.data[0]);
				break;
			}

			case MidiEvent::PITCH_WHEEL: // Wheel/pitch bend
			{
				midi_synth->channels[mid_ch]->pitch_bend(((uint16_t)p_evt.data[1] << 7) + (uint16_t)p_evt.data[0]);
				break;
			}

			default:
				break;
		} // switch
	}

public:
	enum class FileFormat {
		//! MIDI format
		MIDI,
		//! EA-MUS format
		RSXX
	};

	enum class LoopFormat {
		DEFAULT,
		RPG_MAKER = 1,
		EMIDI,
		HMI
	};

private:
	//! Music file format type. MIDI is default.
	FileFormat midi_format;
	//! SMF format identifier.
	unsigned midi_smf_format;
	//! Loop points format
	LoopFormat midi_loop_format;

	//! Current position
	Position midi_current_position;
	//! Track begin position
	Position midi_track_begin_position;
	//! Loop start point
	Position midi_loop_begin_position;

	//! Is looping enabled or not
	bool midi_loop_enabled;

	//! Full song length in seconds
	double midi_full_song_time_length;
	//! Delay after song playd before rejecting the output stream requests
	double midi_post_song_wait_delay;

	//! Global loop start time
	double midi_loop_start_time;
	//! Global loop end time
	double midi_loop_end_time;

	//! Pre-processed track data storage
	std::vector<std::list<MidiTrackRow>> midi_track_data;

	//! Time of one tick
	FixedFraction midi_individual_tick_delta;
	//! Current tempo
	FixedFraction midi_tempo;

	//! Tempo multiplier factor
	double midi_tempo_multiplier;
	//! Is song at end
	bool midi_at_end;

	//! Set the number of loops limit. Lesser than 0 - loop infinite
	int midi_loop_count;

	Synthesizer *midi_synth;
	struct LoopStackEntry {
		//! is infinite loop
		bool infinity = false;
		//! Count of loops left to break. <0 - infinite loop
		int loops = 0;
		//! Start position snapshot to return back
		Position start_position;
		//! Loop start tick
		uint64_t start = 0;
		//! Loop end tick
		uint64_t end = 0;
	};

	class LoopState {
	public:
		//! Loop start has reached
		bool caught_start;
		//! Loop end has reached, reset on handling
		bool caught_end;

		//! Loop start has reached
		bool caught_stack_start;
		//! Loop next has reached, reset on handling
		bool caught_stack_end;
		//! Loop break has reached, reset on handling
		bool caught_stack_break;
		//! Skip next stack loop start event handling
		bool skip_stack_start;

		//! Are loop points invalid?
		bool invalid_loop; /*Loop points are invalid (loopStart after loopEnd
							 or loopStart and loopEnd are on same place)*/

		//! Is look got temporarily broken because of post-end seek?
		bool temporary_broken;

		//! How much times the loop should start repeat? For example, if you
		//! want to loop song twice, set value 1
		int loops_count;

		//! how many loops left until finish the song
		int loops_left;

		//! Stack of nested loops
		std::vector<LoopStackEntry> stack;
		//! Current level on the loop stack (<0 - out of loop, 0++ - the index
		//! in the loop stack)
		int stack_level;

		void reset() {
			caught_start = false;
			caught_end = false;
			caught_stack_start = false;
			caught_stack_end = false;
			caught_stack_break = false;
			skip_stack_start = false;
			loops_left = loops_count;
		}

		void full_reset() {
			loops_count = -1;
			reset();
			invalid_loop = false;
			temporary_broken = false;
			stack.clear();
			stack_level = -1;
		}

		bool is_stack_end() {
			if (caught_stack_end && (stack_level >= 0) && (stack_level < (int)(stack.size()))) {
				const LoopStackEntry &e = stack[(size_t)(stack_level)];
				if (e.infinity || (!e.infinity && e.loops > 0)) {
					return true;
				}
			}
			return false;
		}

		void stack_up(int count = 1) {
			stack_level += count;
		}

		void stack_down(int count = 1) {
			stack_level -= count;
		}

		LoopStackEntry &get_current_stack() {
			if ((stack_level >= 0) && (stack_level < (int)(stack.size()))) {
				return stack[(size_t)(stack_level)];
			}
			if (stack.empty()) {
				LoopStackEntry d;
				d.loops = 0;
				d.infinity = 0;
				d.start = 0;
				d.end = 0;
				stack.push_back(d);
			}
			return stack[0];
		}
	};

	LoopState midi_loop;

	//! MIDI channel disable (exception for extra port-prefix-based channels)
	bool channel_disabled[16];
	class SequencerTime {
	public:
		//! Time buffer
		double time_rest;
		//! Sample rate
		uint32_t sample_rate;
		//! Size of one frame in bytes
		uint32_t frame_size;
		//! Minimum possible delay, granuality
		double minimum_delay;
		//! Last delay
		double delay;

		void init(uint32_t p_rate, uint32_t p_frame_size) {
			sample_rate = p_rate;
			frame_size = p_frame_size;
			reset();
		}

		void reset() {
			time_rest = 0.0;
			minimum_delay = 1.0 / (double)(sample_rate);
			delay = 0.0;
		}
	};

	SequencerTime midi_time;

public:
	Sequencer(uint32_t p_rate, uint32_t p_frame_size, Synthesizer *p_synth) :
			midi_format(FileFormat::MIDI), midi_smf_format(0), midi_loop_format(LoopFormat::DEFAULT), midi_loop_enabled(false), midi_full_song_time_length(0.0), midi_post_song_wait_delay(1.0), midi_loop_start_time(-1.0), midi_loop_end_time(-1.0), midi_tempo_multiplier(1.0), midi_at_end(false), midi_loop_count(-1), midi_synth(p_synth) {
		midi_loop.reset();
		midi_loop.invalid_loop = false;
		midi_time.init(p_rate, p_frame_size);
	}

	~Sequencer() {
	}

	int play_stream(uint8_t *p_stream, size_t p_length) {
		int count = 0;
		size_t samples = (size_t)(p_length / (size_t)(midi_time.frame_size));
		size_t left = samples;
		size_t period_size = 0;
		uint8_t *stream_pos = p_stream;

		while (left > 0) {
			const double left_delay = left / double(midi_time.sample_rate);
			const double max_delay = midi_time.time_rest < left_delay ? midi_time.time_rest : left_delay;
			if ((position_at_end()) && (midi_time.delay <= 0.0)) {
				break; // Stop to fetch samples at reaching the song end with
					   // disabled loop
			}

			midi_time.time_rest -= max_delay;
			period_size = (size_t)((double)(midi_time.sample_rate) * max_delay);

			if (p_stream) {
				size_t generate_size = period_size > left ? (size_t)(left) : (size_t)(period_size);
				float *buffer = (float *)stream_pos;
				for (size_t samp = 0; samp < generate_size * midi_time.frame_size / sizeof(float); samp += 2) {
					StereoValue sum{ 0.0f, 0.0f };
					for (Voice *voice : midi_synth->voices) {
						Synthesizer::Voice::State status = voice->get_status();
						if (status == Synthesizer::Voice::State::FINISHED ||
								status == Synthesizer::Voice::State::UNUSED) {
							continue;
						}
						voice->update();
						if (voice->get_status() == Synthesizer::Voice::State::FINISHED) {
							continue;
						}
						sum += voice->render();
					}
					sum = sum * midi_synth->volume;
					buffer[samp] = sum.left;
					buffer[samp + 1] = sum.right;
				}
				stream_pos += generate_size * midi_time.frame_size;
				count += generate_size;
				left -= generate_size;
				if (left > samples) { // shouldn't happen, but catch just in case
					left = samples;
				}
			}

			if (midi_time.time_rest <= 0.0) {
				midi_time.delay = tick(midi_time.delay, midi_time.minimum_delay);
				midi_time.time_rest += midi_time.delay;
			}
		}

		return count * (int)(midi_time.frame_size);
	}

	inline bool position_at_end() {
		return midi_at_end;
	}

	bool load_midi(FileAndMemReader *p_mfr) {
		midi_at_end = false;
		midi_loop.full_reset();
		midi_loop.caught_start = true;

		midi_format = FileFormat::MIDI;
		midi_smf_format = 0;

		char header_buf[MIDI_PARSE_HEADER_SIZE] = "";

		size_t fsize = p_mfr->read(header_buf, 1, MIDI_PARSE_HEADER_SIZE);
		if (fsize < MIDI_PARSE_HEADER_SIZE) {
			return false;
		}

		if (memcmp(header_buf, "MThd\0\0\0\6", 8) == 0) {
			p_mfr->seek(0, SEEK_SET);
			return parse_smf(p_mfr);
		}

		if (memcmp(header_buf, "RIFF", 4) == 0) {
			p_mfr->seek(0, SEEK_SET);
			return parse_rmi(p_mfr);
		}

		if (memcmp(header_buf, "GMF\x1", 4) == 0) {
			p_mfr->seek(0, SEEK_SET);
			return parse_gmf(p_mfr);
		}
		if (detect_rsxx(header_buf, p_mfr)) {
			p_mfr->seek(0, SEEK_SET);
			return parse_rsxx(p_mfr);
		}
		if (memcmp(header_buf, "MUS\x1A", 4) == 0) {
			// need to convert prior to parsing
			p_mfr->seek(0, SEEK_SET);
			FileAndMemReader *temp = nullptr;
			FileAndMemReader *converted = nullptr;
			if (!p_mfr->get_data()) // need to load to memory first
			{
				size_t raw_size = p_mfr->file_size();
				uint8_t *raw = (uint8_t *)malloc(raw_size);
				p_mfr->read(raw, 1, p_mfr->file_size());
				temp = new FileAndMemReader;
				temp->open_data(raw, raw_size);
			}
			if (temp) {
				converted = mus_to_midi(temp);
			} else {
				converted = mus_to_midi(p_mfr);
			}
			if (!converted) {
				if (temp) {
					temp->close(true);
					delete temp;
				}
				return false;
			}
			bool result = parse_smf(converted);
			if (temp) {
				temp->close(true);
				delete temp;
			}
			converted->close(true);
			delete converted;
			return result;
		}

		return false;
	}

	void full_reset() {
		midi_at_end = false;
		midi_loop.full_reset();
		midi_loop.caught_start = true;

		midi_format = FileFormat::MIDI;
		midi_smf_format = 0;
	}

	double tick(double p_s, double p_granularity) {
		p_s *= midi_tempo_multiplier;
		midi_current_position.wait -= p_s;
		midi_current_position.absolute_time_position += p_s;

		int anti_freeze_counter = 10000; // Limit 10000 loops to avoid freezing
		while ((midi_current_position.wait <= p_granularity * 0.5) && (anti_freeze_counter > 0)) {
			if (!process_events()) {
				break;
			}
			if (midi_current_position.wait <= 0.0) {
				anti_freeze_counter--;
			}
		}

		if (anti_freeze_counter <= 0) {
			midi_current_position.wait += 1.0; /* Add extra 1 second when over 10000 events
												with zero delay are been detected */
		}

		if (midi_current_position.wait < 0.0) { // Avoid negative delay value!
			return 0.0;
		}

		return midi_current_position.wait;
	}

	void rewind() {
		midi_current_position = midi_track_begin_position;
		midi_at_end = false;

		midi_loop.loops_count = midi_loop_count;
		midi_loop.reset();
		midi_loop.caught_start = true;
		midi_loop.temporary_broken = false;
		midi_time.reset();
	}

private:
	bool detect_rsxx(const char *p_head, FileAndMemReader *p_mfr) {
		char header_buf[7] = "";
		bool ret = false;

		// Try to identify RSXX format
		if (p_head[0] >= 0x5D) {
			p_mfr->seek(p_head[0] - 0x10, SEEK_SET);
			p_mfr->read(header_buf, 1, 6);
			if (memcmp(header_buf, "rsxx}u", 6) == 0) {
				ret = true;
			}
		}

		p_mfr->seek(0, SEEK_SET);
		return ret;
	}

	bool parse_rsxx(FileAndMemReader *p_mfr) {
		char header_buf[MIDI_PARSE_HEADER_SIZE] = "";
		size_t fsize = 0;
		size_t delta_ticks = 192, track_count = 1;
		std::vector<std::vector<uint8_t>> raw_track_data;

		fsize = p_mfr->read(header_buf, 1, MIDI_PARSE_HEADER_SIZE);
		if (fsize < MIDI_PARSE_HEADER_SIZE) {
			return false;
		}

		// Try to identify RSXX format
		char start = header_buf[0];
		if (start < 0x5D) {
			return false;
		} else {
			p_mfr->seek(header_buf[0] - 0x10, SEEK_SET);
			p_mfr->read(header_buf, 1, 6);
			if (memcmp(header_buf, "rsxx}u", 6) == 0) {
				midi_format = FileFormat::RSXX;
				p_mfr->seek(start, SEEK_SET);
				track_count = 1;
				delta_ticks = 60;
			} else {
				return false;
			}
		}

		raw_track_data.clear();
		raw_track_data.resize(track_count);
		midi_individual_tick_delta = FixedFraction(1, 1000000l * (uint64_t)(delta_ticks));
		midi_tempo = FixedFraction(1, (uint64_t)(delta_ticks));

		size_t total_gotten = 0;

		for (size_t tk = 0; tk < track_count; ++tk) {
			// Read track header
			size_t track_length;

			size_t pos = p_mfr->tell();
			p_mfr->seek(0, SEEK_END);
			track_length = p_mfr->tell() - pos;
			p_mfr->seek((long)(pos), SEEK_SET);

			// Read track data
			raw_track_data[tk].resize(track_length);
			fsize = p_mfr->read(&raw_track_data[tk][0], 1, track_length);
			if (fsize < track_length) {
				return false;
			}
			total_gotten += fsize;

			// Finalize raw track data with a zero
			raw_track_data[tk].push_back(0);
		}

		for (size_t tk = 0; tk < track_count; ++tk) {
			total_gotten += raw_track_data[tk].size();
		}

		if (total_gotten == 0) {
			return false;
		}

		// Build new MIDI events table
		if (!build_smf_track_data(raw_track_data)) {
			return false;
		}

		midi_smf_format = 0;
		midi_loop.stack_level = -1;

		return true;
	}

	bool parse_gmf(FileAndMemReader *p_mfr) {
		char header_buf[MIDI_PARSE_HEADER_SIZE] = "";
		size_t fsize = 0;
		size_t delta_ticks = 192, track_count = 1;
		std::vector<std::vector<uint8_t>> raw_track_data;

		fsize = p_mfr->read(header_buf, 1, MIDI_PARSE_HEADER_SIZE);
		if (fsize < MIDI_PARSE_HEADER_SIZE) {
			return false;
		}

		if (memcmp(header_buf, "GMF\x1", 4) != 0) {
			return false;
		}

		p_mfr->seek(7 - (long)(MIDI_PARSE_HEADER_SIZE), SEEK_CUR);

		raw_track_data.clear();
		raw_track_data.resize(track_count);
		midi_individual_tick_delta = FixedFraction(1, 1000000l * (uint64_t)(delta_ticks));
		midi_tempo = FixedFraction(1, (uint64_t)(delta_ticks) * 2);
		static const unsigned char EndTag[4] = { 0xFF, 0x2F, 0x00, 0x00 };
		size_t total_gotten = 0;

		for (size_t tk = 0; tk < track_count; ++tk) {
			// Read track header
			size_t track_length;
			size_t pos = p_mfr->tell();
			p_mfr->seek(0, SEEK_END);
			track_length = p_mfr->tell() - pos;
			p_mfr->seek((long)(pos), SEEK_SET);

			// Read track data
			raw_track_data[tk].resize(track_length);
			fsize = p_mfr->read(&raw_track_data[tk][0], 1, track_length);
			if (fsize < track_length) {
				return false;
			}
			total_gotten += fsize;
			// Note: GMF does include the track end tag.
			raw_track_data[tk].push_back(EndTag[0]);
			raw_track_data[tk].push_back(EndTag[1]);
			raw_track_data[tk].push_back(EndTag[2]);
			raw_track_data[tk].push_back(EndTag[3]);
		}

		for (size_t tk = 0; tk < track_count; ++tk) {
			total_gotten += raw_track_data[tk].size();
		}

		if (total_gotten == 0) {
			return false;
		}

		// Build new MIDI events table
		if (!build_smf_track_data(raw_track_data)) {
			return false;
		}

		return true;
	}

	bool parse_smf(FileAndMemReader *p_mfr) {
		char header_buf[MIDI_PARSE_HEADER_SIZE] = "";
		size_t fsize = 0;
		size_t delta_ticks = 192, track_count = 1;
		unsigned smf_format = 0;
		std::vector<std::vector<uint8_t>> raw_track_data;

		fsize = p_mfr->read(header_buf, 1, MIDI_PARSE_HEADER_SIZE);
		if (fsize < MIDI_PARSE_HEADER_SIZE) {
			return false;
		}

		if (memcmp(header_buf, "MThd\0\0\0\6", 8) != 0) {
			return false;
		}

		smf_format = (unsigned)(read_int_big_endian(header_buf + 8, 2));
		track_count = (size_t)(read_int_big_endian(header_buf + 10, 2));
		delta_ticks = (size_t)(read_int_big_endian(header_buf + 12, 2));

		if (smf_format > 2) {
			smf_format = 1;
		}

		raw_track_data.clear();
		raw_track_data.resize(track_count);
		midi_individual_tick_delta = FixedFraction(1, 1000000l * (uint64_t)(delta_ticks));
		midi_tempo = FixedFraction(1, (uint64_t)(delta_ticks) * 2);

		size_t total_gotten = 0;

		for (size_t tk = 0; tk < track_count; ++tk) {
			// Read track header
			size_t track_length;

			fsize = p_mfr->read(header_buf, 1, 8);
			if ((fsize < 8) || (memcmp(header_buf, TRACK_MAGIC, 4) != 0)) {
				return false;
			}
			track_length = (size_t)read_int_big_endian(header_buf + 4, 4);

			// Read track data
			raw_track_data[tk].resize(track_length);
			fsize = p_mfr->read(&raw_track_data[tk][0], 1, track_length);
			if (fsize < track_length) {
				return false;
			}

			total_gotten += fsize;
		}

		for (size_t tk = 0; tk < track_count; ++tk) {
			total_gotten += raw_track_data[tk].size();
		}

		if (total_gotten == 0) {
			return false;
		}

		// Build new MIDI events table
		if (!build_smf_track_data(raw_track_data)) {
			return false;
		}

		midi_smf_format = smf_format;
		midi_loop.stack_level = -1;

		return true;
	}

	bool parse_rmi(FileAndMemReader *p_mfr) {
		char header_buf[MIDI_PARSE_HEADER_SIZE] = "";

		size_t fsize = p_mfr->read(header_buf, 1, MIDI_PARSE_HEADER_SIZE);
		if (fsize < MIDI_PARSE_HEADER_SIZE) {
			return false;
		}

		if (memcmp(header_buf, "RIFF", 4) != 0) {
			return false;
		}

		midi_format = FileFormat::MIDI;

		p_mfr->seek(6l, SEEK_CUR);
		return parse_smf(p_mfr);
	}
};

Synthesizer::Synthesizer(float p_rate, size_t p_voices) :
		standard(Standard::GM), volume(1.0f) {
	initialize_conversion_tables();

	voices.reserve(p_voices);
	for (size_t i = 0; i < p_voices; ++i) {
		voices.push_back(new Voice());
	}

	channels.reserve(NUM_CHANNELS);
	for (size_t i = 0; i < NUM_CHANNELS; ++i) {
		channels.push_back(new Channel(i, p_rate, &voices));
	}

	soundfont = nullptr;
	sequencer = new Sequencer(p_rate, 2 * sizeof(float), this);
	no_drums = false;
	no_piano = false;
}

Synthesizer::~Synthesizer() {
	sequencer->full_reset();
	delete soundfont;
	delete sequencer;
	for (Voice *voice : voices) {
		delete voice;
	}
	for (Channel *channel : channels) {
		delete channel;
	}
}

bool Synthesizer::load_soundfont(const char *p_filename) {
	if (soundfont) {
		delete soundfont;
	}
	FileAndMemReader *p_font = new FileAndMemReader;
	p_font->open_file(p_filename);
	if (!p_font->is_valid()) {
		delete p_font;
		return false;
	}
	load_error = false;
#ifdef TINYPRIMESYNTH_FLAC_SUPPORT
	char flac_check[4];
	p_font->read(&flac_check, 1, 4);
	p_font->seek(0, SEEK_SET);
	if (!memcmp(flac_check, FLAC_MAGIC, 4)) {
		FLACBitStream decoder(p_font);
		std::vector<uint8_t> decoded = decode_sf2_flac(decoder);
		delete p_font;
		if (decoded.empty()) {
			return false;
		} else {
			FileAndMemReader *font = new FileAndMemReader;
			font->open_data(decoded.data(), decoded.size());
			soundfont = new SoundFont(font, this);
			decoded.clear();
			delete font;
			return !load_error;
		}
	} else {
		soundfont = new SoundFont(p_font, this);
		delete p_font;
		return !load_error;
	}
#else
	soundfont = new SoundFont(p_font, this);
	delete p_font;
	return !load_error;
#endif
}

bool Synthesizer::load_soundfont(const uint8_t *p_data, size_t p_length) {
	if (soundfont) {
		delete soundfont;
	}
	FileAndMemReader *p_font = new FileAndMemReader;
	p_font->open_data(p_data, p_length);
	if (!p_font->is_valid()) {
		delete p_font;
		return false;
	}
	load_error = false;
#ifdef TINYPRIMESYNTH_FLAC_SUPPORT
	char flac_check[4];
	p_font->read(&flac_check, 1, 4);
	p_font->seek(0, SEEK_SET);
	if (!memcmp(flac_check, FLAC_MAGIC, 4)) {
		FLACBitStream decoder(p_font);
		std::vector<uint8_t> decoded = decode_sf2_flac(decoder);
		delete p_font;
		if (decoded.empty()) {
			return false;
		} else {
			FileAndMemReader *font = new FileAndMemReader;
			font->open_data(decoded.data(), decoded.size());
			soundfont = new SoundFont(font, this);
			decoded.clear();
			delete font;
			return !load_error;
		}
	} else {
		soundfont = new SoundFont(p_font, this);
		delete p_font;
		return !load_error;
	}
#else
	soundfont = new SoundFont(p_font, this);
	delete p_font;
	return !load_error;
#endif
}

bool Synthesizer::load_song(const char *p_filename) {
	FileAndMemReader *p_song = new FileAndMemReader;
	p_song->open_file(p_filename);
	if (!p_song->is_valid()) {
		delete p_song;
		return false;
	}
	bool result = sequencer->load_midi(p_song);
	delete p_song;
	return result;
}

bool Synthesizer::load_song(const uint8_t *p_data, size_t p_length) {
	FileAndMemReader *p_song = new FileAndMemReader;
	p_song->open_data(p_data, p_length);
	if (!p_song->is_valid()) {
		delete p_song;
		return false;
	}
	bool result = sequencer->load_midi(p_song);
	delete p_song;
	return result;
}

bool Synthesizer::get_load_error(void) const {
	return load_error;
}

void Synthesizer::set_load_error(bool p_error) {
	load_error = p_error;
}

void Synthesizer::set_volume(float p_volume) {
	volume = fmax(0.0f, p_volume);
}

int Synthesizer::play_stream(uint8_t *p_stream, size_t p_length) {
	return sequencer->play_stream(p_stream, p_length);
}

const Synthesizer::Preset *Synthesizer::find_preset(uint16_t p_bank, uint16_t p_id) {
	for (const Synthesizer::Preset *preset : soundfont->get_preset_pointers()) {
		if (preset->bank == p_bank && preset->preset_id == p_id) {
			return preset;
		}
	}

	// fallback
	if (p_bank == PERCUSSION_BANK) {
		if (p_id != 0) {
			// fall back to GM percussion
			return find_preset(p_bank, 0);
		} else {
			no_drums = true;
			return nullptr;
		}
	} else if (p_bank != 0) {
		// fall back to GM bank
		return find_preset(0, p_id);
	} else if (p_id != 0) {
		// preset not found even in GM bank, fall back to Piano
		return find_preset(0, 0);
	} else {
		// Piano not found, there is no more fallback
		no_piano = true;
		return nullptr;
	}
}

void Synthesizer::pause() {
	for (size_t chan = 0; chan < NUM_CHANNELS; chan++) {
		channels[chan]->control_change(123, 0); // AllNotesOff
	}
}

void Synthesizer::stop() {
	for (size_t chan = 0; chan < NUM_CHANNELS; chan++) {
		channels[chan]->control_change(120, 0); // AllSoundOff
	}
}

void Synthesizer::reset() {
	for (size_t chan = 0; chan < NUM_CHANNELS; chan++) {
		channels[chan]->control_change(120, 0); // AllSoundOff
		channels[chan]->control_change(64, 0); // Sustain (release)
		channels[chan]->control_change(121, 0); // ResetAllControllers
	}
	sequencer->full_reset();
}

bool Synthesizer::at_end() const {
	return sequencer->position_at_end();
}

void Synthesizer::rewind() {
	sequencer->rewind();
}

} // namespace tinyprimesynth

#endif // TINYPRIMESYNTH_IMPLEMENTATION
