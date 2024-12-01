#include "../include/tty.h"
#include "../include/task.h"
#include "../include/graphics.h"
#include "../include/printf.h"

extern uint32_t volatile*screen;
extern uint32_t width, height;

static char eos[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'f', 'J', 'K', 'S', 'T', 'm'};
// vt100控制字符中可能的结束符号

static inline int t_is_eos(char ch) {
    for (int i = 0; i < sizeof(eos); i++) {
        if (ch == eos[i]) return 1;
    }
    return 0;
}

static inline long atol(char* s) {
      bool neg = *s == '-';
      if (neg || *s == '+') s++;
      long n = 0;
      for (; isdigit(*s); s++)
        n = n * 10 + (*s - '0');
      return neg ? -n : n;
}

static void tty_gotoxy(struct tty *res, int x, int y) {
    if (res->x == x && res->y == y) return;
    if (x >= 0 && y >= 0) {
        int x2 = x;
        int y2 = y;
        if (x <= res->xsize - 1 && y <= res->ysize - 1) {
            res->MoveCursor(res, x, y);
            return;
        }
        if (x <= res->xsize - 1) {
            for (int i = 0; i < y - res->ysize + 1; i++) {
                res->screen_ne(res);
            }
            res->MoveCursor(res, x, res->ysize - 1);
            return;
        }
        if (x > res->xsize - 1) {
            y2 += x / res->xsize - 1;
            x2  = x % res->xsize;
            if (y2 <= res->ysize - 1)
                tty_gotoxy(res, x2, y2 + 1);
            else
                tty_gotoxy(res, x2, y2);
        }
    } else {
        if (x < 0) {
            x += res->xsize;
            y--;
            tty_gotoxy(res, x, y);
        }
        if (y < 0) { return; }
    }
}

