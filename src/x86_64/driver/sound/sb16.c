#include "sb16.h"
#include "dma.h"
#include "frame.h"
#include "hhdm.h"
#include "io.h"
#include "isr.h"
#include "klog.h"
#include "kprint.h"
#include "page.h"
#include "timer.h"

static volatile bool sig     = false;
uint64_t             buf_phy = 0;

/* sb16 interrupt handler */
__attribute__((interrupt)) static void sb16_handler(interrupt_frame_t *frame) {
    close_interrupt;
    io_in8(SB_INTR16);
    send_eoi();
    sig = true;
    open_interrupt;
}

/* Send data to port sb16 */
static void sb_out(uint8_t value) {
    while (io_in8(SB_WRITE) & 0x80)
        ;
    io_out8(SB_WRITE, value);
}

/* Initialize sb16 sound card */
void sb16_init(void) {
    if(!sb_reset()) return;

    buf_phy = alloc_frames(PADDING_UP(DMA_BUF_SIZE, PAGE_SIZE) / PAGE_SIZE);

    io_out8(SB_MIXER, 0x80);
    io_out8(SB_MIXER_DATA, 0x02);

    sb_out(DSP_CMD_SPEAKER_ON);

    register_interrupt_handler(sb16, sb16_handler, 0, 0x8e);
}

/* Reset sb16 sound card */
bool sb_reset(void) {
    io_out8(SB_RESET, 1);
    nsleep(5000);
    io_out8(SB_RESET, 0);

    if (io_in8(SB_STATE) == 0x80) {
        kinfo("SB16: Reset OK, state = 0x%x", io_in8(SB_READ));
        return true;
    } else {
        return false;
    }
}

/* Set the default volume of the sb16 sound card */
void sb16_set_volume(uint8_t left, uint8_t right) {
    if (left > 15) left = 15;
    if (right > 15) right = 15;

    io_out8(SB_MIXER, 0x22);
    io_out8(SB_MIXER_DATA, (left << 4) | (right & 0x0F));
}

/* Set the default baud rate of the sb16 sound card */
void sb16_set_sample_rate(uint16_t rate) {
    sb_out(0x41);
    sb_out((rate >> 8) & 0xff);
    sb_out(rate & 0xff);
}

/* Send data packets to the sb16 sound card */
void sb16_send_data(const uint8_t *data, size_t size) {
    uint16_t count = (size / 2 - 1);

    if (size > DMA_BUF_SIZE) size = DMA_BUF_SIZE;

    for (size_t i = 0; i < size; i++)
        ((uint8_t *)phys_to_virt(buf_phy))[i] = data[i];
    dma_send(5, (uint32_t *)buf_phy, size);

    sb_out(0xB0);
    sb_out(0x10);
    sb_out(count & 0xff);
    sb_out((count >> 8) & 0xff);
}

/* Play PCM audio data */
void sb16_play(const uint8_t *data, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        size_t chunk = size - offset;
        if (chunk > DMA_BUF_SIZE) chunk = DMA_BUF_SIZE;
        sb16_send_data(data + offset, chunk);
        waitif(sig == false);
        sig     = false;
        offset += chunk;
    }
}
