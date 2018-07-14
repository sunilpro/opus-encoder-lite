#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <opus.h>
#include <stdio.h>
/*The frame size is hardcoded for this sample code but it doesn't have to be*/
#define FRAME_SIZE (640)
#define SAMPLE_RATE 16000
#define CHANNELS 1
#define APPLICATION OPUS_APPLICATION_VOIP
#define BITRATE 12000
#define MAX_FRAME_SIZE 640
#define MAX_PACKET_SIZE (80)

const char *opus_strerror(int error)
{
   static const char * const error_strings[8] = {
      "success",
      "invalid argument",
      "buffer too small",
      "internal error",
      "corrupted stream",
      "request not implemented",
      "invalid state",
      "memory allocation failed"
   };
   if (error > 0 || error < -7)
      return "unknown error";
   else
      return error_strings[-error];
}

static opus_int16 my_in[FRAME_SIZE*CHANNELS];
int main(int argc, char **argv)
{
    char *inFile;
    FILE *fin;
    char *outFile;
    FILE *fout;
    FILE *fbits;
    opus_int16 out[MAX_FRAME_SIZE*CHANNELS];
    unsigned char cbits[MAX_PACKET_SIZE];
    /*Holds the state of the encoder and decoder */
    OpusEncoder *encoder;
    int err;
    if (argc != 3)
    {
        fprintf(stderr, "usage: trivial_example input.pcm output.pcm\n");
        fprintf(stderr, "input and output are 16-bit little-endian raw files\n");
        return EXIT_FAILURE;
    }
    /*Create a new encoder state */
    encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, APPLICATION, &err);
    if (err<0)
    {
        fprintf(stderr, "failed to create an encoder: %s\n", opus_strerror(err));
        return EXIT_FAILURE;
    }
    /* Set the desired bit-rate. You can also set other parameters if needed.
     The Opus library is designed to have good defaults, so only set
     parameters you know you need. Doing otherwise is likely to result
     in worse quality, but better. */
    err = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(BITRATE));
    if (err<0)
    {
        fprintf(stderr, "failed to set bitrate: %s\n", opus_strerror(err));
        return EXIT_FAILURE;
    }
    err = opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(0));
    if (err<0) {
        fprintf(stderr, "failed to OPUS_SET_COMPLEXITY: %s\n", opus_strerror(err));
    }
    inFile = argv[1];
    fin = fopen(inFile, "r");
    if (fin==NULL) {
        fprintf(stderr, "failed to open input file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    fbits = fopen(argv[2], "wb");
    if (fbits==NULL) {
        fprintf(stderr, "failed to open output file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    int total = 0;
    while (1)
    {
        int i;
        int frame_size;
        int8_t nbBytes;
        /* Read a 16 bits/sample audio frame. */
        fread(my_in, sizeof(short)*CHANNELS, FRAME_SIZE, fin);
        if (feof(fin))
            break;
        static opus_int16 t_in[FRAME_SIZE*CHANNELS];
        nbBytes = opus_encode(encoder, my_in, FRAME_SIZE, cbits, 80);
        if (nbBytes<0) {
            fprintf(stderr, "encode failed: %s\n", opus_strerror(nbBytes));
            return EXIT_FAILURE;
        }
        fwrite(&nbBytes, 1, 1, fbits);
        fwrite(cbits, 1, nbBytes, fbits);
        total += nbBytes;
    }
    printf("Total=%d\n", total);
    /*Destroy the encoder state*/
    //opus_encoder_destroy(encoder);
    fclose(fin);
    return EXIT_SUCCESS;
}
