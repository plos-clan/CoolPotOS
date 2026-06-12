#pragma once

#include "types.h"
#include "term/termios.h"

#define _IOC(a, b, c, d) (((a) << 30) | ((b) << 8) | (c) | ((d) << 16))
#define _IOC_NONE        0U
#define _IOC_WRITE       1U
#define _IOC_READ        2U

#define _IO(a, b)      _IOC(_IOC_NONE, (a), (b), 0)
#define _IOW(a, b, c)  _IOC(_IOC_WRITE, (a), (b), sizeof(c))
#define _IOR(a, b, c)  _IOC(_IOC_READ, (a), (b), sizeof(c))
#define _IOWR(a, b, c) _IOC(_IOC_READ | _IOC_WRITE, (a), (b), sizeof(c))

#define SEEK_SET  0 /* Seek from beginning of file.  */
#define SEEK_CUR  1 /* Seek from current position.  */
#define SEEK_END  2 /* Seek from end of file.  */
#define SEEK_DATA 3 /* seek to the next data */
#define SEEK_HOLE 4 /* seek to the next hole */
#define SEEK_MAX  SEEK_HOLE

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12
#define DT_WHT     14

#define AT_FDCWD (-100)

#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_SYMLINK_FOLLOW   0x400
#define AT_NO_AUTOMOUNT     0x800
#define AT_EMPTY_PATH       0x1000
#define AT_RECURSIVE        0x8000

#define O_RDONLY 00
#define O_WRONLY 01
#define O_RDWR   02

#define O_ACCMODE_FLAGS (O_RDONLY | O_WRONLY | O_RDWR)

#define O_CREAT     0100
#define O_EXCL      0200
#define O_NOCTTY    0400
#define O_TRUNC     01000
#define O_APPEND    02000
#define O_NONBLOCK  04000
#define O_DSYNC     010000
#define O_SYNC      04010000
#define O_RSYNC     04010000
#define O_DIRECTORY 0200000
#define O_NOFOLLOW  0400000
#define O_CLOEXEC   02000000

#define O_ASYNC     020000
#define O_DIRECT    040000
#define O_LARGEFILE 0100000
#define O_NOATIME   01000000
#define O_PATH      010000000
#define O_TMPFILE   020200000
#define O_NDELAY    O_NONBLOCK

#define O_STATUS_FLAGS                                                                             \
    (O_APPEND | O_NONBLOCK | O_DSYNC | O_ASYNC | O_DIRECT | O_LARGEFILE | O_NOATIME | O_SYNC       \
     | O_PATH)

#define O_SETFL_FLAGS (O_APPEND | O_NONBLOCK | O_ASYNC | O_DIRECT | O_NOATIME)

#define FD_CLOEXEC 0x1

struct dirent {
    long d_ino;
    long d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};

#define EPOLLIN        0x001
#define EPOLLPRI       0x002
#define EPOLLOUT       0x004
#define EPOLLRDNORM    0x040
#define EPOLLNVAL      0x020
#define EPOLLRDBAND    0x080
#define EPOLLWRNORM    0x100
#define EPOLLWRBAND    0x200
#define EPOLLMSG       0x400
#define EPOLLERR       0x008
#define EPOLLHUP       0x010
#define EPOLLRDHUP     0x2000
#define EPOLLEXCLUSIVE (1U << 28)
#define EPOLLWAKEUP    (1U << 29)
#define EPOLLONESHOT   (1U << 30)
#define EPOLLET        (1U << 31)

#define POLLIN     0x0001
#define POLLPRI    0x0002
#define POLLOUT    0x0004
#define POLLERR    0x0008
#define POLLHUP    0x0010
#define POLLNVAL   0x0020
#define POLLRDNORM 0x0040
#define POLLRDBAND 0x0080
#define POLLWRNORM 0x0100
#define POLLWRBAND 0x0200
#define POLLMSG    0x0400
#define POLLRDHUP  0x2000