static int parse_vt100(struct tty *res, char *string) {
    switch (res->mode) {
        case MODE_A: {
            char dig_string[81] = {0};
            for (int i = 2, j = 0; string[i]; i++) {
                if (t_is_eos(string[i])) break;
                if (!isdigit(string[i])) return 0;
                dig_string[j++] = string[i];
            }
            int delta = atol(dig_string);
            if (!delta) delta = 1;
            res->gotoxy(res, res->x, res->y - delta);
            return 1;
        }
        case MODE_B: {
            char dig_string[81] = {0};
            for (int i = 2, j = 0; string[i]; i++) {
                if (t_is_eos(string[i])) break;
                if (!isdigit(string[i])) return 0;
                dig_string[j++] = string[i];
            }
            int delta = atol(dig_string);
            if (!delta) delta = 1;
            res->gotoxy(res, res->x, res->y + delta);
            return 1;
        }
        case MODE_C: {
            char dig_string[81] = {0};
            for (int i = 2, j = 0; string[i]; i++) {
                if (t_is_eos(string[i])) break;
                if (!isdigit(string[i])) return 0;
                dig_string[j++] = string[i];
            }
            int delta = atol(dig_string);
            if (!delta) delta = 1;
           // res->gotoxy(res, res->x + delta, res->y); TODO gotoxy
            return 1;
        }
        case MODE_D: {
            vbe_putchar('\b');
            return 1;
            /* TODO VT100 MODE D
            char dig_string[81] = {0};
            for (int i = 2, j = 0; string[i]; i++) {
                if (t_is_eos(string[i])) break;
                if (!isdigit(string[i])) return 0;
                dig_string[j++] = string[i];
            }
            int delta = atol(dig_string);
            if (!delta) delta = 1;
            res->gotoxy(res, res->x - delta, res->y);
            return 1;
             */
        }
        case MODE_E: {
            char dig_string[81] = {0};
            for (int i = 2, j = 0; string[i]; i++) {
                if (t_is_eos(string[i])) break;
                if (!isdigit(string[i])) return 0;
                dig_string[j++] = string[i];
            }
            int delta = atol(dig_string);
            if (!delta) delta = 1;
            res->gotoxy(res, 0, res->y + delta);
            return 1;
        }
        case MODE_F: {
            char dig_string[81] = {0};
            for (int i = 2, j = 0; string[i]; i++) {
                if (t_is_eos(string[i])) break;
                if (!isdigit(string[i])) return 0;
                dig_string[j++] = string[i];
            }
            int delta = atol(dig_string);
            if (!delta) delta = 1;
            res->gotoxy(res, 0, res->y - delta);
            return 1;
        }
        case MODE_G: {
            char dig_string[81] = {0};
            for (int i = 2, j = 0; string[i]; i++) {
                if (t_is_eos(string[i])) break;
                if (!isdigit(string[i])) return 0;
                dig_string[j++] = string[i];
            }
            int delta = atol(dig_string);
            if (!delta) delta = 1;
            res->gotoxy(res, delta - 1, res->y); // 可能是这样的
            return 1;
        }
        case MODE_H: {
            int  k = 0;
            char dig_string[2][81];
            memset(dig_string, 0, sizeof(dig_string)); // 全部设置为0
            for (int i = 2, j = 0; string[i]; i++) {
                if (t_is_eos(string[i])) break;
                if (!isdigit(string[i])) {
                    if (string[i] == ';') { // 分号
                        k++;
                        j = 0;
                        continue;
                    }
                    return 0;
                }
                dig_string[k][j++] = string[i];
            }
            if (k > 1) return 0;

            int delta[2] = {0};
            for (int i = 0; i <= k; i++) {
                delta[i] = atol(dig_string[i]);
            }
            if (k == 0 && delta[0] != 0) return 0;
            switch (k) {
                case 0: res->gotoxy(res, 0, 0); break;
                case 1: res->gotoxy(res, delta[0], delta[1]); break;
                default: return 0;
            }
            return 1;
        }
        case MODE_J: {
            char dig_string[81] = {0};
            for (int i = 2, j = 0; string[i]; i++) {
                if (t_is_eos(string[i])) break;
                if (!isdigit(string[i])) return 0;
                dig_string[j++] = string[i];
            }
            int delta = atol(dig_string);
            int old_x = res->x, old_y = res->y;
            switch (delta) {
                case 0: {
                    int flag = 0;
                    for (int i = old_y * res->xsize + old_x; i < res->xsize * res->ysize; i++) {
                        flag = 1;
                        res->putchar(res, ' ');
                    }
                    if (flag) res->gotoxy(res, old_x, old_y);
                    break;
                }
                case 1:
                    res->gotoxy(res, 0, 0);
                    for (int i = 0; i < old_y * res->xsize + old_x; i++) {
                        res->putchar(res, ' ');
                    }
                    break;
                case 2: res->clear(res); break;
                default: return 0;
            }
            return 1;
        }
        case MODE_K: {
            char dig_string[81] = {0};
            for (int i = 2, j = 0; string[i]; i++) {
                if (t_is_eos(string[i])) break;
                if (!isdigit(string[i])) return 0;
                dig_string[j++] = string[i];
            }
            int delta = atol(dig_string);
            int old_x = res->x, old_y = res->y;
            switch (delta) {
                case 0: {
                    int flag = 0;
                    for (int i = old_y * res->xsize + old_x; i < (old_y + 1) * res->ysize; i++) {
                        flag = 1;
                        res->putchar(res, ' ');
                    }
                    if (flag) res->gotoxy(res, old_x, old_y);
                    break;
                }
                case 1:
                    res->gotoxy(res, 0, res->y);
                    for (int i = old_y * res->xsize; i < old_y * res->xsize + old_x; i++) {
                        res->putchar(res, ' ');
                    }
                    break;
                case 2:
                    res->gotoxy(res, 0, res->y);
                    for (int i = old_y * res->xsize; i < (old_y + 1) * res->xsize; i++) {
                        res->putchar(res, ' ');
                    }
                    res->gotoxy(res, old_x, old_y);
                    break;
                default: return 0;
            }
            return 1;
        }
        case MODE_S: return 0; // unsupported
        case MODE_T: {
            char dig_string[81] = {0};
            for (int i = 2, j = 0; string[i]; i++) {
                if (t_is_eos(string[i])) break;
                if (!isdigit(string[i])) return 0;
                dig_string[j++] = string[i];
            }
            int delta = atol(dig_string);
            if (delta) return 0;
            res->screen_ne(res);
            return 1;
        }
        case MODE_m: {
            int  k = 0;
            char dig_string[2][81];
            memset(dig_string, 0, sizeof(dig_string)); // 全部设置为0
            for (int i = 2, j = 0; string[i]; i++) {
                if (t_is_eos(string[i])) break;
                if (!isdigit(string[i])) {
                    if (string[i] == ';') { // 分号
                        k++;
                        j = 0;
                        continue;
                    }
                    return 0;
                }
                dig_string[k][j++] = string[i];
            }
            if (k > 1) {
                return 0;
            }

            int delta[2] = {0};
            for (int i = 0; i <= k; i++) {
                delta[i] = atol(dig_string[i]);
            }

            if (delta[0] == 0 && k == 0) {
                int sel_color    = res->color_saved != -1 ? res->color_saved : res->color;
                //res->color       = sel_color;
                res->color_saved = -1;
                res->vt_status = 0;
                return 1;
            }
            static const uint32_t color_map[8] = {0x0, 0xFF0000, 0x00FF00, 0xFFFF00, 0x0000FF, 0xFF69B4, 0x00FFFF, 0xFFFFFF};

            switch (k) {
                case 0: {
                    if(delta[0] <= 8){
                        switch (delta[0]) {
                            case 0:
                                res->vt_status = 0;
                                return 1;
                            case 1:
                                res->vt_status |= VT100_BOLD;
                                return 1;
                            case 2:
                                res->vt_status |= VT100_DIM;
                                return 1;
                            case 3:
                                res->vt_status |= VT100_SMSO;
                                return 1;
                            case 4:
                                res->vt_status |= VT100_SMUL;
                                return 1;
                            case 5:
                                res->vt_status |= VT100_BLINK;
                                return 1;
                            case 6:
                                res->vt_status |= VT100_REV;
                                return 1;
                            case 7:
                                uint32_t color = res->color;
                                res->color = res->back_color;
                                res->back_color = color;
                                return 1;
                        }
                    } else if (delta[0] >= 30 && delta[0] <= 38) { // foreground color
                        if (res->color_saved == -1) res->color_saved = res->color;
                        res->color = color_map[delta[0] - 30];
                        return 1;
                    } else if (delta[0] >= 40 && delta[0] <= 48) {
                        if (res->color_saved == -1) res->color_saved = res->color;
                        res->back_color = (color_map[delta[0] - 40]);
                        return 1;
                    } else {
                        if(delta[0] == 39){
                            res->color = 0xc6c6c6;
                            return 1;
                        } else if(delta[0] == 49){
                            res->back_color = 0;
                            return 1;
                        }
                        return 0;
                    }
                }
                case 1: {
                    if (delta[0] != 1) {
                        return 0;
                    }
                    if (delta[1] >= 30 && delta[1] <= 37) { // foreground color
                        if (res->color_saved == -1) { res->color_saved = res->color; }
                        res->color = color_map[delta[1] - 30];
                        return 1;
                    } else if (delta[1] >= 40 && delta[1] <= 47) {
                        if (res->color_saved == -1) res->color_saved = res->color;
                        res->back_color = (color_map[delta[1] - 40]);
                        return 1;
                    } else {
                        return 0;
                    }
                }
                default: return 0;
            }
        }
        default: break;
    }
    return 0;
}

