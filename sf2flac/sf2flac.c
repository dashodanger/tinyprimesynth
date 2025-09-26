/* SPDX-License-Identifier: 0BSD */

// Adapted from the "Even Simpler Encoder" example with the following license:

/*
Copyright (C) 2024 John Regan <john@jrjrtech.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
*/

#define TFLAC_DISABLE_COUNTERS
#define TFLAC_IMPLEMENTATION
#include "tflac.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define FRAME_SIZE   4096
#define SAMPLERATE  44100
#define BITDEPTH       16
#define CHANNELS        1

static tflac_u16 unpack_u16le(const tflac_u8* d) {
    return (((tflac_u16)d[0])    ) |
           (((tflac_u16)d[1])<< 8);
}

static tflac_s16 unpack_s16le(const tflac_u8* d) {
    return (tflac_s16)unpack_u16le(d);
}

void
repack_samples(tflac_s16 *s, tflac_u32 channels, tflac_u32 num) {
    tflac_u32 i = 0;
    while(i < (channels*num)) {
        s[i] = unpack_s16le( (tflac_u8*) (&s[i]) );
        i++;
    }
}

typedef tflac_s16 sample;

int main(int argc, const char *argv[]) {
    tflac_u8 *buffer = NULL;
    tflac_u32 bufferlen = 0;
    tflac_u32 bufferused = 0;
    FILE *input = NULL;
    FILE *output = NULL;
    tflac_u32 frames = 0;
    sample *samples = NULL;
    void *tflac_mem = NULL;
    tflac t;

    if(argc < 2) {
        printf("Usage: %s /path/to/sf2\n",argv[0]);
        return 1;
    }

    if(tflac_size_memory(FRAME_SIZE) != TFLAC_SIZE_MEMORY(FRAME_SIZE)) {
        printf("Error with needed memory size: %u != %lu\n",
          tflac_size_memory(FRAME_SIZE),TFLAC_SIZE_MEMORY(FRAME_SIZE));
        return 1;
    }

    if(tflac_size_frame(FRAME_SIZE,CHANNELS,BITDEPTH) != TFLAC_SIZE_FRAME(FRAME_SIZE,CHANNELS,BITDEPTH)) {
        printf("Error with needed frame size: %u != %lu\n",
          tflac_size_frame(FRAME_SIZE,CHANNELS,BITDEPTH),TFLAC_SIZE_FRAME(FRAME_SIZE,CHANNELS,BITDEPTH));
        return 1;
    }

    tflac_detect_cpu();
    tflac_init(&t);

    t.samplerate = SAMPLERATE;
    t.channels = CHANNELS;
    t.bitdepth = BITDEPTH;
    t.blocksize = FRAME_SIZE;
    t.enable_md5 = 0;

    if (argv[1] == NULL) return 1;

    input = fopen(argv[1],"rb");

    if(input == NULL) return 1;

    // At least check for the RIFF header
    char riff_check[4];

    if (fread(&riff_check, 4, 1, input) != 1)
    {
        printf("Unable to read header!\n");
        fclose(input);
        return 1;
    }
    if (memcmp(riff_check, "RIFF", 4) != 0)
    {
        printf("Header invalid! (Is this an SF2 file?)\n");
        fclose(input);
        return 1;
    }    

    fseek(input, 0, SEEK_SET);

    // Just append "flac" to whatever extension it had (probably sf2)
    size_t arg_length = strlen(argv[1]);
    char *out_name = (char *)calloc(arg_length + 5, sizeof(char));
    if (out_name == NULL) return 1;
    strncpy(out_name, argv[1], arg_length);
    out_name[arg_length] = 'f';
    out_name[arg_length+1] = 'l';
    out_name[arg_length+2] = 'a';
    out_name[arg_length+3] = 'c';

    output = fopen(out_name,"wb");
    if(output == NULL) {
        printf("Unable to open output file %s!\n", out_name);
        free(out_name);
        fclose(input);
        return 1;
    }
    else
        free(out_name);

    tflac_mem = malloc(tflac_size_memory(t.blocksize));
    if(tflac_mem == NULL) abort();

    tflac_set_constant_subframe(&t, 1);
    tflac_set_fixed_subframe(&t, 1);

    if(tflac_validate(&t, tflac_mem, tflac_size_memory(t.blocksize)) != 0) abort();

    bufferlen = tflac_size_frame(FRAME_SIZE,CHANNELS,BITDEPTH);
    buffer = malloc(bufferlen);
    if(buffer == NULL) abort();

    samples = (sample *)malloc(sizeof(sample) * CHANNELS * FRAME_SIZE);
    if(!samples) abort();

    fwrite("fLaC",1,4,output);

    tflac_encode_streaminfo(&t, 1, buffer, bufferlen, &bufferused);
    fwrite(buffer,1,bufferused,output);

    while((frames = fread(samples,sizeof(sample) * CHANNELS, FRAME_SIZE, input)) > 0) {
        repack_samples(samples, CHANNELS, frames);

        if(tflac_encode_s16i(&t, frames, samples, buffer, bufferlen, &bufferused) != 0) abort();
        fwrite(buffer,1,bufferused,output);
    }

    tflac_finalize(&t);

    fseek(output,4,SEEK_SET);
    tflac_encode_streaminfo(&t, 1, buffer, bufferlen, &bufferused);
    fwrite(buffer,1,bufferused,output);

    fclose(input);
    fclose(output);
    free(tflac_mem);
    free(samples);
    free(buffer);

    return 0;
}

