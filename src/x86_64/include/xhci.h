#pragma once

#pragma once

#include "ctype.h"
#include "frame.h"
#include "hhdm.h"
#include "pcb.h"
#include "pcie.h"
#include "usb.h"

#define XHCI_TIME_POSTPOWER 20
#define XHCI_RING_ITEMS     255

#define XHCI_CMD_RS     (1 << 0) // Run Stop
#define XHCI_CMD_HCRST  (1 << 1)
#define XHCI_CMD_INTE   (1 << 2)
#define XHCI_CMD_HSEE   (1 << 3)
#define XHCI_CMD_LHCRST (1 << 7)
#define XHCI_CMD_CSS    (1 << 8)
#define XHCI_CMD_CRS    (1 << 9)
#define XHCI_CMD_EWE    (1 << 10)
#define XHCI_CMD_EU3S   (1 << 11)

#define XHCI_STS_HCH  (1 << 0)
#define XHCI_STS_HSE  (1 << 2)
#define XHCI_STS_EINT (1 << 3)
#define XHCI_STS_PCD  (1 << 4)
#define XHCI_STS_SSS  (1 << 8)
#define XHCI_STS_RSS  (1 << 9)
#define XHCI_STS_SRE  (1 << 10)
#define XHCI_STS_CNR  (1 << 11)
#define XHCI_STS_HCE  (1 << 12)

#define XHCI_PORTSC_CCS (1 << 0) // Current Connect Status
#define XHCI_PORTSC_PED (1 << 1) // Port Enabled Disabled
#define XHCI_PORTSC_PR  (1 << 4) // Port Reset
#define XHCI_PORTSC_PP  (1 << 9) // Port Power

#define TRB_TR_IDT (1 << 6)

#define TRB_LINK_TC 2

