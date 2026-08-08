#ifndef AC97_H
#define AC97_H
#include <stdint.h>
int prep_play(void);
void ac97_stop(void);
void ac97_pause(void);
void ac97_resume(void);
void ac97_set_volume(uint8_t vol_atten);
void ac97_set_mute(int mute);
int play_wav_file(const char* filename);
void ac97_play_drive1(void);
int ac97_init(void);
int ac97_play_test_tone(void);
int ac97_play_pcm(void* buffer, uint32_t length_bytes);
#endif
