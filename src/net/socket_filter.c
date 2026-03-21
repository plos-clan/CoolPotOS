#include "krlibc.h"
#include "net/socket_filter.h"
#include "term/klog.h"

int bpf_validate(const struct sock_filter *prog, int len) {
    if (len < 1) {
        printk("BPF: invalid program length %d\n", len);
        return -1;
    }

    for (int i = 0; i < len; i++) {
        const struct sock_filter *insn = &prog[i];
        uint16_t code = insn->code;

        switch (BPF_CLASS(code)) {
        case BPF_LD:
        case BPF_LDX:
            if (BPF_MODE(code) == BPF_MEM && insn->k >= 16) {
                printk("BPF: invalid M[] index at %d\n", i);
                return -1;
            }
            break;

        case BPF_ST:
        case BPF_STX:
            if (insn->k >= 16) {
                printk("BPF: invalid M[] index at %d\n", i);
                return -1;
            }
            break;

        case BPF_JMP:
            if (BPF_OP(code) == BPF_JA) {
                if (i + 1 + insn->k >= (uint32_t)len) {
                    printk("BPF: invalid jump at %d\n", i);
                    return -1;
                }
            } else {
                if (i + 1 + insn->jt >= len || i + 1 + insn->jf >= len) {
                    printk("BPF: invalid conditional jump at %d\n", i);
                    return -1;
                }
            }
            break;

        case BPF_RET:
        case BPF_ALU:
        case BPF_MISC:
            break;

        default:
            printk("BPF: unknown instruction class at %d\n", i);
            return -1;
        }
    }

    if (BPF_CLASS(prog[len - 1].code) != BPF_RET) {
        printk("BPF: program doesn't end with RET\n");
        return -1;
    }

    return 0;
}

uint32_t bpf_run(const struct sock_filter *prog, int proglen,
                 const uint8_t *data, uint32_t datalen) {
    struct bpf_vm vm = {0};
    vm.data = data;
    vm.len = datalen;
    vm.pc = 0;

    int max_insns = 1000000;

    while (vm.pc < (uint32_t)proglen && max_insns-- > 0) {
        const struct sock_filter *insn = &prog[vm.pc];
        uint16_t code = insn->code;
        uint32_t k = insn->k;

        switch (BPF_CLASS(code)) {
        case BPF_LD:
            switch (BPF_MODE(code)) {
            case BPF_IMM:
                vm.A = k;
                break;
            case BPF_ABS:
                switch (BPF_SIZE(code)) {
                case BPF_W:
                    vm.A = bpf_read_word(data, datalen, k);
                    break;
                case BPF_H:
                    vm.A = bpf_read_half(data, datalen, k);
                    break;
                case BPF_B:
                    vm.A = bpf_read_byte(data, datalen, k);
                    break;
                }
                break;
            case BPF_IND:
                switch (BPF_SIZE(code)) {
                case BPF_W:
                    vm.A = bpf_read_word(data, datalen, vm.X + k);
                    break;
                case BPF_H:
                    vm.A = bpf_read_half(data, datalen, vm.X + k);
                    break;
                case BPF_B:
                    vm.A = bpf_read_byte(data, datalen, vm.X + k);
                    break;
                }
                break;
            case BPF_MEM:
                vm.A = vm.M[k & 0xf];
                break;
            case BPF_LEN:
                vm.A = datalen;
                break;
            }
            vm.pc++;
            break;

        case BPF_LDX:
            switch (BPF_MODE(code)) {
            case BPF_IMM:
                vm.X = k;
                break;
            case BPF_MEM:
                vm.X = vm.M[k & 0xf];
                break;
            case BPF_LEN:
                vm.X = datalen;
                break;
            case BPF_MSH:
                vm.X = (bpf_read_byte(data, datalen, k) & 0x0f) * 4;
                break;
            }
            vm.pc++;
            break;

        case BPF_ST:
            vm.M[k & 0xf] = vm.A;
            vm.pc++;
            break;

        case BPF_STX:
            vm.M[k & 0xf] = vm.X;
            vm.pc++;
            break;

        case BPF_ALU: {
            uint32_t src = (BPF_SRC(code) == BPF_K) ? k : vm.X;
            switch (BPF_OP(code)) {
            case BPF_ADD:
                vm.A += src;
                break;
            case BPF_SUB:
                vm.A -= src;
                break;
            case BPF_MUL:
                vm.A *= src;
                break;
            case BPF_DIV:
                vm.A = src ? vm.A / src : 0;
                break;
            case BPF_MOD:
                vm.A = src ? vm.A % src : 0;
                break;
            case BPF_AND:
                vm.A &= src;
                break;
            case BPF_OR:
                vm.A |= src;
                break;
            case BPF_XOR:
                vm.A ^= src;
                break;
            case BPF_LSH:
                vm.A <<= src;
                break;
            case BPF_RSH:
                vm.A >>= src;
                break;
            case BPF_NEG:
                vm.A = (uint32_t)-((int32_t)vm.A);
                break;
            }
            vm.pc++;
            break;
        }

        case BPF_JMP: {
            uint32_t src = (BPF_SRC(code) == BPF_K) ? k : vm.X;
            bool cond = false;

            switch (BPF_OP(code)) {
            case BPF_JA:
                vm.pc += k;
                continue;
            case BPF_JEQ:
                cond = (vm.A == src);
                break;
            case BPF_JGT:
                cond = (vm.A > src);
                break;
            case BPF_JGE:
                cond = (vm.A >= src);
                break;
            case BPF_JSET:
                cond = (vm.A & src) != 0;
                break;
            }

            vm.pc++;
            vm.pc += cond ? insn->jt : insn->jf;
            continue;
        }

        case BPF_RET:
            if (BPF_RVAL(code) == BPF_A) {
                return vm.A;
            }
            return k;

        case BPF_MISC:
            switch (BPF_MISCOP(code)) {
            case BPF_TAX:
                vm.X = vm.A;
                break;
            case BPF_TXA:
                vm.A = vm.X;
                break;
            }
            vm.pc++;
            break;

        default:
            return 0;
        }
    }

    return 0;
}