typedef struct _XHCI_CAPABILITY {
    uint8_t  CL;   // Capability Register Length
    uint8_t  RSV;  // Reserved
    uint16_t IVN;  // Interface Version Number
    uint32_t SP1;  // Struct Parameters 1
    uint32_t SP2;  // Struct Parameters 2
    uint32_t SP3;  // Struct Parameters 3
    uint32_t CP1;  // Capability Parameters
    uint32_t DBO;  // Doorbell Offset
    uint32_t RRSO; // Runtime Registers Space Offset
    uint32_t CP2;  // Capability Parameters 2
} XHCI_CAPABILITY;
typedef struct _XHCI_OPERATIONAL {
    uint32_t CMD; // USB Command
    uint32_t STS; // USB Status
    uint32_t PS;  // Page Size
    uint32_t RSV0[2];
    uint32_t DNC; // Device Notification Control
    uint64_t CRC; // Command Ring Control
    uint32_t RSV1[4];
    uint64_t DCA; // Device Context Base Address Array Pointer
    uint32_t CFG; // Configure
} XHCI_OPERATIONAL;
typedef struct _XHCI_PORT {
    uint32_t PSC;   // Port Status and Control
    uint32_t PPMSC; // Port Power Management Status and Control
    uint32_t PLI;   // Port Link Info
    uint32_t RSV;
} XHCI_PORT;
typedef struct _XHCI_INTERRUPT {
    uint32_t IMAN; // Interrupt Management
    uint32_t IMOD; // Interrupt Moderation
    uint32_t TS;   // Event Ring Segment Table Size
    uint32_t RSV;
    uint64_t ERS; // Event Ring Segment Table Base Address
    uint64_t ERD; // Event Ring Dequeue Pointer
} XHCI_INTERRUPT;
typedef struct _XHCI_RUNTIME {
    uint64_t       MFI[4]; // Microframe Index
    XHCI_INTERRUPT IR[0];  // Interrupt Register Set
} XHCI_RUNTIME;
typedef struct _XHCI_XCAPABILITY {
    uint32_t CAP;
    uint32_t DATA[];
} XHCI_XCAPABILITY;
typedef union _XHCI_TRANSFER_BLOCK // transfer block (ring element)
{
    struct {
        uint32_t DATA[4];
    };
    struct {
        uint32_t RSV0;
        uint32_t RSV1;
        uint32_t RSV2;
        uint32_t C    : 0x01; // Cycle
        uint32_t RSV3 : 0x09;
        uint32_t TYPE : 0x06; // TBR Type
        uint32_t RSV4 : 0x10;
    };
} XHCI_TRANSFER_BLOCK;
typedef struct _XHCI_TRANSFER_RING {
    XHCI_TRANSFER_BLOCK *RING;
    XHCI_TRANSFER_BLOCK  EVT;
    uint16_t             EID;
    uint16_t             NID;
    uint8_t              CCS;
    uint8_t              LOCK;
} XHCI_TRANSFER_RING;
// event ring segment
typedef struct _XHCI_RING_SEGMENT {
    uint64_t A; // Address
    uint32_t S; // Size
    uint32_t R; // Reserved
} XHCI_RING_SEGMENT;
typedef struct _XHCI_CONTROLLER {
    USB_CONTROLLER     USB;
    // Register Set
    XHCI_CAPABILITY   *CR; // Capability Registers
    XHCI_OPERATIONAL  *OR; // Operational Registers
    XHCI_PORT         *PR; // Port Registers
    XHCI_RUNTIME      *RR; // Runtime Registers
    uint32_t          *DR; // Doorbell Registers
    // Data structures
    uint64_t          *DVC; // Device Context List
    XHCI_RING_SEGMENT *SEG; // Device Segment List
    XHCI_TRANSFER_RING CMD; // Device Command List
    XHCI_TRANSFER_RING EVT; // Device Event List
    // HCSP1
    uint32_t           SN; // Number of Device Slots (MAXSLOTS)
    uint32_t           IN; // Number of Interrupts (MAXINTRS)
    uint32_t           PN; // Number of Ports (MAXPORTS)
    // HCCP1
    uint32_t           XEC; // xHCI Extended Capability Pointer
    uint32_t           CSZ; // Context Size
} XHCI_CONTROLLER;
typedef struct _XHCI_INPUT_COMTROL_CONTEXT {
    uint32_t DRP;
    uint32_t ADD;
    uint32_t RSV0[5];
    uint8_t  CV; // Configuration Value
    uint8_t  IN; // Interface Number
    uint8_t  AS; // Alternate Setting
    uint8_t  RSV1;
} XHCI_INPUT_CONTROL_CONTEXT;
typedef struct _XHCI_SLOT_CONTEXT {
    uint32_t RS   : 0x14; // Route String
    uint32_t SPD  : 0x04; // Speed
    uint32_t RSV0 : 0x01;
    uint32_t MTT  : 0x01; // Multi-TT
    uint32_t HUB  : 0x01;
    uint32_t CE   : 0x05; // Context Entries
    uint16_t MEL;         // Max Exit Latency
    uint8_t  RHPN;        // Root Hub Port Number
    uint8_t  PN;          // Number of Ports
    uint8_t  TTID;        // TT Hub Slot ID
    uint8_t  TTP;         // TT Port Number
    uint16_t TTT  : 0x02; // TT Think Time
    uint16_t RSV1 : 0x04;
    uint16_t IT   : 0x0A; // Interrupter Target
    uint32_t DA   : 0x08; // USB Device Address
    uint32_t RSV2 : 0x13;
    uint32_t SS   : 0x05; // Slot State
    uint32_t RSV3[4];
} XHCI_SLOT_CONTEXT;
typedef struct _XHCI_ENDPOINT_CONTEXT {
    uint32_t EPS  : 0x03; // EP State
    uint32_t RSV0 : 0x05;
    uint32_t MULT : 0x02;
    uint32_t MPSM : 0x05; // Max Primary Streams
    uint32_t LSA  : 0x01; // Linear Stream Array
    uint32_t ITV  : 0x08; // Interval
    uint32_t MEPH : 0x08; // Max Endpoint Service Time Interval Payload Hi

    uint32_t RSV1 : 0x01;
    uint32_t EC   : 0x02; // Error Count
    uint32_t EPT  : 0x03; // Endpoint Type
    uint32_t RSV2 : 0x01;
    uint32_t HID  : 0x01; // Host Initiate Disable
    uint32_t MBS  : 0x08; // Max Brust Size
    uint32_t MPSZ : 0x10; // Max Packet Size

    uint64_t DCS  : 0x01; // Dequeue Cycle State
    uint64_t RSV3 : 0x03;
    uint64_t TRDP : 0x3C; // TR Dequeue Pointer

    uint16_t ATL;  // Average TRB Length
    uint16_t MEPL; // Max ESIT Payload Lo
    uint32_t RSV4[3];
} XHCI_ENDPOINT_CONTEXT;
typedef struct _XHCI_PIPE {
    USB_PIPE           USB;
    XHCI_TRANSFER_RING RING;
    uint32_t           SID;
    uint32_t           EPID;
} XHCI_PIPE;
typedef union _XHCI_TRB_NORMAL {
    XHCI_TRANSFER_BLOCK TRB;
    struct {
        uint64_t DATA;
        uint32_t TL   : 0x11; // TRB Transfer Length
        uint32_t TDS  : 0x05; // TD Size
        uint32_t IT   : 0x0A; // Interrupt Target
        uint32_t C    : 0x01; // Cycle
        uint32_t ENT  : 0x01; // Evaluate Next TRB
        uint32_t ISP  : 0x01; // Interrupt on Short Packet
        uint32_t NS   : 0x01; // No Snoop
        uint32_t CH   : 0x01; // Chain
        uint32_t IOC  : 0x01; // Interrupt On Complete
        uint32_t IDT  : 0x01; // Immediate Data
        uint32_t RSV0 : 0x02;
        uint32_t BEI  : 0x01; // Block Event Interrupt
        uint32_t TYPE : 0x06; // TRB Type
        uint32_t RSV1 : 0x10;
    };
} XHCI_TRB_NORMAL;
typedef union _XHCI_TRB_SETUP_STAGE {
    XHCI_TRANSFER_BLOCK TRB;
    struct {
        USB_DEVICE_REQUEST DATA;

        uint32_t TL   : 0x11; // TRB Transfer Length
        uint32_t RSV0 : 0x05;
        uint32_t IT   : 0x0A; // Interrupt Target

        uint32_t C    : 0x01; // Cycle
        uint32_t RSV1 : 0x04;
        uint32_t IOC  : 0x01; // Interrupt On Completion
        uint32_t IDT  : 0x01; // Immediate Data
        uint32_t RSV2 : 0x03;
        uint32_t TYPE : 0x06; // TRB Type
        uint32_t TRT  : 0x02; // Transfer Type
        uint32_t RSV3 : 0x0E;
    };
} XHCI_TRB_SETUP_STAGE;
typedef union _XHCI_TRB_DATA_STAGE {
    XHCI_TRANSFER_BLOCK TRB;
    struct {
        uint64_t DATA : 0x40; // Data Buffer

        uint32_t TL  : 0x11; // Transfer Length
        uint32_t TDS : 0x05; // TD Size
        uint32_t IT  : 0x0A; // Interrupt Target

        uint32_t C    : 0x01; // Cycle
        uint32_t ENT  : 0x01; // Evaluate Next TRB
        uint32_t ISP  : 0x01; // Interrupt on Short Packet
        uint32_t NS   : 0x01; // No Snoop
        uint32_t CH   : 0x01; // Chain
        uint32_t IOC  : 0x01; // Interrupt On Completion
        uint32_t IDT  : 0x01; // Immediate Data
        uint32_t RSV0 : 0x03;
        uint32_t TYPE : 0x06; // TRB Type
        uint32_t DIR  : 0x01; // Direction
        uint32_t RSV1 : 0x0F;
    };
} XHCI_TRB_DATA_STAGE;
typedef union _XHCI_TRB_STATUS_STAGE {
    XHCI_TRANSFER_BLOCK TRB;
    struct {
        uint32_t RSV0;

        uint32_t RSV1;

        uint32_t RSV2 : 0x16;
        uint32_t IT   : 0x0A; // Interrupt Target

        uint32_t C    : 0x01; // Cycle
        uint32_t ENT  : 0x01; // Evaluate Next TRB
        uint32_t RSV3 : 0x02;
        uint32_t CH   : 0x01; // Chain
        uint32_t IOC  : 0x01; // Interrupt On Completion
        uint32_t RSV4 : 0x04;
        uint32_t TYPE : 0x06; // TRB Type
        uint32_t DIR  : 0x01; // Direction
        uint32_t RSV5 : 0x0F;
    };
} XHCI_TRB_STATUS_STAGE;
typedef union _XHCI_TRB_LINK {
    XHCI_TRANSFER_BLOCK TRB;
    struct {
        uint64_t RSV0 : 0x04;
        uint64_t RSP  : 0x3C; // Ring Segment Pointer
        uint32_t RSV1 : 0x16;
        uint32_t IT   : 0x0A; // Interrupter Target
        uint32_t C    : 0x01; // Cycle
        uint32_t TC   : 0x01; // Toggle Cycle
        uint32_t RSV2 : 0x02;
        uint32_t CH   : 0x01; // Chain
        uint32_t IOC  : 0x01; // Interrupt On Completion
        uint32_t RSV3 : 0x04;
        uint32_t TYPE : 0x06; // TRB Type
        uint32_t RSV4 : 0x10;
    };
} XHCI_TRB_LINK;
typedef union _XHCI_TRB_ENABLE_SLOT {
    XHCI_TRANSFER_BLOCK TRB;
    struct {
        uint32_t RSV0[3];
        uint32_t C    : 0x01; // Cycle
        uint32_t RSV1 : 0x09;
        uint32_t TYPE : 0x06; // TBR Type
        uint32_t ST   : 0x05; // Slot Type
        uint32_t RSV2 : 0x0B;
    };
} XHCI_TRB_ENABLE_SLOT;
typedef union _XHCI_TRB_DISABLE_SLOT {
    XHCI_TRANSFER_BLOCK TRB;
    struct {
        uint32_t RSV0[3];
        uint16_t C    : 0x01; // Cycle
        uint16_t RSV1 : 0x09;
        uint16_t TYPE : 0x06; // TRB Type
        uint8_t  RSV2;
        uint8_t  SID;
    };
} XHCI_TRB_DISABLE_SLOT;
typedef union _XHCI_TRB_ADDRESS_DEVICE {
    XHCI_TRANSFER_BLOCK TRB;
    struct {
        uint64_t RSV0 : 0x04;
        uint64_t ICP  : 0x3C; // Input Context Pointer
        uint32_t RSV1;
        uint16_t C    : 0x01; // Cycle
        uint16_t RSV2 : 0x08;
        uint16_t BSR  : 0x01; // Block Set Address Request
        uint16_t TYPE : 0x06; // TRB Type
        uint8_t  RSV3;
        uint8_t  SID; // Slot ID
    };
} XHCI_TRB_ADDRESS_DEVICE;
typedef union _XHCI_TRB_CONFIGURE_ENDPOINT {
    XHCI_TRANSFER_BLOCK TRB;
    struct {
        uint64_t ICP : 0x40; // Input Context Pointer (0x10 aligned)
        uint32_t RSV0;
        uint16_t C    : 0x01; // Cycle
        uint16_t RSV1 : 0x08;
        uint16_t DC   : 0x01; // Deconfigure
        uint16_t TYPE : 0x06; // TRB Type
        uint8_t  RSV2;
        uint8_t  SID; // Slot ID
    };
} XHCI_TRB_CONFIGURE_ENDPOINT;
typedef union _XHCI_TRB_RESET_ENDPOINT {
    XHCI_TRANSFER_BLOCK TRB;
    struct {
        uint32_t RSV0[3];
        uint16_t C    : 0x01; // Cycle
        uint16_t RSV1 : 0x09;
        uint16_t TYPE : 0x06; // TRB Type
        uint8_t  RSV2;
        uint8_t  SID; // Slot ID
    };
} XHCI_TRB_RESET_ENDPOINT;
typedef union _XHCI_TRB_EVALUATE_CONTEXT {
    XHCI_TRANSFER_BLOCK TRB;
    struct {
        uint64_t ICP; // Input Context Pointer (0x16 aligned)
        uint32_t RSV0;
        uint16_t C    : 0x01; // Cycle
        uint16_t RSV1 : 0x08;
        uint16_t BSR  : 0x01; // Block Set Address Request
        uint16_t TYPE : 0x06; // TRB Type
        uint8_t  RSV2;
        uint8_t  SID; // Slot ID
    };
} XHCI_TRB_EVALUATE_CONTEXT;
typedef union _XHCI_TRB_COMMAND_COMPLETION {
    XHCI_TRANSFER_BLOCK TRB;
    struct {
        uint64_t CTP  : 0x40; // Command TRBV Pointer
        uint32_t CCP  : 0x18; // Command Completion Parameter
        uint32_t CC   : 0x08; // Completion Code
        uint16_t C    : 0x01; // Cycle
        uint16_t RSV1 : 0x09;
        uint16_t TYPE : 0x06;
        uint8_t  VFID;
        uint8_t  SID;
    };
} XHCI_TRB_COMMAND_COMPLETION;
typedef union _XHCI_TRB_TRANSFER_RING {
    XHCI_TRANSFER_BLOCK TRB;
    struct {
        uint64_t RING;
        uint32_t RSV0;
        uint16_t C    : 0x01; // Cycle
        uint16_t RSV1 : 0x09;
        uint16_t TYPE : 0x06; // TRB Type
        uint16_t RSV2;
    };
} XHCI_TRB_TRANSFER_RING;
typedef enum _XHCI_PORT_LINK_STATE {
    PLS_U0              = 0,
    PLS_U1              = 1,
    PLS_U2              = 2,
    PLS_U3              = 3,
    PLS_DISABLED        = 4,
    PLS_RX_DETECT       = 5,
    PLS_INACTIVE        = 6,
    PLS_POLLING         = 7,
    PLS_RECOVERY        = 8,
    PLS_HOT_RESET       = 9,
    PLS_COMPILANCE_MODE = 10,
    PLS_TEST_MODE       = 11,
    PLS_RESUME          = 15,
} XHCI_PORT_LINK_STATE;
typedef enum _XHCI_ENDPOINT_TYPE {
    EP_NOT_VALID = 0,
    EP_OUT_ISOCH,
    EP_OUT_BULK,
    EP_OUT_INTERRUPT,
    EP_CONTROL,
    EP_IN_ISOCH,
    EP_IN_BULK,
    EP_IN_INTERRUPT
} XHCI_ENDPOINT_TYPE;
typedef enum _XHCI_TRB_TYPE {
    TRB_RESERVED = 0,
    TRB_NORMAL,
    TRB_SETUP_STAGE,
    TRB_DATA_STAGE,
    TRB_STATUS_STAGE,
    TRB_ISOCH,
    TRB_LINK,
    TRB_EVDATA,
    TRB_NOOP,
    TRB_ENABLE_SLOT,
    TRB_DISABLE_SLOT,
    TRB_ADDRESS_DEVICE,
    TRB_CONFIGURE_ENDPOINT,
    TRB_EVALUATE_CONTEXT,
    TRB_RESET_ENDPOINT,
    TRB_STOP_ENDPOINT,
    TRB_SET_TR_DEQUEUE,
    TRB_RESET_DEVICE,
    TRB_FORCE_EVENT,
    TRB_NEGOTIATE_BW,
    TRB_SET_LATENCY_TOLERANCE,
    TRB_GET_PORT_BANDWIDTH,
    TRB_FORCE_HEADER,
    TRB_NOOP_COMMAND,
    TRB_TRANSFER = 32,
    TRB_COMMAND_COMPLETE,
    TRB_PORT_STATUS_CHANGE,
    TRB_BANDWIDTH_REQUEST,
    TRB_DOORBELL,
    TRB_HOST_CONTROLLER,
    TRB_DEVICE_NOTIFICATION,
    TRB_MFINDEX_WRAP,
} XHCI_TRB_TYPE;

