#pragma once

#define NVME_REG_CAP   0x00
#define NVME_REG_VS    0x08
#define NVME_REG_INTMS 0x0C
#define NVME_REG_INTMC 0x10
#define NVME_REG_CC    0x14
#define NVME_REG_CSTS  0x1C
#define NVME_REG_AQA   0x24
#define NVME_REG_ASQ   0x28
#define NVME_REG_ACQ   0x30

#define NVME_REG_DBS 0x1000

#define NVME_CC_ENABLE     (1 << 0)
#define NVME_CC_CSS_NVM    (0 << 4)
#define NVME_CC_MPS_SHIFT  7
#define NVME_CC_AMS_RR     (0 << 11)
#define NVME_CC_SHN_NONE   (0 << 14)
#define NVME_CC_SHN_NORMAL (1 << 14)
#define NVME_CC_IOSQES     (6 << 16)
#define NVME_CC_IOCQES     (4 << 20)

#define NVME_CSTS_RDY         (1 << 0)
#define NVME_CSTS_CFS         (1 << 1)
#define NVME_CSTS_SHST_MASK   (3 << 2)
#define NVME_CSTS_SHST_NORMAL (0 << 2)

#define NVME_ADMIN_DELETE_SQ    0x00
#define NVME_ADMIN_CREATE_SQ    0x01
#define NVME_ADMIN_DELETE_CQ    0x04
#define NVME_ADMIN_CREATE_CQ    0x05
#define NVME_ADMIN_IDENTIFY     0x06
#define NVME_ADMIN_SET_FEATURES 0x09
#define NVME_ADMIN_GET_FEATURES 0x0A

#define NVME_CMD_FLUSH 0x00
#define NVME_CMD_WRITE 0x01
#define NVME_CMD_READ  0x02

#define NVME_ADMIN_QUEUE_SIZE 64
#define NVME_IO_QUEUE_SIZE    256

#define NVME_PAGE_SIZE            4096
#define NVME_PAGE_MASK            (NVME_PAGE_SIZE - 1)
#define NVME_PRP_ENTRY_SIZE       8
#define NVME_MAX_PRP_LIST_ENTRIES (NVME_PAGE_SIZE / NVME_PRP_ENTRY_SIZE)

#define NVME_MAX_REQUESTS   256
#define NVME_MAX_NAMESPACES 256
#define NVME_MAX_IO_QUEUES  1

#include "driver_subsystem.h"
#include "lock.h"

typedef struct {
    uint32_t cdw0;
    uint32_t nsid;
    uint64_t rsvd;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed)) nvme_sqe_t;

typedef struct {
    uint32_t dw0;
    uint32_t dw1;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t cid;
    uint16_t status;
} __attribute__((packed)) nvme_cqe_t;

typedef struct {
    struct nvme_controller *ctrl;

    spin_t lock;

    nvme_sqe_t *sq;
    nvme_cqe_t *cq;

    uint64_t sq_phys;
    uint64_t cq_phys;

    uint16_t sq_head;
    uint16_t sq_tail;
    uint16_t cq_head;
    uint16_t cq_phase;

    uint16_t queue_id;
    uint16_t queue_depth;

    volatile uint32_t *sq_doorbell;
    volatile uint32_t *cq_doorbell;
} nvme_queue_t;

typedef struct {
    uint16_t vid;
    uint16_t ssvid;
    char sn[20];
    char mn[40];
    char fr[8];
    uint8_t rab;
    uint8_t ieee[3];
    uint8_t cmic;
    uint8_t mdts;
    uint16_t cntlid;
    uint32_t ver;
    uint32_t rtd3r;
    uint32_t rtd3e;
    uint32_t oaes;
    uint32_t ctratt;
    uint16_t rrls;
    uint8_t rsvd102[9];
    uint8_t cntrltype;
    uint8_t fguid[16];
    uint16_t crdt1;
    uint16_t crdt2;
    uint16_t crdt3;
    uint8_t rsvd134[122];

    uint16_t oacs;
    uint8_t acl;
    uint8_t aerl;
    uint8_t frmw;
    uint8_t lpa;
    uint8_t elpe;
    uint8_t npss;
    uint8_t avscc;
    uint8_t apsta;
    uint16_t wctemp;
    uint16_t cctemp;
    uint16_t mtfa;
    uint32_t hmpre;
    uint32_t hmmin;
    uint8_t tnvmcap[16];
    uint8_t unvmcap[16];
    uint32_t rpmbs;
    uint16_t edstt;
    uint8_t dsto;
    uint8_t fwug;
    uint16_t kas;
    uint16_t hctma;
    uint16_t mntmt;
    uint16_t mxtmt;
    uint32_t sanicap;
    uint32_t hmminds;
    uint16_t hmmaxd;
    uint16_t nsetidmax;
    uint16_t endgidmax;
    uint8_t anatt;
    uint8_t anacap;
    uint32_t anagrpmax;
    uint32_t nanagrpid;
    uint32_t pels;
    uint16_t domain_id;
    uint8_t rsvd358[10];
    uint8_t megcap[16];
    uint8_t rsvd384[128];

    uint8_t sqes;
    uint8_t cqes;
    uint16_t maxcmd;
    uint32_t nn;
    uint16_t oncs;
    uint16_t fuses;
    uint8_t fna;
    uint8_t vwc;
    uint16_t awun;
    uint16_t awupf;
    uint8_t nvscc;
    uint8_t nwpc;
    uint16_t acwu;
    uint16_t rsvd534;
    uint32_t sgls;
    uint32_t mnan;
    uint8_t maxdna[16];
    uint32_t maxcna;
    uint8_t rsvd564[140];

    uint8_t rsvd704[1344];
    uint8_t psd[1024];
    uint8_t vs[1024];
} __attribute__((packed)) nvme_identify_ctrl_t;

