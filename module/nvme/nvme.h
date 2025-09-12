#pragma once

#define NVME_CSTS_FATAL (1U << 1)
#define NVME_CSTS_RDY   (1U << 0)

#define NVME_SQE_OPC_ADMIN_CREATE_IO_SQ 1U
#define NVME_SQE_OPC_ADMIN_CREATE_IO_CQ 5U
#define NVME_SQE_OPC_ADMIN_IDENTIFY     6U

#define NVME_SQE_OPC_IO_WRITE 1U
#define NVME_SQE_OPC_IO_READ  2U

#define NVME_ADMIN_IDENTIFY_CNS_ID_NS   0x00U
#define NVME_ADMIN_IDENTIFY_CNS_ID_CTRL 0x01U
#define NVME_ADMIN_IDENTIFY_CNS_ACT_NSL 0x02U

#include "cp_kernel.h"
#include "errno.h"

typedef struct _NVME_CONTROLLER NVME_CONTROLLER;
typedef struct _NVME_CAPABILITY {
    uint64_t CAP; // Controller Capabilities
    uint32_t VER; // Version
    uint32_t IMS; // Interrupt Mask Set
    uint32_t IMC; // Interrupt Mask Clear
    uint32_t CC;  // Controller Configuration
    uint32_t RSV0;
    uint32_t CST; // Controller Status
    uint32_t RSV1;
    uint32_t AQA; // Admin Queue Attributes
    uint64_t ASQ; // Admin Submission Queue
    uint64_t ACQ; // Admin Completion Queue
} NVME_CAPABILITY;

typedef struct _NVME_SUBMISSION_QUEUE_ENTRY {
    uint32_t CDW0;
    uint32_t NSID; // Namespace ID
    uint32_t CDW2;
    uint32_t CDW3;
    uint64_t META;    // Metadata
    uint64_t DATA[2]; // Data
    uint32_t CDWA;
    uint32_t CDWB;
    uint32_t CDWC;
    uint32_t CDWD;
    uint32_t CDWE;
    uint32_t CDWF;
} NVME_SUBMISSION_QUEUE_ENTRY;

typedef struct _NVME_COMPLETION_QUEUE_ENTRY {
    uint32_t DW0;
    uint32_t DW1;
    uint16_t QHD; // SQ Head Pointer
    uint16_t QID; // SQ ID
    uint16_t CID; // Command ID
    uint16_t STS; // Status
} NVME_COMPLETION_QUEUE_ENTRY;

typedef struct _NVME_QUEUE_COMMON {
    NVME_CONTROLLER *CTR;
    uint32_t        *DBL;
    uint16_t         MSK;
    uint16_t         HAD;
    uint16_t         TAL;
    uint16_t         PHA;
} NVME_QUEUE_COMMON;

typedef struct _NVME_COMPLETION_QUEUE {
    NVME_QUEUE_COMMON            COM;
    NVME_COMPLETION_QUEUE_ENTRY *CQE;
} NVME_COMPLETION_QUEUE;

typedef struct _NVME_SUBMISSION_QUEUE {
    NVME_QUEUE_COMMON            COM;
    NVME_SUBMISSION_QUEUE_ENTRY *SQE;
    NVME_COMPLETION_QUEUE       *ICQ;
} NVME_SUBMISSION_QUEUE;

typedef struct _NVME_CONTROLLER {
    NVME_CAPABILITY      *CAP;
    NVME_COMPLETION_QUEUE ACQ;
    NVME_SUBMISSION_QUEUE ASQ;
    NVME_COMPLETION_QUEUE ICQ;
    NVME_SUBMISSION_QUEUE ISQ;
    uint64_t              DST; // Doorbell Stride
    uint32_t              WTO; // Waiting Timeout (ms)
    uint32_t              NSC; // Namespace Count
    uint8_t               SER[0x14];
    uint8_t               MOD[0x28];
} NVME_CONTROLLER;

typedef struct _NVME_LOGICAL_BLOCK_ADDRESS {
    uint16_t MS; // Metadata Size
    uint8_t  DS; // LBA Data Size
    uint8_t  RP; // Relative Performance
} NVME_LOGICAL_BLOCK_ADDRESS;