extern const uint32_t SPEED_XHCI[];

void             xhci_setup();
void             SetupXHCIControllerPCI(pcie_device_t *);
XHCI_CONTROLLER *SetupXHCIController(uint64_t);
uint32_t         ConfigureXHCI(XHCI_CONTROLLER *);
void             XHCIQueueTRB(XHCI_TRANSFER_RING *, XHCI_TRANSFER_BLOCK *);
void             XHCICopyTRB(XHCI_TRANSFER_RING *, XHCI_TRANSFER_BLOCK *);
void             XHCIDoorbell(XHCI_CONTROLLER *, uint32_t, uint32_t);
uint32_t         XHCIProcessEvent(XHCI_CONTROLLER *);
uint32_t         XHCIWaitCompletion(XHCI_CONTROLLER *, XHCI_TRANSFER_RING *);
uint32_t         XHCICommand(XHCI_CONTROLLER *, XHCI_TRANSFER_BLOCK *);
uint32_t         XHCIHUBDetect(USB_HUB *, uint32_t);
uint32_t         XHCIHUBReset(USB_HUB *, uint32_t);
uint32_t         XHCIHUBDisconnect(USB_HUB *, uint32_t);
USB_PIPE        *XHCICreatePipe(USB_COMMON *, USB_PIPE *, USB_ENDPOINT *);
uint32_t         XHCITransfer(USB_PIPE *, USB_DEVICE_REQUEST *, void *, uint32_t, uint32_t);
void             XHCICreateTransferRing(XHCI_TRANSFER_RING *);
