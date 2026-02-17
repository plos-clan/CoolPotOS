#include "driver/pci/msi.h"
#include "errno.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "term/klog.h"

/**
 * @brief 生成msi消息
 *
 * @param msi_desc msi描述符
 * @return struct msi_msg_t* msi消息指针（在描述符内）
 */
struct msi_msg_t *msi_arch_get_msg(struct msi_desc_t *msi_desc) {
#if defined(__x86_64__)

#    define ia64_pci_get_arch_msi_message_address(processor)                                       \
        (0xfee00000UL | ((uint8_t)processor << 12))

#    define ia64_pci_get_arch_msi_message_data(vector, processor, edge_trigger, assert)            \
        ((uint32_t)((vector & 0xff) | ((edge_trigger == 1) ? 0 : (1 << 15))                        \
                    | ((assert == 0) ? 0 : (1 << 14))))
    msi_desc->msg.address_hi = msi_desc->processor & 0xFFFFFF00;
    msi_desc->msg.address_lo = ia64_pci_get_arch_msi_message_address(msi_desc->processor);
    msi_desc->msg.data       = ia64_pci_get_arch_msi_message_data(
        msi_desc->irq_num, msi_desc->processor, msi_desc->edge_trigger, msi_desc->assert
    );
    msi_desc->msg.vector_control = 0;
#endif
    return &(msi_desc->msg);
}

static inline struct pci_msi_cap_t
__msi_read_cap_list(struct msi_desc_t *msi_desc, uint32_t cap_off) {
    struct pci_msi_cap_t cap_list = { 0 };
    pci_device_t *ptr             = msi_desc->pci_dev;
    uint32_t dw0;
    dw0               = ptr->op->read(ptr->bus, ptr->slot, ptr->func, ptr->segment, cap_off);
    cap_list.cap_id   = dw0 & 0xff;
    cap_list.next_off = (dw0 >> 8) & 0xff;
    cap_list.msg_ctrl = (dw0 >> 16) & 0xffff;

    cap_list.msg_addr_lo =
        ptr->op->read(ptr->bus, ptr->slot, ptr->func, ptr->segment, cap_off + 0x4);
    uint16_t msg_data_off = 0xc;
    if (cap_list.msg_ctrl & (1 << 7)) // 64位
    {
        cap_list.msg_addr_hi =
            ptr->op->read(ptr->bus, ptr->slot, ptr->func, ptr->segment, cap_off + 0x8);
    } else {
        cap_list.msg_addr_hi = 0;
        msg_data_off         = 0x8;
    }

    cap_list.msg_data =
        ptr->op->read(ptr->bus, ptr->slot, ptr->func, ptr->segment, cap_off + msg_data_off)
        & 0xffff;

    cap_list.mask    = ptr->op->read(ptr->bus, ptr->slot, ptr->func, ptr->segment, cap_off + 0x10);
    cap_list.pending = ptr->op->read(ptr->bus, ptr->slot, ptr->func, ptr->segment, cap_off + 0x14);

    return cap_list;
}

/**
 * @brief 读取msix的capability list
 *
 * @param msi_desc msi描述符
 * @param cap_off capability list的offset
 * @return struct pci_msix_cap_t 对应的capability list
 */
static inline struct pci_msix_cap_t
__msi_read_msix_cap_list(struct msi_desc_t *msi_desc, uint32_t cap_off) {
    struct pci_msix_cap_t cap_list = { 0 };
    pci_device_t *ptr              = msi_desc->pci_dev;
    uint32_t dw0;
    dw0               = ptr->op->read(ptr->bus, ptr->slot, ptr->func, ptr->segment, cap_off);
    cap_list.cap_id   = dw0 & 0xff;
    cap_list.next_off = (dw0 >> 8) & 0xff;
    cap_list.msg_ctrl = (dw0 >> 16) & 0xffff;

    cap_list.dword1 = ptr->op->read(ptr->bus, ptr->slot, ptr->func, ptr->segment, cap_off + 0x4);
    cap_list.dword2 = ptr->op->read(ptr->bus, ptr->slot, ptr->func, ptr->segment, cap_off + 0x8);
    return cap_list;
}

/**
 * @brief 映射设备的msix表
 *
 * @param pci_dev pci设备信息结构体
 * @param msix_cap msix capability list的结构体
 * @return int 错误码
 */
static inline int __msix_map_table(pci_device_t *pci_dev, struct pci_msix_cap_t *msix_cap) {
    // msix table相对于bar寄存器中存储的地址的offset
    pci_dev->msix_offset     = msix_cap->dword1 & (~0x7);
    pci_dev->msix_table_size = (msix_cap->msg_ctrl & 0x7ff) + 1;
    pci_dev->msix_mmio_size  = pci_dev->msix_table_size * 16 + pci_dev->msix_offset;

    // 获取BAR的物理地址并映射到虚拟地址空间
    uint32_t bir = msix_cap->dword1 & 0x7;
    if (bir > 5) {
        printk("MSI-X: Invalid bir %d\n", bir);
        return -EINVAL;
    }

    uint64_t bar_physical_address = pci_dev->bars[bir].address;

    if (bar_physical_address == 0) {
        return -ENOMEM;
    }

    // 映射整个BAR区域（包括MSI-X表）
    pci_dev->msix_mmio_vaddr = (uint64_t)phys_to_virt((uint64_t)bar_physical_address);
    page_map_range(
        get_kernel_pagedir(),
        pci_dev->msix_mmio_vaddr,
        bar_physical_address,
        pci_dev->bars[bir].size,
        KERNEL_PTE_FLAGS
    );

    return 0;
}

