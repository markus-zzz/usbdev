#include "usb-dev-driver.h"
#include "usb-soc.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

static const struct Usb_deviceDescriptor deviceDescriptor = {
    .bLength = 18,
    .bDescriptorType = 1,
    .bcdUSB = 0x0200,
    .bDeviceClass = 2,
    .bDeviceSubClass = 2,
    .bDeviceProtocol = 0,
    .bMaxPacketSize = 8,
    .idVendor = 0x0483,
    .idProduct = 0x5740,
    .bcdDevice = 0x0200,
    .iManufacturer = 0x01,
    .iProduct = 0x2,
    .iSerialNumber = 0x3,
    .bNumConfigurations = 0x01};

void put_IN_data(const uint8_t *data, unsigned len) {
  uint32_t endpoint_mask = 1UL << 16;
  *R_USB_DATA_TOGGLE ^= endpoint_mask;
  volatile uint8_t *p = (uint8_t *)(MEM_BASE_RAM);
  for (unsigned i = 0; i < len; i++) {
    p[128 + i] = data[i];
  }

  *R_USB_IN_SIZE_0_7 = len & 0xf; // XXX: mask and shift properly for endpoint
}

uint8_t *in0_data;
int in0_idx, in0_len;
uint32_t prevEndpOwner;

void service_in0(void) {
  if (in0_idx < in0_len) {
    int packetLen = MIN(in0_len - in0_idx, 8);
    put_IN_data(in0_data + in0_idx, packetLen);
    in0_idx += packetLen;
    *R_USB_ENDP_OWNER = prevEndpOwner |= 0x10000;
  }
}

int main(void) {
  *R_USB_ADDR = 0;
  *R_USB_ENDP_OWNER = prevEndpOwner = 0x00000001;
  *R_USB_CTRL = 1;

  while (1) {
    if (*R_USB_ENDP_OWNER == prevEndpOwner)
      continue;

    uint32_t event = *R_USB_ENDP_OWNER ^ prevEndpOwner;
    if (event & 1) {
      // Decode SETUP packet
      volatile struct Usb_controlSetupPacket *setupPacket =
          (volatile struct Usb_controlSetupPacket *)MEM_BASE_RAM;
      if (setupPacket->bmRequestType == 0x80 &&
          setupPacket->bRequest == 6) {      // GET_DESCRIPTOR
        if (setupPacket->wValue == 0x0100) { // Device descriptor
          in0_data = (uint8_t *)&deviceDescriptor;
          in0_len = sizeof(deviceDescriptor);
          in0_idx = 0;
          service_in0();
        }
      }
      else if (setupPacket->bmRequestType == 0x00 &&
          setupPacket->bRequest == 5) { // SET_ADDRESS
        *R_USB_ADDR = setupPacket->wValue;
      }
    } else if (event & 0x10000) {
      service_in0();
    }
  }

  return 0;
}