void tty_print(struct tty *res,const char *string){
    for (int i = 0; i < strlen(string);i++) {
        tty_putchar(res,string[i]);
    }
}
void tty_putchar(struct tty *res,int ch){
    if (ch == '\033' && res->vt100 == 0) {
        memset(res->buffer, 0, 81);
        res->buf_p                = 0;
        res->buffer[res->buf_p++] = '\033';
        res->vt100                = 1;
        res->done                 = 0;
        return;
    }else if (res->vt100 && res->buf_p == 1) {
        if (ch == '[') {
            res->buffer[res->buf_p++] = ch;
            return;
        } else {
            res->vt100 = 0;
            for (int i = 0; i < res->buf_p; i++) {
                res->putchar(res, res->buffer[i]);
            }
        }
    }else if (res->vt100 && res->buf_p >= 81) {
        for (int i = 0; i < res->buf_p; i++) {
            res->putchar(res, res->buffer[i]);
        }
        res->vt100 = 0;
    }else if (res->vt100) {
        res->buffer[res->buf_p++] = ch;
        if (t_is_eos(ch)) {
            res->mode = (vt100_mode_t)ch;
            if (!parse_vt100(res, res->buffer)) { // 失败了
                for (int i = 0; i < res->buf_p; i++) {
                    res->putchar(res, res->buffer[i]);
                }
            }
            res->vt100 = 0;
            return;
        } else if (!isdigit(ch) && ch != ';') {
            for (int i = 0; i < res->buf_p; i++) {
                res->putchar(res, res->buffer[i]);
            }
            res->vt100 = 0;
            return;
        }
        return;
    }
    res->putchar(res,ch);
}
void tty_MoveCursor(struct tty *res,int x, int y){

}
void tty_clear(struct tty *res){
    vbe_clear();
}

static void t_putc(tty_t *res,char ch){
    vbe_putchar(ch);
}

void init_default_tty(struct task_struct *task){
    task->tty->fifo = kmalloc(sizeof(struct FIFO8));
    char* buffer = kmalloc(256);

    extern uint32_t color, back_color;
    extern int32_t cx, cy;
    extern uint32_t c_width, c_height;

    if(get_current() != NULL && get_current()->tty != NULL){
        task->tty->vram = get_current()->tty->vram;
        task->tty->width = get_current()->tty->width;
        task->tty->height = get_current()->tty->height;
        task->tty->color = 0xc6c6c6;
        task->tty->cx = get_current()->tty->cx;
        task->tty->cy = get_current()->tty->cy;
        task->tty->c_width = get_current()->tty->c_width;
        task->tty->c_height = get_current()->tty->c_height;
        task->tty->back_color = 0x000000;
    } else{
        task->tty = kmalloc(sizeof(tty_t));
        task->tty->vram = screen;
        task->tty->width = width;
        task->tty->height = height;
        task->tty->color = color;
        task->tty->cx = cx;
        task->tty->cy = cy;
        task->tty->c_width = c_width;
        task->tty->c_height = c_height;
        task->tty->back_color = back_color;
    }

    task->tty->is_using = true;
    task->tty->print = tty_print;
    task->tty->clear = vbe_clear;
    task->tty->putchar = t_putc;
    task->tty->gotoxy = tty_gotoxy;
    task->tty->screen_ne = screen_ne_TextMode;
    //task->tty->MoveCursor = move_cur_TextMode;

    fifo8_init(task->tty->fifo,256,buffer);
}

void free_tty(struct task_struct *task){
    kfree(task->tty->fifo->buf);
    kfree(task->tty->fifo);
    kfree(task->tty);
}

