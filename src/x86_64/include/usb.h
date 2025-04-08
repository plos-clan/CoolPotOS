#pragma once

#include "ctype.h"
#include "frame.h"
#include "hhdm.h"

#define USB_TYPE_UHCI 1
#define USB_TYPE_OHCI 2
#define USB_TYPE_EHCI 3
#define USB_TYPE_XHCI 4

#define USB_FULLSPEED  0
#define USB_LOWSPEED   1
#define USB_HIGHSPEED  2
#define USB_SUPERSPEED 3

#define USB_ENDPOINT_XFER_TYPE    0x03 /* in bmAttributes */
#define USB_ENDPOINT_XFER_CONTROL 0
#define USB_ENDPOINT_XFER_ISOC    1
#define USB_ENDPOINT_XFER_BULK    2
#define USB_ENDPOINT_XFER_INT     3
#define USB_ENDPOINT_DIR          0x80

#define USB_MAXADDR 127

#define USB_TIME_RSTRCY 10

#define USB_DIR_OUT 0    /* to device */
#define USB_DIR_IN  0x80 /* to host */

#define USB_TYPE_MASK     (0x03 << 5)
#define USB_TYPE_STANDARD (0x00 << 5)
#define USB_TYPE_CLASS    (0x01 << 5)
#define USB_TYPE_VENDOR   (0x02 << 5)
#define USB_TYPE_RESERVED (0x03 << 5)

#define USB_RECIP_MASK      0x1f
#define USB_RECIP_DEVICE    0x00
#define USB_RECIP_INTERFACE 0x01
#define USB_RECIP_ENDPOINT  0x02
#define USB_RECIP_OTHER     0x03

#define USB_REQ_GET_STATUS        0x00
#define USB_REQ_CLEAR_FEATURE     0x01
#define USB_REQ_SET_FEATURE       0x03
#define USB_REQ_SET_ADDRESS       0x05
#define USB_REQ_GET_DESCRIPTOR    0x06
#define USB_REQ_SET_DESCRIPTOR    0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09
#define USB_REQ_GET_INTERFACE     0x0A
#define USB_REQ_SET_INTERFACE     0x0B
#define USB_REQ_SYNCH_FRAME       0x0C

#define USB_DT_DEVICE             0x01
#define USB_DT_CONFIG             0x02
#define USB_DT_STRING             0x03
#define USB_DT_INTERFACE          0x04
#define USB_DT_ENDPOINT           0x05
#define USB_DT_DEVICE_QUALIFIER   0x06
#define USB_DT_OTHER_SPEED_CONFIG 0x07
#define USB_DT_ENDPOINT_COMPANION 0x30

#define USB_CLASS_PER_INTERFACE 0x00 /* for DeviceClass */
#define USB_CLASS_AUDIO         0x01
#define USB_CLASS_COMM          0x02
#define USB_CLASS_HID           0x03
#define USB_CLASS_PHYSICAL      0x05
#define USB_CLASS_STILL_IMAGE   0x06
#define USB_CLASS_PRINTER       0x07
#define USB_CLASS_MASS_STORAGE  0x08
#define USB_CLASS_HUB           0x09

#define USB_INTERFACE_SUBCLASS_BOOT     1
#define USB_INTERFACE_PROTOCOL_KEYBOARD 1
#define USB_INTERFACE_PROTOCOL_MOUSE    2

#define US_PR_BULK 0x50 /* bulk-only transport */
#define US_PR_UAS  0x62 /* usb attached scsi   */

#define USB_CDB_SIZE 12

#define CBW_SIGNATURE 0x43425355 // USBC

