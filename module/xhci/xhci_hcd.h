#pragma once

#include "driver_subsystem.h"
#include "lock.h"

struct xhci_hcd;
typedef struct xhci_hcd xhci_hcd_t;

// XHCI能力寄存器
typedef struct {
    uint8_t  caplength;
    uint8_t  reserved;
    uint16_t hciversion;
    uint32_t hcsparams1;
    uint32_t hcsparams2;
    uint32_t hcsparams3;
    uint32_t hccparams1;
    uint32_t dboff;
    uint32_t rtsoff;
    uint32_t hccparams2;
} __attribute__((packed)) xhci_cap_regs_t;

// XHCI操作寄存器
typedef struct {
    uint32_t usbcmd;
    uint32_t usbsts;
    uint32_t pagesize;
    uint32_t reserved1[2];
    uint32_t dnctrl;
    uint64_t crcr;
    uint32_t reserved2[4];
    uint64_t dcbaap;
    uint32_t config;
} __attribute__((packed)) xhci_op_regs_t;

// XHCI端口寄存器
typedef struct {
    uint32_t portsc;
    uint32_t portpmsc;
    uint32_t portli;
    uint32_t porthlpmc;
} __attribute__((packed)) xhci_port_regs_t;

// XHCI运行时寄存器
typedef struct {
    uint32_t mfindex;
    uint32_t reserved[7];
} __attribute__((packed)) xhci_runtime_regs_t;

// XHCI中断寄存器
typedef struct {
    uint32_t iman;
    uint32_t imod;
    uint32_t erstsz;
    uint32_t reserved;
    uint64_t erstba;
    uint64_t erdp;
} __attribute__((packed)) xhci_intr_regs_t;

// USBCMD寄存器位
#define XHCI_CMD_RUN   (1 << 0)
#define XHCI_CMD_RESET (1 << 1)
#define XHCI_CMD_INTE  (1 << 2)
#define XHCI_CMD_HSEE  (1 << 3)
#define XHCI_CMD_EWE   (1 << 10)

// USBSTS寄存器位
#define XHCI_STS_HCH  (1 << 0)
#define XHCI_STS_HSE  (1 << 2)
#define XHCI_STS_EINT (1 << 3)
#define XHCI_STS_PCD  (1 << 4)
#define XHCI_STS_CNR  (1 << 11)

// PORTSC寄存器位
#define XHCI_PORTSC_CCS (1 << 0)
#define XHCI_PORTSC_PED (1 << 1)
#define XHCI_PORTSC_OCA (1 << 3)
#define XHCI_PORTSC_PR  (1 << 4)
#define XHCI_PORTSC_PP  (1 << 9)
#define XHCI_PORTSC_CSC (1 << 17)
#define XHCI_PORTSC_PEC (1 << 18)
#define XHCI_PORTSC_WRC (1 << 19)
#define XHCI_PORTSC_OCC (1 << 20)
#define XHCI_PORTSC_PRC (1 << 21)

// TRB类型
#define TRB_TYPE_NORMAL         1
#define TRB_TYPE_SETUP          2
#define TRB_TYPE_DATA           3
#define TRB_TYPE_STATUS         4
#define TRB_TYPE_ISOCH          5
#define TRB_TYPE_LINK           6
#define TRB_TYPE_EVENT_DATA     7
#define TRB_TYPE_NOOP           8
#define TRB_TYPE_ENABLE_SLOT    9
#define TRB_TYPE_DISABLE_SLOT   10
#define TRB_TYPE_ADDRESS_DEV    11
#define TRB_TYPE_CONFIG_EP      12
#define TRB_TYPE_EVAL_CTX       13
#define TRB_TYPE_RESET_EP       14
#define TRB_TYPE_STOP_EP        15
#define TRB_TYPE_SET_TR_DEQUEUE 16
#define TRB_TYPE_RESET_DEV      17
#define TRB_TYPE_NOOP_CMD       23
#define TRB_TYPE_TRANSFER       32
#define TRB_TYPE_CMD_COMPLETE   33
#define TRB_TYPE_PORT_STATUS    34