typedef struct {
    uint64_t nsze;
    uint64_t ncap;
    uint64_t nuse;
    uint8_t nsfeat;
    uint8_t nlbaf;
    uint8_t flbas;
    uint8_t mc;
    uint8_t dpc;
    uint8_t dps;
    uint8_t nmic;
    uint8_t rescap;
    uint8_t fpi;
    uint8_t dlfeat;
    uint16_t nawun;
    uint16_t nawupf;
    uint16_t nacwu;
    uint16_t nabsn;
    uint16_t nabo;
    uint16_t nabspf;
    uint16_t noiob;
    uint8_t nvmcap[16];
    uint16_t npwg;
    uint16_t npwa;
    uint16_t npdg;
    uint16_t npda;
    uint16_t nows;
    uint16_t mssrl;
    uint32_t mcl;
    uint8_t msrc;
    uint8_t rsvd81[11];
    uint32_t anagrpid;
    uint8_t rsvd96[3];
    uint8_t nsattr;
    uint16_t nvmsetid;
    uint16_t endgid;
    uint8_t nguid[16];
    uint8_t eui64[8];

    struct {
        uint16_t ms;
        uint8_t lbads;
        uint8_t rp;
    } lbaf[16];

    uint8_t rsvd192[192];
    uint8_t vs[3712];
} __attribute__((packed)) nvme_identify_ns_t;

typedef void (*nvme_io_callback_t)(void *ctx, bool success, uint32_t result);

typedef struct nvme_prp_list {
    uint64_t prp[NVME_MAX_PRP_LIST_ENTRIES];
} __attribute__((packed)) nvme_prp_list_t;

typedef struct nvme_request {
    uint16_t cid;
    nvme_io_callback_t callback;
    void *ctx;
    nvme_prp_list_t *prp_list;
    uint64_t prp_list_phys;
    struct nvme_request *next;
} nvme_request_t;

typedef struct {
    uint32_t nsid;
    uint64_t block_count;
    uint32_t block_size;
    bool valid;
} nvme_namespace_t;

typedef struct nvme_controller {
    pci_device_t *pci_dev;
    volatile uint8_t *bar0;

    uint32_t doorbell_stride;
    uint32_t max_transfer_size;
    uint32_t max_transfer_pages;
    uint32_t page_size;

    nvme_queue_t admin_queue;
    nvme_queue_t io_queues[NVME_MAX_IO_QUEUES];
    uint32_t num_io_queues;

    spin_t cid_alloc_lock;
    uint16_t cid_alloc_pos;
    nvme_request_t *requests[NVME_MAX_REQUESTS];
    nvme_request_t request_slots[NVME_MAX_REQUESTS];

    nvme_namespace_t namespaces[NVME_MAX_NAMESPACES];
    uint32_t num_namespaces;

    bool initialized;
    void *platform_data;
} nvme_controller_t;

typedef struct {
    void *(*dma_alloc)(size_t size, uint64_t *phys_addr);
    void (*dma_free)(void *virt, size_t size);

    void (*mb)(void);
    void (*rmb)(void);
    void (*wmb)(void);

    void (*udelay)(uint32_t us);
    uint64_t (*get_time_ms)(void);

    void *(*mutex_create)(void);
    void (*mutex_lock)(void *mutex);
    void (*mutex_unlock)(void *mutex);
    void (*mutex_destroy)(void *mutex);

    int (*log)(const char *fmt, ...);
} nvme_platform_ops_t;

typedef struct {
    nvme_controller_t *ctrl;
    nvme_namespace_t *ns;
} nvme_ns_t;

typedef struct {
    bool done;
    bool success;
    uint32_t result;
    volatile uint32_t refs;
} admin_sync_ctx_t;

typedef struct {
    bool completed;
    bool success;
    uint32_t result;
    volatile uint32_t refs;
} nvme_callback_ctx_t;

extern nvme_platform_ops_t *g_nvme_platform_ops;

void nvme_probe(pci_device_t *device);
int nvme_read_async(
    nvme_controller_t *ctrl,
    uint32_t nsid,
    uint64_t lba,
    uint32_t block_count,
    void *buffer,
    uint64_t buffer_phys,
    nvme_io_callback_t callback,
    void *ctx
);
int nvme_write_async(
    nvme_controller_t *ctrl,
    uint32_t nsid,
    uint64_t lba,
    uint32_t block_count,
    const void *buffer,
    uint64_t buffer_phys,
    nvme_io_callback_t callback,
    void *ctx
);
void nvme_process_completions(nvme_controller_t *ctrl);
int nvme_get_namespace_info(
    nvme_controller_t *ctrl, uint32_t nsid, uint64_t *block_count, uint32_t *block_size
);