struct _USB_COMMON;
struct _USB_HUB;
typedef struct _USB_PIPE USB_PIPE;
struct _USB_ENDPOINT;
struct _USB_CONTROLLER;
typedef struct _USB_DEVICE_REQUEST {
    uint8_t  T; // Request Type
    uint8_t  C; // Request Code
    uint16_t V; // Value
    uint16_t I; // Index
    uint16_t L; // Length
} USB_DEVICE_REQUEST;
typedef struct _USB_DEVICE {
    // Major data
    uint8_t  L;
    uint8_t  DT;  // Descriptor Type
    uint16_t VER; // USB Version
    uint8_t  DC;  // Device Class
    uint8_t  DS;  // Device Subclass
    uint8_t  DP;  // Device Protocol
    uint8_t  MPS; // Max Packet Size of endpoint 0
    // Minor data
    uint16_t VID; // Vendor ID
    uint16_t PID; // Product ID
    uint16_t DID; // Device ID
    uint8_t  MIX; // Manufacturer Index
    uint8_t  PIX; // Product Index
    uint8_t  SIX; // Serial Number Index
    uint8_t  CC;  // Configuration Count

} USB_DEVICE;
typedef struct _USB_CONFIG {
    uint8_t  L;
    uint8_t  DT;  // Descriptor Type
    uint16_t TL;  // Total Length
    uint8_t  IC;  // Interface Count
    uint8_t  CV;  // Configuration Value
    uint8_t  CIX; // Configuration Index
    uint8_t  AT;  // Attributes
    uint8_t  MP;  // Max Power
} USB_CONFIG;
typedef struct _USB_INTERFACE {
    uint8_t L;
    uint8_t DT; // Descriptor Type
    uint8_t IN; // Interface Number
    uint8_t AS; // Alternate Setting
    uint8_t EC; // Endpoint Count
    uint8_t IC; // Interface Class
    uint8_t IS; // Interface Subclass
    uint8_t IP; // Interface Protocol
    uint8_t II; // Interface Index
} USB_INTERFACE;
typedef struct _USB_ENDPOINT {
    uint8_t  L;   // Length
    uint8_t  DT;  // DescriptorType
    uint8_t  EA;  // EndpointAddres
    uint8_t  AT;  // Attributes
    uint16_t MPS; // MaxPacketSize
    uint8_t  ITV; // Interval
} USB_ENDPOINT;
typedef struct _USB_HUB_OPERATION {
    uint32_t (*DTC)(struct _USB_HUB *, uint32_t); // Detect
    uint32_t (*RST)(struct _USB_HUB *, uint32_t); // Reset
    uint32_t (*DCC)(struct _USB_HUB *, uint32_t); // Disconnect
} USB_HUB_OPERATION;
typedef struct _USB_HUB {
    struct _USB_CONTROLLER *CTRL;
    struct _USB_COMMON     *DEVICE;
    USB_HUB_OPERATION      *OP;
    uint32_t                PC;
    uint32_t                DC;
} USB_HUB;
typedef struct _USB_PIPE {
    struct _USB_CONTROLLER *CTRL;
    uint16_t                MPS;
} USB_PIPE;
typedef struct _USB_CONTROLLER {
    USB_HUB RH; // Root Hub
    struct _USB_PIPE *(*CPIP)(struct _USB_COMMON *, struct _USB_PIPE *, struct _USB_ENDPOINT *);
    uint32_t (*XFER)(USB_PIPE *, USB_DEVICE_REQUEST *, void *, uint32_t, uint32_t);
    uint8_t TYPE;
    uint8_t MA; // Max Address
} USB_CONTROLLER;
typedef struct _USB_COMMAND_BLOCK_WRAPPER {
    uint32_t SIG;     // CBW Signature, fixed 0x43425355 ('USBC')
    uint32_t TAG;     // CBW Identifier, device return it in CSW.dCSWTag
    uint32_t DTL;     // CBW Required uint8_t count while transfering
    uint8_t  FLG;     // Data transfer direction, OUT=0x00, IN=0x80
    uint8_t  LUN;     // LUN ID
    uint8_t  CBL;     // Command length, in [0,16]
    uint8_t  CMD[16]; // Command to transfer
} USB_COMMAND_BLOCK_WRAPPER;
#pragma pack(1)
typedef struct _USB_COMMAND_STATUS_WRAPPER {
    uint32_t SIG;
    uint32_t TAG;
    uint32_t RSD; // Remainder data size
    uint8_t  STS; // Command status, 0 means correct
} USB_COMMAND_STATUS_WRAPPER;
#pragma pack()
typedef struct _USB_DISK_DRIVER {
    // SCSI_DISK_DRIVER DVR;
    USB_PIPE *BIP;
    USB_PIPE *BOP;
    uint32_t  LUN;
} USB_DISK_DRIVER;
typedef struct _USB_COMMON {
    struct _USB_COMMON     *A0; // L
    struct _USB_COMMON     *A1; // R
    struct _USB_CONTROLLER *CTRL;
    USB_CONFIG             *CFG;
    USB_INTERFACE          *IFC;
    USB_HUB                *HUB;
    USB_PIPE               *PIPE;
    uint32_t                PORT;
    uint32_t                SPD;
    USB_DISK_DRIVER        *DRV;
} USB_COMMON;

extern const uint32_t  SPEED_TO_CTRL_SIZE[];
extern USB_CONTROLLER *USB_CTRL;

uint32_t      USBEnumerate(USB_HUB *);
uint32_t      USBSetAddress(USB_COMMON *);
uint32_t      ConfigureUSB(USB_COMMON *);
void          USBD2P(USB_PIPE *, USB_COMMON *, USB_ENDPOINT *);
USB_ENDPOINT *USBSearchEndpoint(USB_COMMON *, int, int);
