#pragma once

void keyboard_init();
int input_char_inSM(); //提取当前进程缓冲队列顶部的键盘扫描码
int kernel_getch();    //提取转义后的ASCII字符