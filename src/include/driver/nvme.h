#pragma once

// NVMe 寄存器偏移 (Controller Registers)
#define NVME_REG_CAP 0x00   // Controller Capabilities
#define NVME_REG_VS 0x08    // Version
#define NVME_REG_INTMS 0x0C // Interrupt Mask Set
#define NVME_REG_INTMC 0x10 // Interrupt Mask Clear
#define NVME_REG_CC 0x14    // Controller Configuration
#define NVME_REG_CSTS 0x1C  // Controller Status
#define NVME_REG_AQA 0x24   // Admin Queue Attributes
#define NVME_REG_ASQ 0x28   // Admin Submission Queue
#define NVME_REG_ACQ 0x30   // Admin Completion Queue

// Doorbell registers (stride calculated from CAP)
#define NVME_REG_DBS 0x1000

// Controller Configuration bits
#define NVME_CC_ENABLE (1 << 0)
#define NVME_CC_CSS_NVM (0 << 4)
#define NVME_CC_MPS_SHIFT 7
#define NVME_CC_AMS_RR (0 << 11)
#define NVME_CC_SHN_NONE (0 << 14)
#define NVME_CC_SHN_NORMAL (1 << 14)
#define NVME_CC_IOSQES (6 << 16) // 2^6 = 64 bytes
#define NVME_CC_IOCQES (4 << 20) // 2^4 = 16 bytes

// Controller Status bits
#define NVME_CSTS_RDY (1 << 0)
#define NVME_CSTS_CFS (1 << 1)
#define NVME_CSTS_SHST_MASK (3 << 2)
#define NVME_CSTS_SHST_NORMAL (0 << 2)

// Admin Commands
#define NVME_ADMIN_DELETE_SQ 0x00
#define NVME_ADMIN_CREATE_SQ 0x01
#define NVME_ADMIN_DELETE_CQ 0x04
#define NVME_ADMIN_CREATE_CQ 0x05
#define NVME_ADMIN_IDENTIFY 0x06
#define NVME_ADMIN_SET_FEATURES 0x09
#define NVME_ADMIN_GET_FEATURES 0x0A

// NVM Commands
#define NVME_CMD_FLUSH 0x00
#define NVME_CMD_WRITE 0x01
#define NVME_CMD_READ 0x02

// Queue sizes
#define NVME_ADMIN_QUEUE_SIZE 64
#define NVME_IO_QUEUE_SIZE 256

#define NVME_PAGE_SIZE 4096
#define NVME_PAGE_MASK (NVME_PAGE_SIZE - 1)
#define NVME_PRP_ENTRY_SIZE 8
#define NVME_MAX_PRP_LIST_ENTRIES (NVME_PAGE_SIZE / NVME_PRP_ENTRY_SIZE) // 512 entries

#include "driver/pci/pci.h"
#include "lock.h"
#include "types.h"

// NVMe Submission Queue Entry
typedef struct {
    uint32_t cdw0; // Command Dword 0 (Opcode, Flags, CID)
    uint32_t nsid; // Namespace ID
    uint64_t rsvd;
    uint64_t mptr; // Metadata Pointer
    uint64_t prp1; // PRP Entry 1
    uint64_t prp2; // PRP Entry 2
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed)) nvme_sqe_t;

// NVMe Completion Queue Entry
typedef struct {
    uint32_t dw0;     // Command-specific
    uint32_t dw1;     // Reserved
    uint16_t sq_head; // SQ Head Pointer
    uint16_t sq_id;   // SQ Identifier
    uint16_t cid;     // Command Identifier
    uint16_t status;  // Status Field (phase bit + status code)
} __attribute__((packed)) nvme_cqe_t;

// NVMe Queue Pair
typedef struct {
    struct nvme_controller *ctrl;

    spin_t lock;

    uint64_t vector;

    nvme_sqe_t *sq; // Submission Queue
    nvme_cqe_t *cq; // Completion Queue

    uint64_t sq_phys; // Physical address of SQ
    uint64_t cq_phys; // Physical address of CQ

    uint16_t sq_head;  // SQ head pointer
    uint16_t sq_tail;  // SQ tail pointer
    uint16_t cq_head;  // CQ head pointer
    uint16_t cq_phase; // CQ phase bit

    uint16_t queue_id;
    uint16_t queue_depth;

    volatile uint32_t *sq_doorbell;
    volatile uint32_t *cq_doorbell;
} nvme_queue_t;

