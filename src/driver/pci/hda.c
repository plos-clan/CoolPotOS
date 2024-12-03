#include "hda.h"
#include "klog.h"
#include "pci.h"
#include "krlibc.h"
#include "timer.h"
#include "kmalloc.h"
#include "isr.h"

static uint32_t hda_base;
static uint32_t hda_output_base;
static uint32_t *hda_output_buffer;
static uint32_t *corb = NULL;
static uint32_t *rirb = NULL;
static uint32_t corb_entry_count = 0,corb_write_pointer = 0;
static uint32_t rirb_entry_count = 0,rirb_read_pointer = 0;
static uint32_t send_verb_method;
static uint32_t hda_codec_number = 0;
static uint32_t  hda_afg_pcm_format_cap    = 0;
static uint32_t  hda_afg_stream_format_cap = 0;
static uint32_t  hda_afg_amp_cap           = 0;
static uint32_t hda_pin_output_node       = 0;
static uint32_t hda_pin_output_amp_cap    = 0;
static uint32_t hda_pin_pcm_format_cap    = 0;
static uint32_t hda_pin_stream_format_cap = 0;

static bool hda_stopping = false;

void hda_stop() {
    mem_set8(hda_output_base + 0x0, 0);
}

void hda_continue() {
    mem_set8(hda_output_base + 0x0, 0b110);
}

uint32_t hda_get_bytes_sent() {
    return mem_get32(hda_output_base + 0x4);
}

uint8_t hda_node_type(uint32_t codec, uint32_t node) {
    return (hda_verb(codec, node, 0xf00, 0x9) >> 20) & 0xf;
}

uint8_t hda_pin_complex_type(uint32_t codec, uint32_t node) {
    return (hda_verb(codec, node, 0xF1C, 0) >> 20) & 0xf;
}

uint16_t hda_get_connection_list_entry(uint32_t codec, uint32_t node, uint32_t index) {
    uint32_t  cll        = hda_verb(codec, node, 0xf00, 0xe);
    bool long_entry = (cll & 0x80) != 0;
    printk("cll: %08x and long_entry: %d\n", cll, long_entry);
    if (!long_entry) {
        return (hda_verb(codec, node, 0xf02, index / 4 * 4) >> ((index % 4) * 8)) & 0xff;
    } else {
        return (hda_verb(codec, node, 0xf02, index / 2 * 2) >> ((index % 2) * 16)) & 0xffff;
    }
    __builtin_unreachable();
    return 0;
}

void hda_pin_set_output_volume(uint32_t codec, uint32_t node, uint32_t cap, uint32_t volume) {
    uint32_t val  = (1 << 12) | (1 << 13);
    val     |= 0x8000;
    if (volume == 0 && cap & 0x80000000) {
        val |= (1 << 7);
    } else {
        val |= ((cap >> 8) & 0x7F) * volume / 100;
    }
    hda_verb(codec, node, 0x3, val);
}

void hda_init_audio_output(uint32_t codec, uint32_t node) {
    hda_verb(codec, node, 0x705, 0x0);
    hda_verb(codec, node, 0x708, 0x0);
    hda_verb(codec, node, 0x703, 0x0);
    hda_verb(codec, node, 0x706, 0x10);

    hda_pin_output_node = node;

    uint32_t amp_cap = hda_verb(codec, node, 0xf00, 0x12);
    hda_pin_set_output_volume(codec, node, amp_cap, 100);
    if (amp_cap != 0) {
        hda_pin_output_node    = node;
        hda_pin_output_amp_cap = amp_cap;
    }
    uint32_t pcm_format_cap = hda_verb(codec, node, 0xf00, 0xA);
    printk("pcm format cap: %08x\n", pcm_format_cap);
    if (pcm_format_cap != 0) {
        hda_pin_pcm_format_cap = pcm_format_cap;
    } else {
        hda_pin_pcm_format_cap = hda_afg_pcm_format_cap;
        printk("using afg pcm format cap: %08x\n", hda_afg_pcm_format_cap);
    }

    uint32_t stream_format_cap = hda_verb(codec, node, 0xf00, 0xB);
    if (stream_format_cap != 0) {
        hda_pin_stream_format_cap = stream_format_cap;
    } else {
        hda_pin_stream_format_cap = hda_afg_stream_format_cap;
    }
    if (hda_pin_output_amp_cap == 0) {
        hda_pin_output_node    = node;
        hda_pin_output_amp_cap = hda_afg_amp_cap;
    }
    printk("successfully initialized audio output node %d S\n", hda_pin_output_node);
}

