#pragma once

#include <types.h>

struct __attribute__((packed)) usb_setup_packet {
    uint8_t request_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
};

typedef struct usb_setup_packet SetupPacket;

struct __attribute__((packed)) usb_device_descriptor {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t bcd_usb;
    uint8_t device_class;
    uint8_t device_subclass;
    uint8_t device_protocol;
    uint8_t max_packet_size_0;
    uint16_t id_vendor;
    uint16_t id_product;
    uint16_t bcd_device;
    uint8_t i_manufacturer;
    uint8_t i_product;
    uint8_t i_serial_number;
    uint8_t num_configurations;
};

typedef struct usb_device_descriptor DeviceDescriptor;

struct __attribute__((packed)) usb_configuration_descriptor {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t total_length;
    uint8_t num_interfaces;
    uint8_t configuration_value;
    uint8_t configuration_str;
    uint8_t attributes;
    uint8_t max_power;
};

typedef struct usb_configuration_descriptor ConfigurationDescriptor;

struct __attribute__((packed)) usb_interface_descriptor {
    uint8_t length;
    uint8_t descriptor_type;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t num_endpoints;
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    uint8_t interface_str;
};

typedef struct usb_interface_descriptor InterfaceDescriptor;

struct __attribute__((packed)) usb_hid_descriptor_header {
    uint8_t length;
    uint8_t descriptor_type;
    uint16_t bcd_hid;
    uint8_t country_code;
    uint8_t num_descriptors;
};

typedef struct usb_hid_descriptor_header HidDescriptorHeader;

struct __attribute__((packed)) usb_endpoint_descriptor {
    uint8_t length;
    uint8_t descriptor_type;
    uint8_t endpoint_address;
    uint8_t attributes;
    uint16_t max_packet_size;
    uint8_t interval;
};

typedef struct usb_endpoint_descriptor EndpointDescriptor;

struct __attribute__((packed)) usb_ss_endpoint_companion_descriptor {
    uint8_t length;
    uint8_t descriptor_type;
    uint8_t max_burst;
    uint8_t attributes;
    uint16_t bytes_per_interval;
};

typedef struct usb_ss_endpoint_companion_descriptor SsEndpointCompanionDescriptor;
