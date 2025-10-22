#include "syscall.h"
#include "krlibc.h"
#include "errno.h"

syscall_(uname, struct utsname *utsname) {
    if (unlikely(utsname == NULL)) return SYSCALL_FAULT_(EINVAL);
    char sysname[] = "CoolPotOS";
    char machine[] = "x86_64";
    char version[] = "0.0.1";
    memcpy(utsname->sysname, sysname, sizeof(sysname));
    memcpy(utsname->nodename, "localhost", 50);
    memcpy(utsname->release, KERNEL_NAME, sizeof(KERNEL_NAME));
    memcpy(utsname->version, version, sizeof(version));
    memcpy(utsname->machine, machine, sizeof(machine));
    return EOK;
}