// NVMe Identify Controller Data Structure (4096 bytes)
typedef struct {
    // Controller Capabilities and Features (Bytes 0-255)
    uint16_t vid;         // 0: PCI Vendor ID
    uint16_t ssvid;       // 2: PCI Subsystem Vendor ID
    char sn[20];          // 4: Serial Number
    char mn[40];          // 24: Model Number
    char fr[8];           // 64: Firmware Revision
    uint8_t rab;          // 72: Recommended Arbitration Burst
    uint8_t ieee[3];      // 73: IEEE OUI Identifier
    uint8_t cmic;         // 76: Controller Multi-Path I/O and Namespace Sharing
                          // Capabilities
    uint8_t mdts;         // 77: Maximum Data Transfer Size
    uint16_t cntlid;      // 78: Controller ID
    uint32_t ver;         // 80: Version
    uint32_t rtd3r;       // 84: RTD3 Resume Latency
    uint32_t rtd3e;       // 88: RTD3 Entry Latency
    uint32_t oaes;        // 92: Optional Asynchronous Events Supported
    uint32_t ctratt;      // 96: Controller Attributes
    uint16_t rrls;        // 100: Read Recovery Levels Supported
    uint8_t rsvd102[9];   // 102-110: Reserved
    uint8_t cntrltype;    // 111: Controller Type
    uint8_t fguid[16];    // 112: FRU Globally Unique Identifier
    uint16_t crdt1;       // 128: Command Retry Delay Time 1
    uint16_t crdt2;       // 130: Command Retry Delay Time 2
    uint16_t crdt3;       // 132: Command Retry Delay Time 3
    uint8_t rsvd134[122]; // 134-255: Reserved

    // Admin Command Set Attributes (Bytes 256-511)
    uint16_t oacs;        // 256: Optional Admin Command Support
    uint8_t acl;          // 258: Abort Command Limit
    uint8_t aerl;         // 259: Asynchronous Event Request Limit
    uint8_t frmw;         // 260: Firmware Updates
    uint8_t lpa;          // 261: Log Page Attributes
    uint8_t elpe;         // 262: Error Log Page Entries
    uint8_t npss;         // 263: Number of Power States Support
    uint8_t avscc;        // 264: Admin Vendor Specific Command Configuration
    uint8_t apsta;        // 265: Autonomous Power State Transition Attributes
    uint16_t wctemp;      // 266: Warning Composite Temperature Threshold
    uint16_t cctemp;      // 268: Critical Composite Temperature Threshold
    uint16_t mtfa;        // 270: Maximum Time for Firmware Activation
    uint32_t hmpre;       // 272: Host Memory Buffer Preferred Size
    uint32_t hmmin;       // 276: Host Memory Buffer Minimum Size
    uint8_t tnvmcap[16];  // 280: Total NVM Capacity
    uint8_t unvmcap[16];  // 296: Unallocated NVM Capacity
    uint32_t rpmbs;       // 312: Replay Protected Memory Block Support
    uint16_t edstt;       // 316: Extended Device Self-test Time
    uint8_t dsto;         // 318: Device Self-test Options
    uint8_t fwug;         // 319: Firmware Update Granularity
    uint16_t kas;         // 320: Keep Alive Support
    uint16_t hctma;       // 322: Host Controlled Thermal Management Attributes
    uint16_t mntmt;       // 324: Minimum Thermal Management Temperature
    uint16_t mxtmt;       // 326: Maximum Thermal Management Temperature
    uint32_t sanicap;     // 328: Sanitize Capabilities
    uint32_t hmminds;     // 332: Host Memory Buffer Minimum Descriptor Entry Size
    uint16_t hmmaxd;      // 336: Host Memory Maximum Descriptors Entries
    uint16_t nsetidmax;   // 338: NVM Set Identifier Maximum
    uint16_t endgidmax;   // 340: Endurance Group Identifier Maximum
    uint8_t anatt;        // 342: ANA Transition Time
    uint8_t anacap;       // 343: Asymmetric Namespace Access Capabilities
    uint32_t anagrpmax;   // 344: ANA Group Identifier Maximum
    uint32_t nanagrpid;   // 348: Number of ANA Group Identifiers
    uint32_t pels;        // 352: Persistent Event Log Size
    uint16_t domain_id;   // 356: Domain Identifier
    uint8_t rsvd358[10];  // 358-367: Reserved
    uint8_t megcap[16];   // 368: Max Endurance Group Capacity
    uint8_t rsvd384[128]; // 384-511: Reserved

    // NVM Command Set Attributes (Bytes 512-703)
    uint8_t sqes;         // 512: Submission Queue Entry Size
    uint8_t cqes;         // 513: Completion Queue Entry Size
    uint16_t maxcmd;      // 514: Maximum Outstanding Commands
    uint32_t nn;          // 516: Number of Namespaces *** 这是关键字段 ***
    uint16_t oncs;        // 520: Optional NVM Command Support
    uint16_t fuses;       // 522: Fused Operation Support
    uint8_t fna;          // 524: Format NVM Attributes
    uint8_t vwc;          // 525: Volatile Write Cache
    uint16_t awun;        // 526: Atomic Write Unit Normal
    uint16_t awupf;       // 528: Atomic Write Unit Power Fail
    uint8_t nvscc;        // 530: NVM Vendor Specific Command Configuration
    uint8_t nwpc;         // 531: Namespace Write Protection Capabilities
    uint16_t acwu;        // 532: Atomic Compare & Write Unit
    uint16_t rsvd534;     // 534: Reserved
    uint32_t sgls;        // 536: SGL Support
    uint32_t mnan;        // 540: Maximum Number of Allowed Namespaces
    uint8_t maxdna[16];   // 544: Maximum Domain Namespace Attachments
    uint32_t maxcna;      // 560: Maximum I/O Controller Namespace Attachments
    uint8_t rsvd564[140]; // 564-703: Reserved

    // I/O Command Set Attributes (Bytes 704-2047)
    uint8_t rsvd704[1344]; // 704-2047: Reserved

    // Power State Descriptors (Bytes 2048-3071)
    uint8_t psd[1024]; // 2048-3071: Power State Descriptors (32 * 32 bytes)

    // Vendor Specific (Bytes 3072-4095)
    uint8_t vs[1024]; // 3072-4095: Vendor Specific
} __attribute__((packed)) nvme_identify_ctrl_t;