typedef struct _NVME_IDENTIFY_NAMESPACE {
    uint64_t                   SIZE; // Namespace Size
    uint64_t                   CAPA; // Namespace Capacity
    uint64_t                   UTIL; // Namespace Utilization
    uint8_t                    FEAT; // Namespace Features
    uint8_t                    NLBA; // Number of LBA Format
    uint8_t                    FLBA; // Formatted LBA size
    uint8_t                    META; // Metadata Capability
    uint8_t                    DPCA; // End-to-end Data Protection Capability
    uint8_t                    DPTS; // End-to-end Data Protection Type Settings
    uint8_t                    NMIC; // Namespace Multi-path I/O and Namespace Sharing Capabilities
    uint8_t                    RSVC; // Reservation Capabilities
    uint8_t                    FPID; // Format Prograss Indicator
    uint8_t                    DLBF; // Deallocate Logical Block Feature
    uint16_t                   AWUN; // Namespace Atomic Write Unit Normal
    uint16_t                   AWUP; // Namespace Atomic Write Unit Power Fail
    uint16_t                   ACWU; // Namespace Atomic Compare & Write Unit
    uint16_t                   ABSN; // Namespace Atomic Boundary Size Normal
    uint16_t                   ABOS; // Namespace Atomic Boundary Offset
    uint16_t                   ABSP; // Namespace Atomic Boundary Size Power Fail
    uint16_t                   OIOB; // Namespace Optimal I/O Boundary
    uint8_t                    NVMC[16]; // NVM Capacity
    uint16_t                   NPWG;     // Namespace Preferred Write Granularity
    uint16_t                   NPWA;     // Namespace Preferred Write Alignment
    uint16_t                   NPDG;     // Namespace Preferred Deallocate Granularity
    uint16_t                   NPDA;     // Namespace Preferred Deallocate Alignment
    uint16_t                   NOWS;     // Namespace Optimal Write Size
    uint16_t                   SSRL;     // Maximum Single Source Range length
    uint32_t                   MXCL;     // Maximum Copy length
    uint8_t                    MSRC;     // Maximum Source Range Count (MSRC)
    uint8_t                    RSV0[11];
    uint32_t                   AGID; // ANA Group Identifier
    uint8_t                    RSV1[3];
    uint8_t                    ATTR;    // Namespace Attributes
    uint16_t                   NSID;    // NVM Set Identifier
    uint16_t                   EGID;    // Endurance Group Identifier
    uint64_t                   GUID[2]; // Namespace Globally Unique Identifier
    uint64_t                   XUID;    // IEEE Extended Unique Identifier
    NVME_LOGICAL_BLOCK_ADDRESS LBAF[64];
} NVME_IDENTIFY_NAMESPACE;

