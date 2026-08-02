#ifndef AC97_H
#define AC97_H
#include <stdint.h>
int prep_play(void);
int play_wav_file(const char* filename);
void ac97_play_drive1(void);
int ac97_init(void);
int ac97_play_test_tone(void);
int ac97_play_pcm(void* buffer, uint32_t length_bytes);
#endif