#define TCGETS       0x5401
#define TCSETS       0x5402
#define TCSETSW      0x5403
#define TCSETSF      0x5404
#define TCGETA       0x5405
#define TCSETA       0x5406
#define TCSETAW      0x5407
#define TCSETAF      0x5408
#define TCGETS2      _IOR('T', 0x2A, struct termios2)
#define TCSETS2      _IOW('T', 0x2B, struct termios2)
#define TCSETSW2     _IOW('T', 0x2C, struct termios2)
#define TCSETSF2     _IOW('T', 0x2D, struct termios2)
#define TCSBRK       0x5409
#define TCXONC       0x540A
#define TCFLSH       0x540B
#define TIOCEXCL     0x540C
#define TIOCNXCL     0x540D
#define TIOCSCTTY    0x540E
#define TIOCGPGRP    0x540F
#define TIOCSPGRP    0x5410
#define TIOCOUTQ     0x5411
#define TIOCSTI      0x5412
#define TIOCGWINSZ   0x5413
#define TIOCSWINSZ   0x5414
#define TIOCMGET     0x5415
#define TIOCMBIS     0x5416
#define TIOCMBIC     0x5417
#define TIOCMSET     0x5418
#define TIOCGSOFTCAR 0x5419
#define TIOCSSOFTCAR 0x541A
#define FIONREAD     0x541B
#define TIOCINQ      FIONREAD
#define TIOCLINUX    0x541C
#define TIOCCONS     0x541D
#define TIOCGSERIAL  0x541E
#define TIOCSSERIAL  0x541F
#define TIOCPKT      0x5420
#define FIONBIO      0x5421
#define TIOCNOTTY    0x5422
#define TIOCSETD     0x5423
#define TIOCGETD     0x5424
#define TCSBRKP      0x5425
#define TIOCSBRK     0x5427
#define TIOCCBRK     0x5428
#define TIOCGSID     0x5429
#define TIOCGRS485   0x542E
#define TIOCSRS485   0x542F
#define TIOCGPTN     0x80045430
#define TIOCSPTLCK   0x40045431
#define TIOCGDEV     0x80045432
#define TCGETX       0x5432
#define TCSETX       0x5433
#define TCSETXF      0x5434
#define TCSETXW      0x5435
#define TIOCSIG      0x40045436
#define TIOCVHANGUP  0x5437
#define TIOCGPKT     0x80045438
#define TIOCGPTLCK   0x80045439
#define TIOCGEXCL    0x80045440
#define TIOCGPTPEER  0x5441
#define TIOCGISO7816 0x80285442
#define TIOCSISO7816 0xc0285443

#define TIOCPKT_DATA       0
#define TIOCPKT_FLUSHREAD  1
#define TIOCPKT_FLUSHWRITE 2
#define TIOCPKT_STOP       4
#define TIOCPKT_START      8
#define TIOCPKT_NOSTOP     16
#define TIOCPKT_DOSTOP     32
#define TIOCPKT_IOCTL      64

#define FIONCLEX        0x5450
#define FIOCLEX         0x5451
#define FIOASYNC        0x5452
#define TIOCSERCONFIG   0x5453
#define TIOCSERGWILD    0x5454
#define TIOCSERSWILD    0x5455
#define TIOCGLCKTRMIOS  0x5456
#define TIOCSLCKTRMIOS  0x5457
#define TIOCSERGSTRUCT  0x5458
#define TIOCSERGETLSR   0x5459
#define TIOCSERGETMULTI 0x545A
#define TIOCSERSETMULTI 0x545B

#define TIOCMIWAIT  0x545C
#define TIOCGICOUNT 0x545D
#define FIOQSIZE    0x5460

#define TIOCM_LE   0x001
#define TIOCM_DTR  0x002
#define TIOCM_RTS  0x004
#define TIOCM_ST   0x008
#define TIOCM_SR   0x010
#define TIOCM_CTS  0x020
#define TIOCM_CAR  0x040
#define TIOCM_RNG  0x080
#define TIOCM_DSR  0x100
#define TIOCM_CD   TIOCM_CAR
#define TIOCM_RI   TIOCM_RNG
#define TIOCM_OUT1 0x2000
#define TIOCM_OUT2 0x4000
#define TIOCM_LOOP 0x8000

struct file_handle {
    unsigned int handle_bytes;
    int handle_type;
    unsigned char f_handle[0];
};
