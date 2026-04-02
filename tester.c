#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/opus.h"

#define FRAME_SIZE 960
#define SAMPLE_RATE 48000
#define CHANNELS 2
#define MAX_PACKET_SIZE 3825

void write_wav_header(FILE *f, int data_size, int channels) {
    unsigned char header[44];
    int total_size = data_size + 36;
    int byte_rate = SAMPLE_RATE * channels * 2;
    int block_align = channels * 2;

    memcpy(&header[0], "RIFF", 4);
    *((int*)&header[4]) = total_size;
    memcpy(&header[8], "WAVE", 4);
    memcpy(&header[12], "fmt ", 4);
    *((int*)&header[16]) = 16; // PCM
    *((short*)&header[20]) = 1; // PCM
    *((short*)&header[22]) = channels;
    *((int*)&header[24]) = SAMPLE_RATE;
    *((int*)&header[28]) = byte_rate;
    *((short*)&header[32]) = block_align;
    *((short*)&header[34]) = 16; // bits per sample
    memcpy(&header[36], "data", 4);
    *((int*)&header[40]) = data_size;

    fwrite(header, 1, 44, f);
}

int main(int argc, char **argv) {
    int err;
    OpusEncoder *enc;
    OpusDecoder *dec;
    FILE *fin, *fout, *foutL, *foutR;
    short in[FRAME_SIZE * CHANNELS];
    short out[FRAME_SIZE * CHANNELS];
    short outL[FRAME_SIZE];
    short outR[FRAME_SIZE];
    unsigned char packet[MAX_PACKET_SIZE];
    int len;

    fin = fopen("test_48000_stereo_16.raw", "rb");
    if (!fin) {
        perror("fopen in");
        return 1;
    }
    
    fout = fopen("output.wav", "wb");
    foutL = fopen("outputL.wav", "wb");
    foutR = fopen("outputR.wav", "wb");
    
    if (!fout || !foutL || !foutR) {
        perror("fopen out");
        return 1;
    }

    // Skip headers for now, we'll seek back and write them later
    fseek(fout, 44, SEEK_SET);
    fseek(foutL, 44, SEEK_SET);
    fseek(foutR, 44, SEEK_SET);

    enc = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_AUDIO, &err);
    if (err != OPUS_OK) {
        fprintf(stderr, "Cannot create encoder: %s\n", opus_strerror(err));
        return 1;
    }

    dec = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);
    if (err != OPUS_OK) {
        fprintf(stderr, "Cannot create decoder: %s\n", opus_strerror(err));
        return 1;
    }

    opus_encoder_ctl(enc, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_FULLBAND));
    opus_encoder_ctl(enc, OPUS_SET_VBR(1));
    opus_encoder_ctl(enc, OPUS_SET_BITRATE(256000));
    opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(10));
    opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
	// opus_encoder_ctl(enc, OPUS_SET_QEXT(1));

    printf("Starting encoding loop...\n");
    int frame_count = 0;
    while (fread(in, sizeof(short), FRAME_SIZE * CHANNELS, fin) == FRAME_SIZE * CHANNELS) {
        len = opus_encode(enc, in, FRAME_SIZE, packet, MAX_PACKET_SIZE);
        if (len < 0) {
            fprintf(stderr, "Encode failed: %s\n", opus_strerror(len));
            break;
        }

        int decoded = opus_decode(dec, packet, len, out, FRAME_SIZE, 0);
        if (decoded < 0) {
            fprintf(stderr, "Decode failed: %s\n", opus_strerror(decoded));
            break;
        }

        for (int i = 0; i < FRAME_SIZE; i++) {
            outL[i] = out[i * 2];
            outR[i] = out[i * 2 + 1];
        }

        fwrite(out, sizeof(short), FRAME_SIZE * CHANNELS, fout);
        fwrite(outL, sizeof(short), FRAME_SIZE, foutL);
        fwrite(outR, sizeof(short), FRAME_SIZE, foutR);
        
        frame_count++;
        if (frame_count % 100 == 0) printf("Processed %d frames\n", frame_count);
    }

    int total_samples = frame_count * FRAME_SIZE;
    
    fseek(fout, 0, SEEK_SET);
    write_wav_header(fout, total_samples * CHANNELS * sizeof(short), CHANNELS);
    
    fseek(foutL, 0, SEEK_SET);
    write_wav_header(foutL, total_samples * sizeof(short), 1);
    
    fseek(foutR, 0, SEEK_SET);
    write_wav_header(foutR, total_samples * sizeof(short), 1);

    printf("Done. Processed %d frames.\n", frame_count);

    opus_encoder_destroy(enc);
    opus_decoder_destroy(dec);
    fclose(fin);
    fclose(fout);
    fclose(foutL);
    fclose(foutR);

    return 0;
}
