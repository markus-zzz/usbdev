#pragma once
#include <stdbool.h>
#include <stdint.h>

struct Usb_deviceDescriptor {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint16_t bcdUSB;
  uint8_t bDeviceClass;
  uint8_t bDeviceSubClass;
  uint8_t bDeviceProtocol;
  uint8_t bMaxPacketSize;
  uint16_t idVendor;
  uint16_t idProduct;
  uint16_t bcdDevice;
  uint8_t iManufacturer;
  uint8_t iProduct;
  uint8_t iSerialNumber;
  uint8_t bNumConfigurations;
};

struct Usb_configDescriptor {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint16_t wTotalLength;
  uint8_t bNumInterfaces;
  uint8_t bConfigurationValue;
  uint8_t iConfiguration;
  uint8_t bmAttributes;
  uint8_t MaxPower;
};

struct Usb_interfaceDescriptor {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bInterfaceNumber;
  uint8_t bAlternateSetting;
  uint8_t bNumEndpoints;
  uint8_t bInterfaceClass;
  uint8_t bInterfaceSubClass;
  uint8_t bInterfaceProtocol;
  uint8_t iInterface;
};

struct Usb_endpointDescriptor {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bEndpointAddress;
  uint8_t bmAttributes;
  uint16_t wMaxPacketSize;
  uint8_t bInterval;
  uint8_t bRefresh;
  uint8_t bSynchAddress;
};

struct Usb_controlSetupPacket {
  uint8_t bmRequestType;
  uint8_t bRequest;
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
};

enum Usb_endpointOwner { USB_ENDP_OWNER_CPU, USB_ENDP_OWNER_USB };

enum Usb_endpointId {
  USB_ENDP_OUT_0 = 0,
  USB_ENDP_OUT_1,
  USB_ENDP_OUT_2,
  USB_ENDP_OUT_3,
  USB_ENDP_OUT_4,
  USB_ENDP_OUT_5,
  USB_ENDP_OUT_6,
  USB_ENDP_OUT_7,
  USB_ENDP_OUT_8,
  USB_ENDP_OUT_9,
  USB_ENDP_OUT_10,
  USB_ENDP_OUT_11,
  USB_ENDP_OUT_12,
  USB_ENDP_OUT_13,
  USB_ENDP_OUT_14,
  USB_ENDP_OUT_15,
  USB_ENDP_IN_0,
  USB_ENDP_IN_1,
  USB_ENDP_IN_2,
  USB_ENDP_IN_3,
  USB_ENDP_IN_4,
  USB_ENDP_IN_5,
  USB_ENDP_IN_6,
  USB_ENDP_IN_7,
  USB_ENDP_IN_8,
  USB_ENDP_IN_9,
  USB_ENDP_IN_10,
  USB_ENDP_IN_11,
  USB_ENDP_IN_12,
  USB_ENDP_IN_13,
  USB_ENDP_IN_14,
  USB_ENDP_IN_15
};

struct Usb_ownerChangeMask {
  uint32_t mask;
};
