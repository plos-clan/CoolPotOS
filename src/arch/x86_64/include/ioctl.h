#pragma once

#define _IOC(a, b, c, d) (((a) << 30) | ((b) << 8) | (c) | ((d) << 16))
#define _IOC_NONE        0U
#define _IOC_WRITE       1U
#define _IOC_READ        2U

#define _IO(a, b)      _IOC(_IOC_NONE, (a), (b), 0)
#define _IOW(a, b, c)  _IOC(_IOC_WRITE, (a), (b), sizeof(c))
#define _IOR(a, b, c)  _IOC(_IOC_READ, (a), (b), sizeof(c))
#define _IOWR(a, b, c) _IOC(_IOC_READ | _IOC_WRITE, (a), (b), sizeof(c))

#define _IOC_NRBITS   8
#define _IOC_TYPEBITS 8

#ifndef _IOC_SIZEBITS
#    define _IOC_SIZEBITS 14
#endif

#ifndef _IOC_DIRBITS
#    define _IOC_DIRBITS 2
#endif

#define _IOC_NRMASK   ((1 << _IOC_NRBITS) - 1)
#define _IOC_TYPEMASK ((1 << _IOC_TYPEBITS) - 1)
#define _IOC_SIZEMASK ((1 << _IOC_SIZEBITS) - 1)
#define _IOC_DIRMASK  ((1 << _IOC_DIRBITS) - 1)

#define _IOC_NRSHIFT   0
#define _IOC_TYPESHIFT (_IOC_NRSHIFT + _IOC_NRBITS)
#define _IOC_SIZESHIFT (_IOC_TYPESHIFT + _IOC_TYPEBITS)
#define _IOC_DIRSHIFT  (_IOC_SIZESHIFT + _IOC_SIZEBITS)

#define _IOC_DIR(nr)  (((nr) >> _IOC_DIRSHIFT) & _IOC_DIRMASK)
#define _IOC_TYPE(nr) (((nr) >> _IOC_TYPESHIFT) & _IOC_TYPEMASK)
#define _IOC_NR(nr)   (((nr) >> _IOC_NRSHIFT) & _IOC_NRMASK)
#define _IOC_SIZE(nr) (((nr) >> _IOC_SIZESHIFT) & _IOC_SIZEMASK)

#define TCGETS       0x5401
#define TCSETS       0x5402
#define TCSETSW      0x5403
#define TCSETSF      0x5404
#define TCGETA       0x5405
#define TCSETA       0x5406
#define TCSETAW      0x5407
#define TCSETAF      0x5408
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

#define TIOCPKT_DATA       0
#define TIOCPKT_FLUSHREAD  1
#define TIOCPKT_FLUSHWRITE 2
#define TIOCPKT_STOP       4
#define TIOCPKT_START      8
#define TIOCPKT_NOSTOP     16
#define TIOCPKT_DOSTOP     32
#define TIOCPKT_IOCTL      64

#define TIOCSER_TEMT 0x01

#define FBIOGET_VSCREENINFO 0x4600
#define FBIOPUT_VSCREENINFO 0x4601
#define FBIOGET_FSCREENINFO 0x4602
#define FBIOGETCMAP         0x4604
#define FBIOPUTCMAP         0x4605
#define FBIOPAN_DISPLAY     0x4606
#define FBIOGET_CON2FBMAP   0x460f
#define FBIOPUT_CON2FBMAP   0x4610
#define FBIOBLANK           0x4611
#define FBIOGET_VBLANK      0x4612

#define CSIZE  0000060
#define CS5    0000000
#define CS6    0000020
#define CS7    0000040
#define CS8    0000060
#define CSTOPB 0000100
#define CREAD  0000200
#define PARENB 0000400
#define PARODD 0001000
#define HUPCL  0002000
#define CLOCAL 0004000

#define IXON  0002000 /* Enable start/stop output control.  */
#define IXANY 0004000 /* Enable any character to restart output.  */
#define IXOFF 0010000 /* Enable start/stop input control.  */
#define IMAXBEL                                                                                    \
    0020000           /* Ring bell when input queue is full
			    (not in POSIX).  */
#define IUTF8 0040000 /* Input is UTF8 (not in POSIX).  */

#define OPOST 0000001 /* Post-process output.  */
#define OLCUC                                                                                      \
    0000002            /* Map lowercase characters to uppercase on output.
			    (not in POSIX).  */