// 通用TRB标志
#define TRB_FLAG_CYCLE (1 << 0) // Cycle bit
#define TRB_FLAG_ENT   (1 << 1) // Evaluate Next TRB (非Link TRB)
#define TRB_FLAG_ISP   (1 << 2) // Interrupt on Short Packet
#define TRB_FLAG_NS    (1 << 3) // No Snoop
#define TRB_FLAG_CHAIN (1 << 4) // Chain bit
#define TRB_FLAG_IOC   (1 << 5) // Interrupt on Completion
#define TRB_FLAG_IDT   (1 << 6) // Immediate Data
#define TRB_FLAG_BEI   (1 << 9) // Block Event Interrupt

// Link TRB 特定标志
#define TRB_LINK_TOGGLE_CYCLE (1 << 1) // Toggle Cycle (Link TRB专用)
#define TRB_LINK_CHAIN        (1 << 4) // Chain bit (Link TRB中保持链)

#define TRB_FLAG_TC TRB_LINK_TOGGLE_CYCLE // Toggle Cycle的别名

// Slot Context - dev_info 字段
#define SLOT_CTX_ROUTE_STRING(x)    ((x) & 0xFFFFF)
#define SLOT_CTX_SPEED(x)           (((x) & 0xF) << 20)
#define SLOT_CTX_MTT(x)             (((x) & 0x1) << 25)
#define SLOT_CTX_HUB(x)             (((x) & 0x1) << 26)
#define SLOT_CTX_CONTEXT_ENTRIES(x) (((x) & 0x1F) << 27)

// Slot Context - dev_info2 字段
#define SLOT_CTX_MAX_EXIT_LATENCY(x) ((x) & 0xFFFF)
#define SLOT_CTX_ROOT_HUB_PORT(x)    (((x) & 0xFF) << 16)
#define SLOT_CTX_NUM_PORTS(x)        (((x) & 0xFF) << 24)

// EP Context - ep_info 字段
#define EP_CTX_EP_STATE(x)      ((x) & 0x7)
#define EP_CTX_MULT(x)          (((x) & 0x3) << 8)
#define EP_CTX_MAX_P_STREAMS(x) (((x) & 0x1F) << 10)
#define EP_CTX_LSA(x)           (((x) & 0x1) << 15)
#define EP_CTX_INTERVAL(x)      (((x) & 0xFF) << 16)
#define EP_CTX_MAX_ESIT_HI(x)   (((x) & 0xFF) << 24)

// EP Context - ep_info2 字段
#define EP_CTX_ERROR_COUNT(x)     (((x) & 0x3) << 1)
#define EP_CTX_EP_TYPE(x)         (((x) & 0x7) << 3)
#define EP_CTX_HID(x)             (((x) & 0x1) << 7)
#define EP_CTX_MAX_BURST(x)       (((x) & 0xFF) << 8)
#define EP_CTX_MAX_PACKET_SIZE(x) (((x) & 0xFFFF) << 16)

// EP Context - tr_dequeue_ptr 字段
#define EP_CTX_DCS (1ULL << 0)

// EP Types
#define EP_TYPE_INVALID       0
#define EP_TYPE_ISOC_OUT      1
#define EP_TYPE_BULK_OUT      2
#define EP_TYPE_INTERRUPT_OUT 3
#define EP_TYPE_CONTROL       4
#define EP_TYPE_ISOC_IN       5
#define EP_TYPE_BULK_IN       6
#define EP_TYPE_INTERRUPT_IN  7

// Transfer Request Block (TRB)
typedef struct {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} __attribute__((packed)) xhci_trb_t;

// 传输环
typedef struct {
    xhci_trb_t *trbs;
    uint32_t    size;
    uint32_t    enqueue_index;
    uint32_t    dequeue_index;
    uint8_t     cycle_state;
    uint64_t    phys_addr; // 物理地址
} xhci_ring_t;

// 事件环段表项
typedef struct {
    uint64_t address;
    uint32_t size;
    uint32_t reserved;
} __attribute__((packed)) xhci_erst_entry_t;

// Slot上下文
typedef struct {
    uint32_t dev_info;
    uint32_t dev_info2;
    uint32_t tt_info;
    uint32_t dev_state;
    uint32_t reserved[4];
} __attribute__((packed)) xhci_slot_ctx_t;