/**
 * @brief 将msi_desc中的数据填写到msix表的指定表项处
 *
 * @param pci_dev pci设备结构体
 * @param msi_desc msi描述符
 */
static inline void __msix_set_entry(struct msi_desc_t *msi_desc) {
    uint64_t table_base = msi_desc->pci_dev->msix_mmio_vaddr + msi_desc->pci_dev->msix_offset;
    volatile uint32_t *entry_ptr = (volatile uint32_t *)(table_base + msi_desc->msi_index * 16);

    // 设置地址字段（低32位 + 高32位），使用小端格式
    entry_ptr[0] = msi_desc->msg.address_lo;
    entry_ptr[1] = msi_desc->msg.address_hi;

    // 设置数据字段和控制字段
    entry_ptr[2] = msi_desc->msg.data;
    entry_ptr[3] = msi_desc->msg.vector_control & ~1;
}

/**
 * @brief 清空设备的msix table的指定表项
 *
 * @param pci_dev pci设备
 * @param msi_index 表项号
 */
static inline void __msix_clear_entry(pci_device_t *pci_dev, uint16_t msi_index) {
    uint64_t table_base          = pci_dev->msix_mmio_vaddr + pci_dev->msix_offset;
    volatile uint64_t *entry_ptr = (volatile uint64_t *)(table_base + msi_index * 16);

    // 清除MSI-X表项
    entry_ptr[0] = 0;
    entry_ptr[1] = 0;
}

/**
 * @brief 启用 Message Signaled Interrupts
 *
 * @param header 设备header
 * @param vector 中断向量号
 * @param processor 要投递到的处理器
 * @param edge_trigger 是否边缘触发
 * @param assert 是否高电平触发
 *
 * @return 返回码
 */
int pci_enable_msi(struct msi_desc_t *msi_desc) {
    pci_device_t *ptr = msi_desc->pci_dev;
    uint32_t cap_ptr;
    uint32_t tmp;
    uint16_t message_control;
    uint64_t message_addr;

    if (msi_desc->pci.msi_attribute.is_msix) {
        cap_ptr = pci_enumerate_capability_list(ptr, 0x11);
        if (cap_ptr == 0) {
            cap_ptr = pci_enumerate_capability_list(ptr, 0x05);
            if (cap_ptr == 0)
                return -ENOSYS;
            msi_desc->pci.msi_attribute.is_msix = 0;
        }
    } else {
        cap_ptr = pci_enumerate_capability_list(ptr, 0x05);
        if (cap_ptr == 0)
            return -ENOSYS;
        msi_desc->pci.msi_attribute.is_msix = 0;
    }

    // 获取msi消息
    msi_arch_get_msg(msi_desc);

    // disable intx
    tmp = ptr->op->read(ptr->bus, ptr->slot, ptr->func, ptr->segment,
                        0x04); // 读取cap+0x0处的值
    tmp &= ~(1U << 10);
    ptr->op->write(ptr->bus, ptr->slot, ptr->func, ptr->segment, 0x04, tmp);

    if (msi_desc->pci.msi_attribute.is_msix) // MSI-X
    {
        if (msi_desc->msi_index == 0) {
            // 读取msix的信息
            struct pci_msix_cap_t cap = __msi_read_msix_cap_list(msi_desc, cap_ptr);
            // 映射msix table
            int ret = __msix_map_table(ptr, &cap);
            if (ret < 0) {
                return ret;
            }

            // 使能msi-x
            tmp = ptr->op->read(
                ptr->bus,
                ptr->slot,
                ptr->func,
                ptr->segment,
                cap_ptr + 0x2
            ); // 读取cap+0x2处的值
            tmp &= ~(1U << 14);
            tmp |= (1U << 15);
            ptr->op->write(ptr->bus, ptr->slot, ptr->func, ptr->segment, cap_ptr + 0x2, tmp);
        }

        // 设置msix的中断
        __msix_set_entry(msi_desc);
    } else {
        tmp = ptr->op->read(
            ptr->bus,
            ptr->slot,
            ptr->func,
            ptr->segment,
            cap_ptr
        ); // 读取cap+0x0处的值
        message_control = (tmp >> 16) & 0xffff;

        // 写入message address
        message_addr =
            ((((uint64_t)msi_desc->msg.address_hi) << 32)
             | msi_desc->msg.address_lo); // 获取message address
        ptr->op->write(
            ptr->bus,
            ptr->slot,
            ptr->func,
            ptr->segment,
            cap_ptr + 0x4,
            (uint32_t)(message_addr & 0xffffffff)
        );

        if (message_control & (1 << 7)) // 64位
            ptr->op->write(
                ptr->bus,
                ptr->slot,
                ptr->func,
                ptr->segment,
                cap_ptr + 0x8,
                (uint32_t)((message_addr >> 32) & 0xffffffff)
            );

        // 写入message data

        tmp = msi_desc->msg.data;
        if (message_control & (1 << 7)) // 64位
            ptr->op->write(ptr->bus, ptr->slot, ptr->func, ptr->segment, cap_ptr + 0xc, tmp);
        else
            ptr->op->write(ptr->bus, ptr->slot, ptr->func, ptr->segment, cap_ptr + 0x8, tmp);

        // 使能msi
        tmp = ptr->op->read(
            ptr->bus,
            ptr->slot,
            ptr->func,
            ptr->segment,
            cap_ptr + 0x2
        ); // 读取cap+0x2处的值
        tmp |= 1;
        tmp &= ~(7 << 4);
        ptr->op->write(ptr->bus, ptr->slot, ptr->func, ptr->segment, cap_ptr + 0x2, tmp);
    }

    return 0;
}