typedef struct _NVME_IDENTIFY_CONTROLLER {
    uint16_t PVID;     // PCI Vendor ID
    uint16_t PSID;     // PCI Subsystem Vendor ID
    uint8_t  SERN[20]; // Serial Number
    uint8_t  MODN[40]; // Model Number
    uint8_t  FREV[8];  // Frimware revision
    uint8_t  RCAB;     // Recommended Arbitration Burst
    uint8_t  IEEE[3];  // IEEE OUI Identifier
    uint8_t  CMIC;     // Controller Multi-Path I/O and Namespace Sharing Capabilities
    uint8_t  MDTS;     // Maximum Data Transfer Size
    uint16_t CTID;     // Controller ID
    uint32_t VERS;     // Version
    uint32_t RTDR;     // RTD3 Resume Latency
    uint32_t RTDE;     // RTD3 Entry Latency
    uint32_t OAES;     // Optional Asynchronous Events Supported
    uint32_t CRTA;     // Controller Attributes
    uint16_t RRLS;     // Read Recovery Levels Supported
    uint8_t  RSV0[9];
    uint8_t  CTRT;    // Controller Type
    uint64_t GUID[2]; // FRU Globally Unique Identifier
    uint16_t CRD1;    // Command Retry Delay Time 1
    uint16_t CRD2;    // Command Retry Delay Time 2
    uint16_t CRD3;    // Command Retry Delay Time 3
    uint8_t  RSV1[119];
    uint8_t  NVMR;    // NVM Subsystem Report
    uint8_t  VWCI;    // VPD Write Cycle Information
    uint8_t  MECA;    // Management Endpoint Capabilities
    uint16_t OACS;    // Optional Admin Command Support
    uint8_t  ACLM;    // Abort Command Limit
    uint8_t  AERL;    // Asynchronous Event Request Limit
    uint8_t  FRMW;    // Firmware Updates
    uint8_t  LPGA;    // Log Page Attributes
    uint8_t  ELPE;    // Error Log Page Entries
    uint8_t  NPSS;    // Number of Power States Support
    uint8_t  AVCC;    // Admin Vendor Specific Command Configuration
    uint8_t  APST;    // Autonomous Power State Transition Attributes
    uint16_t WCTT;    // Warning Composite Temperature Threshold
    uint16_t CCTT;    // Critical Composite Temperature Threshold
    uint16_t MTFA;    // Maximum Time for Firmware Activation
    uint32_t HMPS;    // Host Memory Buffer Preferred Size
    uint32_t HMMS;    // Host Memory Buffer Minimum Size
    uint64_t TNVM[2]; // Total NVM Capacity
    uint64_t UNVM[2]; // Unallocated NVM Capacity
    uint32_t RPMB;    // Replay Protected Memory Block Support
    uint16_t XDST;    // Extended Device Self-test Time
    uint8_t  DSTO;    // Device Self-test Options
    uint8_t  FWUG;    // Firmware Update Granularity
    uint16_t KALV;    // Keep Alive Support
    uint16_t HCTM;    // Host Controlled Thermal Management Attributes
    uint16_t MNTM;    // Minimum Thermal Management Temperature
    uint16_t MXTM;    // Maximum Thermal Management Temperature
    uint32_t SANC;    // Sanitize Capabilities
    uint32_t MNDS;    // Host Memory Buffer Minimum Descriptor Entry Size
    uint16_t MXDE;    // Host Memory Maximum Descriptors Entries
    uint16_t NSIM;    // NVM Set Identifier Maximum
    uint16_t EGIM;    // Endurance Group Identifier Maximum
    uint8_t  ANAT;    // ANA Transition Time
    uint8_t  ANAC;    // Asymmetric Namespace Access Capabilities
    uint32_t AGIM;    // ANA Group Identifier Maximum
    uint32_t AGIN;    // Number of ANA Group Identifiers
    uint32_t PELS;    // Persistent Event Log Size
    uint16_t DMID;    // Domain Identifier
    uint8_t  RSV2[10];
    uint64_t MEGC[2]; // Max Endurance Group Capacity
    uint8_t  RSV3[128];
    uint8_t  SQES; // Submission Queue Entry Size
    uint8_t  CQES; // Completion Queue Entry Size
    uint16_t MXOC; // Maximum Outstanding Commands
    uint32_t NNAM; // Number of Namespaces
} NVME_IDENTIFY_CONTROLLER;

typedef struct _NVME_IDENTIFY_NAMESPACE_LIST {
    uint32_t NSID[1024];
} NVME_IDENTIFY_NAMESPACE_LIST;

typedef struct _NVME_NAMESPACE {
    NVME_CONTROLLER *CTRL;
    uint64_t         NLBA; // Number of LBA
    uint32_t         NSID;
    uint32_t         BSZ;
    uint32_t         META;
    uint32_t         MXRS; // Max Request Size
} NVME_NAMESPACE;

#define NVME_SQE_PRP_PTR ((uint32_t)1 << 13)

typedef struct _NVME_PRP_LIST {
    uint64_t prp1;
    uint64_t prp2;
    bool     UPRP; // Use PRP list
    void    *A;    //
    uint64_t S;    //
} NVME_PRP_LIST;

static inline char *LeadingWhitespace(char *beg, char *end) {
    while (end > beg && *--end <= 0x20) {
        *end = 0;
    }
    while (beg < end && *beg <= 0x20) {
        beg++;
    }
    return beg;
}

uint32_t NVMETransfer(NVME_NAMESPACE *ns, void *buf, uint64_t lba, uint32_t count, uint32_t write);
