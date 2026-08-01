#include <stdint.h>

typedef struct {
    char     chunkId[4];
    uint32_t chunkSize;
    char     format[4];
    char     subchunk1Id[4];
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char     subchunk2Id[4];
    uint32_t subchunk2Size;
} __attribute__((packed)) wav_header_t;

int play_wav_file(const char* filename);
