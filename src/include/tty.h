#ifndef CRASHPOWEROS_TTY_H
#define CRASHPOWEROS_TTY_H

#include "common.h"
#include "fifo.h"

typedef void (*key_lis) (uint8_t);

typedef enum {
    MODE_A = 'A',
    MODE_B = 'B',
    MODE_C = 'C',
    MODE_D = 'D',
    MODE_E = 'E',
    MODE_F = 'F',
    MODE_G = 'G',
    MODE_H = 'H',
    MODE_f = 'f',
    MODE_J = 'J',
    MODE_K = 'K',
    MODE_S = 'S',
    MODE_T = 'T',
    MODE_m = 'm'
} vt100_mode_t;

typedef struct tty{
    void (*print)(struct tty *res,const char *string);
    void (*putchar)(struct tty *res,int c);
    void (*MoveCursor)(struct tty *res,int x, int y);
    void (*clear)(struct tty *res);
    void (*gotoxy)(struct tty *res, int x, int y);
    void (*screen_ne)(struct tty *res);


    bool is_using;            // 是否可以接受输入
    struct FIFO8 *fifo;       // 键盘输出缓冲区
    key_lis keyboard_press;   // 键盘按下
    key_lis keyboard_release; // 键盘松开
    uint32_t *vram;
    uint32_t width,height;
    int xsize, ysize;
    int x, y;
    uint8_t color;
    int Raw_y;

    /* vt100 */
    int          vt100;       // 是否检测到标志
    char         buffer[81];  // 缓冲区
    int          buf_p;       // 缓冲区指针
    int          done;        // 这个东西读取完毕没有？
    vt100_mode_t mode;        // 控制模式
    int          color_saved; // 保存的颜色
}tty_t;

#include "task.h"

void init_default_tty(struct task_struct *task);
void free_tty(struct task_struct *task);
void clear_TextMode(struct tty *res);
void screen_ne_TextMode(struct tty *res);
void putchar_TextMode(struct tty *res, int c);

#endif