void hda_init_audio_mixer(uint32_t codec, uint32_t node) {
    hda_verb(codec, node, 0x705, 0x0);
    hda_verb(codec, node, 0x708, 0x0);

    uint32_t amp_cap = hda_verb(codec, node, 0xf00, 0x12);
    hda_pin_set_output_volume(codec, node, amp_cap, 100);
    if (amp_cap != 0) {
        hda_pin_output_node    = node;
        hda_pin_output_amp_cap = amp_cap;
    }
    uint16_t n = hda_get_connection_list_entry(codec, node, 0);
    uint8_t type = hda_node_type(codec, n);
    if (type == HDA_WIDGET_AUDIO_OUTPUT) {
        printk("(%08x) : node %d is Audio Output\n", type, n);
        hda_init_audio_output(codec, n);
    } else if (type == HDA_WIDGET_MIXER) {
        printk("(%08x) : node %d is Mixer\n", type, n);
        hda_init_audio_mixer(codec, n);
    } else if (type == HDA_WIDGET_AUDIO_SELECTOR) {
        printk("(%08x) : node %d is Audio Selector\n", type, n);
        hda_init_audio_selector(codec, n);
    }
}

void hda_init_audio_selector(uint32_t codec, uint32_t node) {
    hda_verb(codec, node, 0x705, 0x0);
    hda_verb(codec, node, 0x708, 0x0);
    hda_verb(codec, node, 0x703, 0x0);

    uint32_t amp_cap = hda_verb(codec, node, 0xf00, 0x12);
    hda_pin_set_output_volume(codec, node, amp_cap, 100);
    if (amp_cap != 0) {
        hda_pin_output_node    = node;
        hda_pin_output_amp_cap = amp_cap;
    }
    uint16_t n = hda_get_connection_list_entry(codec, node, 0);
    uint8_t type = hda_node_type(codec, n);
    if (type == HDA_WIDGET_AUDIO_OUTPUT) {
        printk("(%08x) : node %d is Audio Output\n", type, n);
        hda_init_audio_output(codec, n);
    }
}

void hda_init_output_pin(uint32_t codec, uint32_t node) {
    hda_verb(codec, node, 0x705, 0x0);
    hda_verb(codec, node, 0x708, 0x0);
    hda_verb(codec, node, 0x703, 0x0);

    hda_verb(codec, node, 0x707, hda_verb(codec, node, 0xf07, 0) | (1 << 6) | (1 << 7));
    hda_verb(codec, node, 0x70c, 1);

    uint32_t amp_cap = hda_verb(codec, node, 0xf00, 0x12);
    hda_pin_set_output_volume(codec, node, amp_cap, 100);
    if (amp_cap != 0) {
        hda_pin_output_node    = node;
        hda_pin_output_amp_cap = amp_cap;
    }
    hda_verb(codec, node, 0x701, 0);
    uint16_t n = hda_get_connection_list_entry(codec, node, 0);
    uint8_t type = hda_node_type(codec, n);
    if (type == HDA_WIDGET_AUDIO_OUTPUT) {
        printk("(%08x) : node %d is Audio Output\n", type, n);
        hda_init_audio_output(codec, n);
    } else if (type == HDA_WIDGET_MIXER) {
        printk("(%08x) : node %d is Mixer\n", type, n);
        hda_init_audio_mixer(codec, n);
    } else if (type == HDA_WIDGET_AUDIO_SELECTOR) {
        printk("(%08x) : node %d is Audio Selector\n", type, n);
        hda_init_audio_selector(codec, n);
    }
}