// NVMe Identify Namespace Data Structure (4096 bytes)
typedef struct {
    uint64_t nsze;      // 0: Namespace Size (in logical blocks)
    uint64_t ncap;      // 8: Namespace Capacity
    uint64_t nuse;      // 16: Namespace Utilization
    uint8_t nsfeat;     // 24: Namespace Features
    uint8_t nlbaf;      // 25: Number of LBA Formats (0's based)
    uint8_t flbas;      // 26: Formatted LBA Size
    uint8_t mc;         // 27: Metadata Capabilities
    uint8_t dpc;        // 28: End-to-end Data Protection Capabilities
    uint8_t dps;        // 29: End-to-end Data Protection Type Settings
    uint8_t nmic;       // 30: Namespace Multi-path I/O and Namespace Sharing Capabilities
    uint8_t rescap;     // 31: Reservation Capabilities
    uint8_t fpi;        // 32: Format Progress Indicator
    uint8_t dlfeat;     // 33: Deallocate Logical Block Features
    uint16_t nawun;     // 34: Namespace Atomic Write Unit Normal
    uint16_t nawupf;    // 36: Namespace Atomic Write Unit Power Fail
    uint16_t nacwu;     // 38: Namespace Atomic Compare & Write Unit
    uint16_t nabsn;     // 40: Namespace Atomic Boundary Size Normal
    uint16_t nabo;      // 42: Namespace Atomic Boundary Offset
    uint16_t nabspf;    // 44: Namespace Atomic Boundary Size Power Fail
    uint16_t noiob;     // 46: Namespace Optimal I/O Boundary
    uint8_t nvmcap[16]; // 48: NVM Capacity
    uint16_t npwg;      // 64: Namespace Preferred Write Granularity
    uint16_t npwa;      // 66: Namespace Preferred Write Alignment
    uint16_t npdg;      // 68: Namespace Preferred Deallocate Granularity
    uint16_t npda;      // 70: Namespace Preferred Deallocate Alignment
    uint16_t nows;      // 72: Namespace Optimal Write Size
    uint16_t mssrl;     // 74: Maximum Single Source Range Length
    uint32_t mcl;       // 76: Maximum Copy Length
    uint8_t msrc;       // 80: Maximum Source Range Count
    uint8_t rsvd81[11]; // 81-91: Reserved
    uint32_t anagrpid;  // 92: ANA Group Identifier
    uint8_t rsvd96[3];  // 96-98: Reserved
    uint8_t nsattr;     // 99: Namespace Attributes
    uint16_t nvmsetid;  // 100: NVM Set Identifier
    uint16_t endgid;    // 102: Endurance Group Identifier
    uint8_t nguid[16];  // 104: Namespace Globally Unique Identifier
    uint8_t eui64[8];   // 120: IEEE Extended Unique Identifier

    // LBA Format Support (Bytes 128-191)
    struct {
        uint16_t ms;   // Metadata Size
        uint8_t lbads; // LBA Data Size (power of 2)
        uint8_t rp;    // Relative Performance
    } lbaf[16];        // 128-191: LBA Format 0-15 Support

    uint8_t rsvd192[192]; // 192-383: Reserved
    uint8_t vs[3712];     // 384-4095: Vendor Specific
} __attribute__((packed)) nvme_identify_ns_t;

