#include "usb-dev-driver.h"
#include "usb-soc.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

///////////////

static uint32_t Usb_currEndpOwner;

struct Usb_ownerChangeMask Usb_ownerPollReg() {
  struct Usb_ownerChangeMask changeMask;
  uint32_t prevEndpOwner = Usb_currEndpOwner;
  Usb_currEndpOwner = *R_USB_ENDP_OWNER;
  changeMask.mask = Usb_currEndpOwner ^ prevEndpOwner;
  return changeMask;
}

bool Usb_ownerChangeMaskTest(struct Usb_ownerChangeMask changeMask,
                             enum Usb_endpointId id) {
  return changeMask.mask & (1 << id) ? true : false;
}

enum Usb_endpointOwner Usb_endpointOwnerGet(enum Usb_endpointId id) {
  return Usb_currEndpOwner & (1 << id) ? USB_ENDP_OWNER_USB
                                       : USB_ENDP_OWNER_CPU;
}

void Usb_endpointOwnerSetUsb(enum Usb_endpointId id) {
  uint32_t mask = (1 << id);
  Usb_currEndpOwner |= mask;
  // Important not to |= register to avoid race condition (also writing zeros
  // to R_USB_ENDP_OWNER has no effect).
  *R_USB_ENDP_OWNER = mask;
}

void Usb_dataToggleClear(enum Usb_endpointId id) {
  uint32_t mask = 1 << id;
  *R_USB_DATA_TOGGLE &= ~mask;
}

void Usb_dataToggleSet(enum Usb_endpointId id) {
  uint32_t mask = 1 << id;
  *R_USB_DATA_TOGGLE |= mask;
}

void Usb_dataToggleToggle(enum Usb_endpointId id) {
  uint32_t mask = 1 << id;
  *R_USB_DATA_TOGGLE ^= mask;
}

void Usb_sizeInSet(enum Usb_endpointId id, unsigned size) {
  unsigned shift = (id & 0x7) * 4;
  uint32_t sizeBits = (size & 0xf) << shift;
  uint32_t sizeMask = 0xf << shift;
  if (USB_ENDP_IN_0 <= id && id <= USB_ENDP_IN_7) {
    *R_USB_IN_SIZE_0_7 &= ~sizeMask;
    *R_USB_IN_SIZE_0_7 |= sizeBits;
  } else {
    *R_USB_IN_SIZE_8_15 &= ~sizeMask;
    *R_USB_IN_SIZE_8_15 |= sizeBits;
  }
}

unsigned Usb_sizeOutGet(enum Usb_endpointId id) {
  uint32_t size;
  if (USB_ENDP_OUT_0 <= id && id <= USB_ENDP_OUT_7)
    size = *R_USB_OUT_SIZE_0_7;
  else
    size = *R_USB_OUT_SIZE_8_15;

  return (size >> ((id & 0x7) * 4)) & 0xf;
}

void Usb_dataCopyIn(enum Usb_endpointId id, const uint8_t *data, unsigned len) {
  volatile uint8_t *p = (uint8_t *)(MEM_BASE_RAM + id * 8);
  for (unsigned i = 0; i < len; i++) {
    p[i] = data[i];
  }
}

///////////////

static const struct Usb_deviceDescriptor deviceDescriptor = {
    .bLength = 18,
    .bDescriptorType = 1,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0xff,
    .bDeviceSubClass = 0xff,
    .bDeviceProtocol = 0xff,
    .bMaxPacketSize = 8,
    .idVendor = 0x1234,
    .idProduct = 0x5678,
    .bcdDevice = 0x0100,
    .iManufacturer = 0,
    .iProduct = 0,
    .iSerialNumber = 0,
    .bNumConfigurations = 0x01};

static const struct Usb_configDescriptor configDescriptor = {
    .bLength = 9,
    .bDescriptorType = 2,
    .wTotalLength = 9,
    .bNumInterfaces = 0,
    .bConfigurationValue = 0,
    .iConfiguration = 0,
    .bmAttributes = 0x80,
    .MaxPower = 50};

uint8_t *in0_data;
int in0_idx, in0_len;

void service_in0(void) {
  if (in0_idx < in0_len) {
    int packetLen = MIN(in0_len - in0_idx, 8);

    Usb_dataCopyIn(USB_ENDP_IN_0, in0_data + in0_idx, packetLen);
    Usb_sizeInSet(USB_ENDP_IN_0, packetLen);
    Usb_dataToggleToggle(USB_ENDP_IN_0);
    Usb_endpointOwnerSetUsb(USB_ENDP_IN_0);

    in0_idx += packetLen;
  }
}

int main(void) {
  // .data and .bss init - begin
  Usb_currEndpOwner = 0;
  // .data and .bss init - end

  *R_USB_ADDR = 0;
  Usb_endpointOwnerSetUsb(USB_ENDP_OUT_0);
  *R_USB_CTRL = 1;

  uint8_t pendingAddress = 0;

  while (1) {
    struct Usb_ownerChangeMask changeMask = Usb_ownerPollReg();

    if (Usb_endpointOwnerGet(USB_ENDP_OUT_0) == USB_ENDP_OWNER_CPU) {
      if (Usb_sizeOutGet(USB_ENDP_OUT_0) == 8) {
        // Decode SETUP packet
        volatile struct Usb_controlSetupPacket *setupPacket =
            (volatile struct Usb_controlSetupPacket *)MEM_BASE_RAM;
        if (setupPacket->bmRequestType == 0x80 &&
            setupPacket->bRequest == 6) {      // GET_DESCRIPTOR
          if (setupPacket->wValue == 0x0100) { // Device descriptor
            in0_data = (uint8_t *)&deviceDescriptor;
            in0_len = MIN(setupPacket->wLength, sizeof(deviceDescriptor));
            in0_idx = 0;
            Usb_dataToggleClear(USB_ENDP_IN_0);
            service_in0();
          } else if (setupPacket->wValue ==
                     0x0200) { // Configuration descriptor
            in0_data = (uint8_t *)&configDescriptor;
            in0_len = MIN(setupPacket->wLength, sizeof(configDescriptor));
            in0_idx = 0;
            Usb_dataToggleClear(USB_ENDP_IN_0);
            service_in0();
          }
        } else if (setupPacket->bmRequestType == 0x00 &&
                   setupPacket->bRequest == 5) { // SET_ADDRESS
          // Wait with address change until after IN DATA1 ACK.
          pendingAddress = setupPacket->wValue;
          Usb_sizeInSet(USB_ENDP_IN_0, 0);
          Usb_dataToggleSet(USB_ENDP_IN_0);
          Usb_endpointOwnerSetUsb(USB_ENDP_IN_0);
        }
      }
      Usb_endpointOwnerSetUsb(USB_ENDP_OUT_0);
    } else if (Usb_ownerChangeMaskTest(changeMask, USB_ENDP_IN_0) &&
               Usb_endpointOwnerGet(USB_ENDP_IN_0) == USB_ENDP_OWNER_CPU) {
      if (pendingAddress != 0) {
        *R_USB_ADDR = pendingAddress;
        pendingAddress = 0;
      } else
        service_in0();
    }
  }

  return 0;
}