void hda_init_afg(uint32_t codec, uint32_t node) {
    hda_verb(codec, node, 0x7ff, 0);
    hda_verb(codec, node, 0x705, 0);
    hda_verb(codec, node, 0x708, 0);

    hda_afg_pcm_format_cap    = hda_verb(codec, node, 0xF00, 0xA);
    hda_afg_stream_format_cap = hda_verb(codec, node, 0xF00, 0xB);
    hda_afg_amp_cap           = hda_verb(codec, node, 0xF00, 0x12);

    printk("pcm format cap: %08x\n", hda_afg_pcm_format_cap);
    printk("stream format cap: %08x\n", hda_afg_stream_format_cap);
    printk("amp cap: %08x\n", hda_afg_amp_cap);

    uint32_t count_raw  = hda_verb(codec, node, 0xF00, 0x4);
    uint32_t node_start = (count_raw >> 16) & 0xff;
    uint32_t node_count = count_raw & 0xff;
    printk("(%08x) : node %d has %d subnodes from %d\n", count_raw, node, node_count, node_start);
    uint32_t pin_speaker   = 0;
    uint32_t pin_headphone = 0;
    uint32_t pin_output    = 0;
    for (int i = node_start; i < node_start + node_count; i++) {
        uint8_t type = hda_node_type(codec, i);
        if (type == HDA_WIDGET_AUDIO_OUTPUT) {
            printk("(%08x) : node %d is Audio Output\n", type, i);
            hda_verb(codec, i, 0x706, 0x0);
        }
        if (type == HDA_WIDGET_PIN_COMPLEX) {
            printk("(%08x) : node %d is Pin Complex\n", type, i);
            uint8_t pin_type = hda_pin_complex_type(codec, i);
            printk("(%08x) : pin type is %d\n", pin_type, i);
            if (pin_type == HDA_PIN_COMPLEX_LINE_OUT) {
                printk("(%08x) : node %d is Line Out\n", pin_type, i);
                pin_output = i;
            } else if (pin_type == HDA_PIN_COMPLEX_SPEAKER) {
                printk("(%08x) : node %d is Speaker\n", pin_type, i);
                if (pin_speaker == 0) pin_speaker = i;
                if ((hda_verb(codec, i, 0xf00, 0x09) & 0b100) &&
                    (hda_verb(codec, i, 0xf1c, 0) >> 30 & 0b11) != 1 &&
                    (hda_verb(codec, i, 0xf00, 0x0c) & 0b10000)) {
                    printk("the speaker is connected ready to output\n");
                    pin_speaker = i;
                } else {
                    printk("no output\n");
                }
            } else if (pin_type == HDA_PIN_COMPLEX_HP_OUT) {
                printk("(%08x) : node %d is Headphone\n", pin_type, i);
                pin_headphone = i;
            } else if (pin_type == HDA_PIN_COMPLEX_CD) {

                printk("(%08x) : node %d is CD\n", pin_type, i);
                pin_output = i;
            } else if (pin_type == HDA_PIN_COMPLEX_SPDIF_OUT) {
                printk("(%08x) : node %d is SPDIF Out\n", pin_type, i);
                pin_output = i;
            } else if (pin_type == HDA_PIN_COMPLEX_DIG_OUT) {
                printk("(%08x) : node %d is Digital Out\n", pin_type, i);
                pin_output = i;
            } else if (pin_type == HDA_PIN_COMPLEX_MODEM_LINE_SIDE) {
                printk("(%08x) : node %d is Modem Line Side\n", pin_type, i);
                pin_output = i;
            } else if (pin_type == HDA_PIN_COMPLEX_MODEM_HANDSET_SIDE) {
                printk("(%08x) : node %d is Modem Handset Side\n", pin_type, i);

                pin_output = i;
            }
        }
    }
    printk("pin_speaker: %d, pin_headphone: %d, pin_output: %d\n", pin_speaker, pin_headphone,
         pin_output);
    if (pin_speaker) {
        hda_init_output_pin(codec, pin_speaker);
        if (!pin_headphone) return;
        // TODO: 处理耳机
    } else if (pin_headphone) {
        hda_init_output_pin(codec, pin_headphone);
    } else if (pin_output) {
        hda_init_output_pin(codec, pin_output);
    }
}

