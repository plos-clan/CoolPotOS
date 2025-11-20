.global kernel_thread_func
kernel_thread_func:
    ld ra, 0(sp) // 恢复 ra
    // sp 稍后恢复
    ld gp, 16(sp)   // 恢复 gp
    ld tp, 24(sp)   // 恢复 tp
    ld t0, 32(sp)   // 恢复 t0
    ld t1, 40(sp)   // 恢复 t1
    ld t2, 48(sp)   // 恢复 t2
    ld s0, 56(sp)   // 恢复 s0/fp
    ld s1, 64(sp)   // 恢复 s1
    ld a0, 72(sp)   // 恢复 a0
    ld a1, 80(sp)   // 恢复 a1
    ld a2, 88(sp)   // 恢复 a2
    ld a3, 96(sp)   // 恢复 a3
    ld a4, 104(sp)  // 恢复 a4
    ld a5, 112(sp)  // 恢复 a5
    ld a6, 120(sp)  // 恢复 a6
    ld a7, 128(sp)  // 恢复 a7
    ld s2, 136(sp)  // 恢复 s2
    ld s3, 144(sp)  // 恢复 s3
    ld s4, 152(sp)  // 恢复 s4
    ld s5, 160(sp)  // 恢复 s5
    ld s6, 168(sp)  // 恢复 s6
    ld s7, 176(sp)  // 恢复 s7
    ld s8, 184(sp)  // 恢复 s8
    ld s9, 192(sp)  // 恢复 s9
    ld s10, 200(sp) // 恢复 s10
    ld s11, 208(sp) // 恢复 s11
    ld t3, 216(sp)  // 恢复 t3
    ld t4, 224(sp)  // 恢复 t4
    ld t5, 232(sp)  // 恢复 t5
    ld t6, 240(sp)  // 恢复 t6

    addi sp, sp, 296
    // RISC-V: s1 存放函数指针，a2 是参数
    mv a0, a2 // 将 a2 作为第一个参数传递
    jalr s1   // 调用 s1 中的函数指针
    // 线程退出
    li a0, 0      // 参数设为 0
    // j task_exit // 调用 task_exit