// I/O Request callback
typedef void (*nvme_io_callback_t)(void *ctx, bool success, uint32_t result);

// I/O Request
typedef struct nvme_request {
    uint16_t cid; // Command ID
    nvme_io_callback_t callback;
    void *ctx;
    struct nvme_request *next;
} nvme_request_t;

// NVMe Namespace
typedef struct {
    uint32_t nsid;
    uint64_t block_count;
    uint32_t block_size;
    bool valid;
} nvme_namespace_t;

// PRP List 结构
typedef struct {
    uint64_t prp[NVME_MAX_PRP_LIST_ENTRIES];
} __attribute__((packed)) nvme_prp_list_t;

// NVMe Controller
typedef struct nvme_controller {
    pci_device_t *pci_dev;
    volatile uint8_t *bar0; // Controller registers (MMIO)

    uint32_t doorbell_stride;    // in bytes
    uint32_t max_transfer_size;  // in bytes
    uint32_t max_transfer_pages; // in pages
    uint32_t page_size;          // Controller page size

    nvme_queue_t admin_queue;
    nvme_queue_t io_queues[8]; // Support up to 16 I/O queue pairs
    uint32_t num_io_queues;

    spin_t cid_alloc_lock;
    nvme_request_t *requests[65536]; // Track requests by CID

    nvme_prp_list_t *prp_list_pool;
    uint64_t prp_list_pool_phys;
    uint32_t prp_list_pool_size;
    uint32_t prp_list_next_free;

    nvme_namespace_t namespaces[256];
    uint32_t num_namespaces;

    bool initialized;

    // Abstraction layer pointers (for portability)
    void *platform_data;
} nvme_controller_t;

// Platform abstraction functions (to be implemented by platform)
typedef struct {
    // Memory allocation (DMA-capable)
    void *(*dma_alloc)(size_t size, uint64_t *phys_addr);
    void (*dma_free)(void *virt, size_t size);

    // Memory barriers
    void (*mb)(void);  // Memory barrier
    void (*rmb)(void); // Read memory barrier
    void (*wmb)(void); // Write memory barrier

    // Timing
    void (*udelay)(uint32_t us);   // Microsecond delay
    uint64_t (*get_time_ms)(void); // Get time in milliseconds

    // Locking (for multi-threaded environments)
    void *(*mutex_create)(void);
    void (*mutex_lock)(void *mutex);
    void (*mutex_unlock)(void *mutex);
    void (*mutex_destroy)(void *mutex);

    // Logging
    int (*log)(const char *fmt, ...);
} nvme_platform_ops_t;

// Global platform operations
extern nvme_platform_ops_t *g_nvme_platform_ops;

// Public API
void nvme_probe(pci_device_t *device);
int nvme_read_async(
    nvme_controller_t *ctrl, uint32_t nsid, uint64_t lba, uint32_t block_count, void *buffer,
    uint64_t buffer_phys, nvme_io_callback_t callback, void *ctx);
int nvme_write_async(
    nvme_controller_t *ctrl, uint32_t nsid, uint64_t lba, uint32_t block_count, const void *buffer,
    uint64_t buffer_phys, nvme_io_callback_t callback, void *ctx);
void nvme_process_completions(nvme_controller_t *ctrl);
int nvme_get_namespace_info(
    nvme_controller_t *ctrl, uint32_t nsid, uint64_t *block_count, uint32_t *block_size);
void nvme_setup();