// 端点上下文
typedef struct {
    uint32_t ep_info;
    uint32_t ep_info2;
    uint64_t tr_dequeue_ptr;
    uint32_t tx_info;
    uint32_t reserved[3];
} __attribute__((packed)) xhci_ep_ctx_t;

// 设备上下文
typedef struct {
    xhci_slot_ctx_t slot;
    xhci_ep_ctx_t   endpoints[31];
} __attribute__((packed)) xhci_device_ctx_t;

// 输入控制上下文
typedef struct {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t reserved[5];
    uint32_t config_value;
} __attribute__((packed)) xhci_input_ctrl_ctx_t;

// 输入上下文
typedef struct {
    xhci_input_ctrl_ctx_t ctrl;
    xhci_slot_ctx_t       slot;
    xhci_ep_ctx_t         endpoints[31];
} __attribute__((packed)) xhci_input_ctx_t;

// XHCI设备私有数据
typedef struct {
    uint8_t            slot_id;
    xhci_device_ctx_t *device_ctx;
    xhci_input_ctx_t  *input_ctx;
    uint64_t           device_ctx_phys;
    uint64_t           input_ctx_phys;
} xhci_device_private_t;

// XHCI端点私有数据
typedef struct {
    uint8_t      dci; // Device Context Index
    xhci_ring_t *transfer_ring;
} xhci_endpoint_private_t;

// 完成状态
typedef enum {
    COMPLETION_STATUS_PENDING = 0,
    COMPLETION_STATUS_SUCCESS = 1,
    COMPLETION_STATUS_ERROR   = 2,
    COMPLETION_STATUS_TIMEOUT = 3,
} completion_status_t;

// 命令完成结构
typedef struct {
    volatile completion_status_t status;
    uint32_t                     completion_code;
    uint64_t                     result;
    uint32_t                     slot_id;
    xhci_hcd_t                  *hcd;
    spin_t                       lock;
    uint64_t                     timeout;
} xhci_command_completion_t;

// 传输完成结构
typedef struct {
    volatile completion_status_t status;
    uint32_t                     completion_code;
    uint32_t                     transferred_length;
    usb_transfer_t              *transfer;
    enum {
        NORMAL_TRANSFER,
        INTR_TRANSFER
    } transfer_type;
    xhci_hcd_t *hcd;
    spin_t      lock;
} xhci_transfer_completion_t;

// 命令跟踪结构
typedef struct xhci_command_tracker {
    xhci_trb_t                  *trb;
    xhci_command_completion_t   *completion;
    uint32_t                     command_type;
    struct xhci_command_tracker *next;
} xhci_command_tracker_t;

// 传输跟踪结构
typedef struct xhci_transfer_tracker {
    xhci_trb_t                   *first_trb;
    int                           trb_num;
    xhci_transfer_completion_t   *completion;
    usb_transfer_t               *transfer;
    struct xhci_transfer_tracker *next;
} xhci_transfer_tracker_t;

// 事件处理线程数据
typedef struct {
    volatile bool running;
    xhci_hcd_t   *xhci;
} xhci_event_thread_t;

// XHCI传输私有数据
typedef struct {
    xhci_trb_t                 *first_trb;
    uint32_t                    num_trbs;
    xhci_transfer_completion_t *completion;
} xhci_transfer_private_t;

// 端口速度 ID (Port Speed ID)
#define XHCI_PSIV_FULL_SPEED  1
#define XHCI_PSIV_LOW_SPEED   2
#define XHCI_PSIV_HIGH_SPEED  3
#define XHCI_PSIV_SUPER_SPEED 4

// 端口协议
typedef enum {
    XHCI_PROTOCOL_USB2 = 2,
    XHCI_PROTOCOL_USB3 = 3,
} xhci_protocol_t;

// 端口信息
typedef struct {
    uint8_t         port_num;    // 端口号 (0-based)
    xhci_protocol_t protocol;    // USB2 or USB3
    uint8_t         port_offset; // 在协议中的偏移
    uint8_t         port_count;  // 该协议的端口数
    bool            connected;   // 是否有设备连接
    uint8_t         speed;       // 当前速度
} xhci_port_info_t;

// XHCI主控制器数据
struct xhci_hcd {
    usb_hcd_t *hcd;