uint32_t hda_verb(uint32_t codec, uint32_t node, uint32_t verb, uint32_t command) {
    if (verb == 0x2 || verb == 0x3) { verb <<= 8; }
    uint32_t value = ((codec << 28) | (node << 20) | (verb << 8) | (command));
    if (send_verb_method == SEND_VERB_METHOD_DMA) {
        corb[corb_write_pointer] = value;
        mem_set16(hda_base + 0x48, corb_write_pointer);
        clock_sleep(1);
        if ((mem_get16(hda_base + 0x58) & 0xff) != corb_write_pointer) {
            logkf("No response from hda.\n");
            return 0;
        }
        value              = rirb[corb_write_pointer * 2];
        corb_write_pointer = (corb_write_pointer + 1) % corb_entry_count;
        rirb_read_pointer  = (rirb_read_pointer + 1) % rirb_entry_count;
        return value;
    } else {
        mem_set16(hda_base + 0x68, 0b10);
        mem_set32(hda_base + 0x60, value);
        mem_set16(hda_base + 0x68, 1);
        clock_sleep(1);
        if ((mem_get16(hda_base + 0x68) & 0x3) != 0b10) {
            mem_set16(hda_base + 0x68, 0b10);
            logkf("No response from hda.\n");
            return 0;
        } else {
            mem_set16(hda_base + 0x68, 0b10);
        }
        return mem_get32(hda_base + 0x64);
    }
}

void hda_init_codec(uint32_t codec) {
    uint32_t count_raw  = hda_verb(codec, 0, 0xf00, 0x4);
    uint32_t node_start = (count_raw >> 16) & 0xff;
    uint32_t node_count = count_raw & 0xff;
    printk("(%08x) : codec %d has %d nodes from %d\n", count_raw, codec, node_count, node_start);
    for (int i = node_start; i < node_start + node_count; i++) {
        uint32_t type = hda_verb(codec, i, 0xf00, 0x5);
        if ((type & 0xff) == 0x1) {
            printk("(%08x) : node %d is Audio Function Group\n", type, i);
            hda_init_afg(codec, i);
            return;
        }
    }
}

void hda_interrupt_handler(registers_t *reg){
    if(hda_stopping){
        hda_stop();
    }
    __asm__("wbinvd");
    mem_set8(hda_output_base + 0x3, 1 << 2);
}