#define ONLCR  0000004 /* Map NL to CR-NL on output.  */
#define OCRNL  0000010 /* Map CR to NL on output.  */
#define ONOCR  0000020 /* No CR output at column 0.  */
#define ONLRET 0000040 /* NL performs CR function.  */
#define OFILL  0000100 /* Use fill characters for delay.  */
#define OFDEL  0000200 /* Fill is DEL.  */

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

#define N_TTY          0
#define N_SLIP         1
#define N_MOUSE        2
#define N_PPP          3
#define N_STRIP        4
#define N_AX25         5
#define N_X25          6
#define N_6PACK        7
#define N_MASC         8
#define N_R3964        9
#define N_PROFIBUS_FDL 10
#define N_IRDA         11
#define N_SMSBLOCK     12
#define N_HDLC         13
#define N_SYNC_PPP     14
#define N_HCI          15

#define FIOSETOWN  0x8901
#define SIOCSPGRP  0x8902
#define FIOGETOWN  0x8903
#define SIOCGPGRP  0x8904
#define SIOCATMARK 0x8905
#define SIOCGSTAMP 0x8906

#define SIOCADDRT 0x890B
#define SIOCDELRT 0x890C
#define SIOCRTMSG 0x890D

#define SIOCGIFNAME        0x8910
#define SIOCSIFLINK        0x8911
#define SIOCGIFCONF        0x8912
#define SIOCGIFFLAGS       0x8913
#define SIOCSIFFLAGS       0x8914
#define SIOCGIFADDR        0x8915
#define SIOCSIFADDR        0x8916
#define SIOCGIFDSTADDR     0x8917
#define SIOCSIFDSTADDR     0x8918
#define SIOCGIFBRDADDR     0x8919
#define SIOCSIFBRDADDR     0x891a
#define SIOCGIFNETMASK     0x891b
#define SIOCSIFNETMASK     0x891c
#define SIOCGIFMETRIC      0x891d
#define SIOCSIFMETRIC      0x891e
#define SIOCGIFMEM         0x891f
#define SIOCSIFMEM         0x8920
#define SIOCGIFMTU         0x8921
#define SIOCSIFMTU         0x8922
#define SIOCSIFNAME        0x8923
#define SIOCSIFHWADDR      0x8924
#define SIOCGIFENCAP       0x8925
#define SIOCSIFENCAP       0x8926
#define SIOCGIFHWADDR      0x8927
#define SIOCGIFSLAVE       0x8929
#define SIOCSIFSLAVE       0x8930
#define SIOCADDMULTI       0x8931
#define SIOCDELMULTI       0x8932
#define SIOCGIFINDEX       0x8933
#define SIOGIFINDEX        SIOCGIFINDEX
#define SIOCSIFPFLAGS      0x8934
#define SIOCGIFPFLAGS      0x8935
#define SIOCDIFADDR        0x8936
#define SIOCSIFHWBROADCAST 0x8937
#define SIOCGIFCOUNT       0x8938

#define SIOCGIFBR 0x8940
#define SIOCSIFBR 0x8941

#define SIOCGIFTXQLEN 0x8942
#define SIOCSIFTXQLEN 0x8943

#define SIOCDARP 0x8953
#define SIOCGARP 0x8954
#define SIOCSARP 0x8955

#define SIOCDRARP 0x8960
#define SIOCGRARP 0x8961
#define SIOCSRARP 0x8962

#define SIOCGIFMAP 0x8970
#define SIOCSIFMAP 0x8971

#define SIOCADDDLCI 0x8980
#define SIOCDELDLCI 0x8981

#define SIOCDEVPRIVATE   0x89F0
#define SIOCPROTOPRIVATE 0x89E0

/* CP_Kernel 特有 ioctl */
#define IOGPTYPE 0x545E // 获取分区类型
#define IOGPINFO 0x545F // 获取分区信息

struct ioctl_gpt_partition {
    uint16_t name[36];                  // 分区名
    uint64_t start;                     // 起始扇区
    uint64_t end;                       // 结束扇区
    uint64_t size;                      // 分区大小（单位：扇区）
    uint8_t  partition_type_guid[16];   // 分区类型 GUID
    uint8_t  unique_partition_guid[16]; // 分区 GUID
    uint8_t  disk_guid[16];
};
