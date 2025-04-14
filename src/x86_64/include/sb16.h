#ifndef INCLUDE_SB16_H_
#define INCLUDE_SB16_H_

#include <stddef.h>
#include <stdint.h>

#define SB_MIXER      0x224 // DSP Mixer port
#define SB_MIXER_DATA 0x225 // DSP Mixer data port
#define SB_RESET      0x226 // DSP Reset
#define SB_READ       0x22A // DSP Read
#define SB_WRITE      0x22C // DSP Write
#define SB_STATE      0x22E // DSP Read Status
#define SB_INTR16     0x22F // DSP 16-bit Interrupt Acknowledge

#define DSP_CMD_SET_TIME_CONST  0x40 // Set time constant
#define DSP_CMD_SET_SAMPLE_RATE 0x41 // Set Output Sample Rate
#define DSP_CMD_SPEAKER_ON      0xD1 // Turn speaker on
#define DSP_CMD_SPEAKER_OFF     0xD3 // Turn speaker off
#define DSP_CMD_STOP_8BIT       0xD0 // Stop playing 8 bit channel
#define DSP_CMD_RESUME_8BIT     0xD4 // Resume playback of 8 bit channel
#define DSP_CMD_STOP_16BIT      0xD5 // Stop playing 16 bit channel
#define DSP_CMD_RESUME_16BIT    0xD6 // Resume playback of 16 bit channel
#define DSP_CMD_GET_VERSION     0xE1 // Get DSP version

#define MIXER_REG_MASTER_VOL 0x22 // Master volume
#define MIXER_REG_IRQ        0x80 // Set IRQ

#define DMA_BUF_SIZE 0x8000 // The buffer size of the DMA

/* Initialize sb16 sound card */
void sb16_init(void);

/* Reset sb16 sound card */
void sb_reset(void);

/* Set the default volume of the sb16 sound card */
void sb16_set_volume(uint8_t left, uint8_t right);

/* Set the default baud rate of the sb16 sound card */
void sb16_set_sample_rate(uint16_t rate);

/* Send data packets to the sb16 sound card */
void sb16_send_data(const uint8_t *data, size_t size);

/* Play PCM audio data */
void sb16_play(const uint8_t *data, size_t size);

#endif // INCLUDE_SB16_H_