void hda_init(){
    pci_device_t *device = pci_find_vid_did(0x8086,0x2668);
    if(device == NULL){
        device = pci_find_vid_did(0x8086,0x27d8);
        if(device == NULL) device = pci_find_vid_did(0x1002,0x4383);
        if(device == NULL){
            klogf(false,"Cannot found Intel High Definition Audio.\n");
            return;
        }
    }
    klogf(true,"Loading Intel High Definition Audio driver...\n");

    //打开中断 | 启用总线主控 | 启用MMIO
    uint32_t d = read_pci(device->bus, device->slot, device->func, 0x04);
    write_pci(device->bus, device->slot, device->func, 0x04,((d & ~((uint32_t)1 << 10)) | (1 << 2) |(1 << 1)));

    hda_base = read_bar_n(device,0);

    mem_set32(hda_base + 0x08,0);
    clock_sleep(1);
    if ((mem_get32(hda_base + 0x08) & 0x01) != 0) {
        klogf(false,"Intel High Definition Audio reset failed.\n");
        return;
    }
    clock_sleep(1);
    mem_set32(hda_base + 0x08, 1);
    if ((mem_get32(hda_base + 0x08) & 0x01) != 1) {
        klogf(false,"Intel High Definition Audio reset failed.\n");
        return;
    }

    int input_stream_count = (mem_get16(hda_base + 0x00) >> 8) & 0x0f;
    hda_output_base = hda_base + 0x80 + (0x20 * input_stream_count);
    hda_output_buffer = kmalloc(4096);

    register_interrupt_handler(0x20 + 0xb,hda_interrupt_handler);
    mem_set32(hda_base + 0x20, ((uint32_t )1 << 31) | ((uint32_t )1 << input_stream_count));

    mem_set32(hda_base + 0x70, 0);
    mem_set32(hda_base + 0x74, 0);
    mem_set32(hda_base + 0x34, 0);
    mem_set32(hda_base + 0x38, 0);
    mem_set8(hda_base + 0x4c, 0);
    mem_set8(hda_base + 0x5c, 0);

    corb = kmalloc(4096);
    mem_set32(hda_base + 0x40, (uint32_t )corb);
    mem_set32(hda_base + 0x44, 0);

    uint32_t corb_size = mem_get8(hda_base + 0x4e) >> 4 & 0x0f;
    if (corb_size & 0b0001) {
        corb_entry_count = 2;
        mem_set8(hda_base + 0x4e, 0b00);
    } else if (corb_size & 0b0010) {
        corb_entry_count = 16;
        mem_set8(hda_base + 0x4e, 0b01);
    } else if (corb_size & 0b0100) {
        corb_entry_count = 256;
        mem_set8(hda_base + 0x4e, 0b10);
    } else {
        klogf(false,"HDA corb size not supported.\n");
        goto mmio;
    }

    mem_set16(hda_base + 0x4A, 0x8000);
    clock_sleep(1);
    if ((mem_get16(hda_base + 0x4A) & 0x8000) == 0) {
        klogf(false,"HDA corb reset failed\n");
        goto mmio;
    }
    mem_set16(hda_base + 0x4A, 0x0000);
    clock_sleep(1);
    if ((mem_get16(hda_base + 0x4A) & 0x8000) != 0) {
        klogf(false,"HDA corb reset failed\n");
        goto mmio;
    }
    mem_set16(hda_base + 0x48, 0);
    corb_write_pointer = 1;

    rirb = kmalloc(4096);
    mem_set32(hda_base + 0x50, (uint32_t )rirb);
    mem_set32(hda_base + 0x54, 0);

    uint8_t rirb_size = mem_get8(hda_base + 0x5e) >> 4 & 0x0f;
    if (rirb_size & 0b0001) {
        rirb_entry_count = 2;
        mem_set8(hda_base + 0x5e, 0b00);
    } else if (rirb_size & 0b0010) {
        rirb_entry_count = 16;
        mem_set8(hda_base + 0x5e, 0b01);
    } else if (rirb_size & 0b0100) {
        rirb_entry_count = 256;
        mem_set8(hda_base + 0x5e, 0b10);
    } else {
        klogf(false,"HDA rirb size not supported\n");
        goto mmio;
    }

    mem_set16(hda_base + 0x58, 0x8000);
    clock_sleep(1);
    mem_set16(hda_base + 0x5A, 0x0000);
    rirb_read_pointer = 1;

    mem_set8(hda_base + 0x4c, 0b10);
    mem_set8(hda_base + 0x5c, 0b10);
    send_verb_method = SEND_VERB_METHOD_DMA;
    goto end;

mmio:
    send_verb_method = SEND_VERB_METHOD_MMIO;
    mem_set8(hda_output_base + 0x4c, 0);
    mem_set8(hda_output_base + 0x5c, 0);
end:
    for (int i = 0; i < 16; i++) {
        uint32_t codec_id = hda_verb(i, 0, 0xf00, 0);
        if (codec_id != 0) {
            hda_codec_number = i;
            hda_init_codec(i);
            klogf(true,"Intel High Definition Audio load done!\n");
            return;
        }
    }
    klogf(true,"Intel High Definition Audio load done!\n");
}