#pragma once

#define FB_TYPE_PACKED_PIXELS      0 /* Packed Pixels	*/
#define FB_TYPE_PLANES             1 /* Non interleaved planes */
#define FB_TYPE_INTERLEAVED_PLANES 2 /* Interleaved planes	*/
#define FB_TYPE_TEXT               3 /* Text/attributes	*/
#define FB_TYPE_VGA_PLANES         4 /* EGA/VGA planes	*/
#define FB_TYPE_FOURCC             5 /* Type identified by a V4L2 FOURCC */

#define FB_VISUAL_MONO01             0 /* Monochr. 1=Black 0=White */
#define FB_VISUAL_MONO10             1 /* Monochr. 1=White 0=Black */
#define FB_VISUAL_TRUECOLOR          2 /* True color	*/
#define FB_VISUAL_PSEUDOCOLOR        3 /* Pseudo color (like atari) */
#define FB_VISUAL_DIRECTCOLOR        4 /* Direct color */
#define FB_VISUAL_STATIC_PSEUDOCOLOR 5 /* Pseudo color readonly */
#define FB_VISUAL_FOURCC             6 /* Visual identified by a V4L2 FOURCC */

#include "ctype.h"
#include "limine.h"

extern struct limine_framebuffer *framebuffer;

struct fb_con2fbmap {
    int console;     // 控制台编号 /dev/ttyX
    int framebuffer; // 映射到哪个 /dev/fbX
};

struct fb_fix_screeninfo {
    char     id[16];       /* identification string eg "TT Builtin" */
    uint64_t smem_start;   /* Start of frame buffer mem */
                           /* (physical address) */
    uint32_t smem_len;     /* length of frame buffer mem */
    uint32_t type;         /* see FB_TYPE_*		*/
    uint32_t type_aux;     /* Interleave for interleaved Planes */
    uint32_t visual;       /* see FB_VISUAL_*		*/
    uint16_t xpanstep;     /* zero if no hardware panning  */
    uint16_t ypanstep;     /* zero if no hardware panning  */
    uint16_t ywrapstep;    /* zero if no hardware ywrap    */
    uint32_t line_length;  /* length of a line in bytes    */
    uint64_t mmio_start;   /* Start of Memory Mapped I/O   */
                           /* (physical address) */
    uint32_t mmio_len;     /* length of Memory Mapped I/O  */
    uint32_t accel;        /* Indicate to driver which	*/
                           /*  specific chip/card we have	*/
    uint16_t capabilities; /* see FB_CAP_*			*/
    uint16_t reserved[2];  /* reserved for future compatibility */
};

struct fb_bitfield {
    uint32_t offset;    /* beginning of bitfield	*/
    uint32_t length;    /* length of bitfield		*/
    uint32_t msb_right; /* != 0 : Most significant bit is */
                        /* right */
};

struct fb_var_screeninfo {
    uint32_t xres; /* visible resolution		*/
    uint32_t yres;
    uint32_t xres_virtual; /* virtual resolution		*/
    uint32_t yres_virtual;
    uint32_t xoffset; /* offset from virtual to visible */
    uint32_t yoffset; /* resolution			*/

    uint32_t           bits_per_pixel; /* guess what			*/
    uint32_t           grayscale;      /* 0 = color, 1 = grayscale,	*/
                                       /* >1 = FOURCC			*/
    struct fb_bitfield red;            /* bitfield in fb mem if true color, */
    struct fb_bitfield green;          /* else only length is significant */
    struct fb_bitfield blue;
    struct fb_bitfield transp; /* transparency			*/

    uint32_t nonstd; /* != 0 Non standard pixel format */

    uint32_t activate; /* see FB_ACTIVATE_*		*/

    uint32_t height; /* height of picture in mm    */
    uint32_t width;  /* width of picture in mm     */

    uint32_t accel_flags; /* (OBSOLETE) see fb_info.flags */

    /* Timing: All values in pixclocks, except pixclock (of course) */
    uint32_t pixclock;     /* pixel clock in ps (pico seconds) */
    uint32_t left_margin;  /* time from sync to picture	*/
    uint32_t right_margin; /* time from picture to sync	*/
    uint32_t upper_margin; /* time from sync to picture	*/
    uint32_t lower_margin;
    uint32_t hsync_len;   /* length of horizontal sync	*/
    uint32_t vsync_len;   /* length of vertical sync	*/
    uint32_t sync;        /* see FB_SYNC_*		*/
    uint32_t vmode;       /* see FB_VMODE_*		*/
    uint32_t rotate;      /* angle we rotate counter clockwise */
    uint32_t colorspace;  /* colorspace for FOURCC-based modes */
    uint32_t reserved[4]; /* reserved for future compatibility */
};

void init_gop();
void gop_dev_setup();

void gop_clear(uint32_t color);

uint64_t get_screen_width();
uint64_t get_screen_height();