    pci_device_t *pci_dev;

    xhci_cap_regs_t     *cap_regs;
    xhci_op_regs_t      *op_regs;
    xhci_port_regs_t    *port_regs;
    xhci_runtime_regs_t *runtime_regs;
    xhci_intr_regs_t    *intr_regs;
    uint32_t            *doorbell_regs;

    bool connection[256];

    xhci_port_info_t *port_info;

    uint8_t max_slots;
    uint8_t max_ports;
    uint8_t max_intrs;

    // 设备上下文基地址数组
    uint64_t *dcbaa;
    uint64_t  dcbaa_phys;

    // 命令环
    xhci_ring_t *cmd_ring;

    // 事件环
    xhci_ring_t       *event_ring;
    xhci_erst_entry_t *erst;
    uint64_t           erst_phys;

    // Scratchpad buffers
    uint64_t *scratchpad_array;
    void    **scratchpad_buffers;
    uint32_t  num_scratchpads;

    // 命令和传输跟踪
    xhci_command_tracker_t  *pending_commands;
    xhci_transfer_tracker_t *pending_transfers;
    spin_t                   tracker_mutex;

    // 事件处理线程
    xhci_event_thread_t event_thread;
};

// XHCI驱动API
usb_hcd_t *xhci_init(void *mmio_base, pci_device_t *pci_dev);
void       xhci_shutdown(usb_hcd_t *hcd);

// 内部函数
int xhci_reset(xhci_hcd_t *xhci);
int xhci_start(xhci_hcd_t *xhci);
int xhci_stop(xhci_hcd_t *xhci);

xhci_ring_t *xhci_alloc_ring(uint32_t num_trbs);
void         xhci_free_ring(xhci_ring_t *ring);
xhci_trb_t  *xhci_queue_trb(xhci_ring_t *ring, uint64_t param, uint32_t status, uint32_t control);
void         xhci_ring_doorbell(xhci_hcd_t *xhci, uint8_t slot_id, uint8_t dci);

void xhci_handle_events(xhci_hcd_t *xhci);
void xhci_handle_port_status(xhci_hcd_t *xhci, uint8_t port_id);

xhci_command_completion_t *xhci_alloc_command_completion(void);
void                       xhci_free_command_completion(xhci_command_completion_t *completion);
int xhci_wait_for_command(xhci_command_completion_t *completion, uint32_t timeout_ms);

xhci_transfer_completion_t *xhci_alloc_transfer_completion(void);
void                        xhci_free_transfer_completion(xhci_transfer_completion_t *completion);
int xhci_wait_for_transfer(xhci_transfer_completion_t *completion, uint32_t timeout_ms);

void xhci_track_command(xhci_hcd_t *xhci, xhci_trb_t *trb, xhci_command_completion_t *completion,
                        uint32_t cmd_type);
void xhci_track_transfer(xhci_hcd_t *xhci, xhci_trb_t *first_trb, int trb_num,
                         xhci_transfer_completion_t *completion, usb_transfer_t *transfer);

void xhci_complete_command(xhci_hcd_t *xhci, xhci_trb_t *event_trb);
void xhci_complete_transfer(xhci_hcd_t *xhci, xhci_trb_t *event_trb);

int  xhci_start_event_handler(xhci_hcd_t *xhci);
void xhci_stop_event_handler(xhci_hcd_t *xhci);

// 获取TRB物理地址
static inline uint64_t xhci_trb_phys(xhci_ring_t *ring, xhci_trb_t *trb) {
    size_t offset = trb - ring->trbs;
    return ring->phys_addr + (offset * sizeof(xhci_trb_t));
}

// 辅助函数
static inline uint32_t xhci_readl(volatile uint32_t *reg) {
    return *reg;
}

static inline void xhci_writel(volatile uint32_t *reg, uint32_t value) {
    *reg = value;
}

static inline uint64_t xhci_readq(volatile uint64_t *reg) {
    return *reg;
}

static inline void xhci_writeq(volatile uint64_t *reg, uint64_t value) {
    *reg = value;
}

static inline int xhci_calc_trbs_num(int new_index, int first_index, uint32_t ring_size) {
    return new_index - first_index;
}
