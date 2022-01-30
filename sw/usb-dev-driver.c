#include "usb-dev-driver.h"
#include "usb-soc.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

typedef unsigned int size_t;

size_t strlen(const char *str) {
  int len = 0;
  for (; str[len] != '\0'; len++)
    ;
  return len;
}

void *memcpy(void *dst, const void *src, size_t len) {
  uint8_t *dstb = dst;
  const uint8_t *srcb = src;
  for (size_t i = 0; i < len; i++)
    dstb[i] = srcb[i];
  return dst;
}

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
    .idVendor = 0xabc0,
    .idProduct = 0x0064,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 0x01};

static const struct __attribute__((packed)) {
  struct Usb_configDescriptor configDescriptor;
  struct Usb_interfaceDescriptor interfaceDescriptor;
  struct Usb_endpointDescriptor endpointDescriptor;
} compoundConfigDescriptor = {
    .configDescriptor = {.bLength = 9,
                         .bDescriptorType = 2,
                         .wTotalLength = sizeof(compoundConfigDescriptor),
                         .bNumInterfaces = 1,
                         .bConfigurationValue = 1,
                         .iConfiguration = 0,
                         .bmAttributes = 0x80,
                         .MaxPower = 50},
    .interfaceDescriptor = {.bLength = 9,
                            .bDescriptorType = 4,
                            .bInterfaceNumber = 0,
                            .bAlternateSetting = 0,
                            .bNumEndpoints = 1,
                            .bInterfaceClass = 0xff,
                            .bInterfaceSubClass = 0xff,
                            .bInterfaceProtocol = 0xff,
                            .iInterface = 0},
    .endpointDescriptor = {.bLength = 7,
                           .bDescriptorType = 5,
                           .bEndpointAddress = 0x01,
                           .bmAttributes = 0x03,
                           .wMaxPacketSize = 8,
                           .bInterval = 5}};

static const struct Usb_stringZeroDescriptor stringZeroDescriptor = {
    .bLength = 4, .bDescriptorType = 3, .wLANGID = {0x0409}};

static const char *strings[] = {
    "Markus Lavin (https://www.zzzconsulting.se/)",
    "MyC64 - FPGA based Commodore 64 emulator as a USB device",
    "0123456789abcdef"};

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

void WriteByte(uint16_t addr, uint8_t byte) {
  volatile uint8_t *q = (volatile uint8_t *)(MEM_BASE_C64_RAM + addr);
  *q = byte;
}

#define R_I2C_SCL ((volatile uint32_t *)0x30000008)
#define R_I2C_SDA ((volatile uint32_t *)0x3000000c)

void i2c_delay() {
  for (volatile int i = 0; i < 4; i++)
    ;
}

void i2c_start() {
  *R_I2C_SDA = 0;
  i2c_delay();
}

void i2c_stop() {
  *R_I2C_SCL = 0;
  *R_I2C_SDA = 0;
  i2c_delay();
  *R_I2C_SCL = 1;
  i2c_delay();
  *R_I2C_SDA = 1;
  i2c_delay();
}

uint8_t i2c_write_byte(uint8_t byte) {
  for (int i = 0; i < 8; i++) {
    *R_I2C_SCL = 0;
    i2c_delay();
    *R_I2C_SDA = (byte >> (7 - i)) & 1;
    i2c_delay();
    *R_I2C_SCL = 1;
    i2c_delay();
    i2c_delay();
  }
  // Get acknowledge from device
  *R_I2C_SCL = 0;
  *R_I2C_SDA = 1;
  i2c_delay();
  i2c_delay();
  *R_I2C_SCL = 1;
  i2c_delay();
  return *R_I2C_SDA & 1;
}

void max9850_write_reg(uint8_t addr, uint8_t data) {
  i2c_start();
  i2c_write_byte(0x20); // I2C address for write
  i2c_write_byte(addr);
  i2c_write_byte(data);
  i2c_stop();
}

void long_delay() {
  for (volatile int i = 0; i < 50000; i++)
    i2c_delay();
}

void max9850_setup(void) {
  *R_I2C_SCL = 1;
  *R_I2C_SDA = 1;

  long_delay();

  // Enable (0x5) - Disable everything
  max9850_write_reg(0x05, 0x00);

  // Clock (0x6) - ICLK = MCLK = 12.5MHz
  max9850_write_reg(0x04, 0x00);

  // General Purpose (0x3)
  max9850_write_reg(0x03, 0x00);

  // Digital Audio (0xA) - MAS=0 (slave mode), BCINV=1
  max9850_write_reg(0x0a, 0x20);

  // LRCLK MSB (0x8) - INT=1 (integer mode)
  max9850_write_reg(0x08, 0x80);

  // LRCLK LSB (0x9) - LSB=16
  max9850_write_reg(0x09, 0x10);

  // Charge Pump (0x7) - NC=9
  max9850_write_reg(0x07, 0x09);

  // Interrupt Enable (0x4) - Disable all interrupts
  max9850_write_reg(0x04, 0x00);

  // Volume (0x2)
  max9850_write_reg(0x02, 0x1c);

  // Enable (0x5)
  max9850_write_reg(0x05, 0xfd);
}

int main(void) {
  max9850_setup();
  // .data and .bss init - begin
  Usb_currEndpOwner = 0;
  // .data and .bss init - end

  *R_USB_ADDR = 0;
  Usb_endpointOwnerSetUsb(USB_ENDP_OUT_0);
  Usb_endpointOwnerSetUsb(USB_ENDP_OUT_1);
  *R_USB_CTRL = 1;

  uint8_t pendingAddress = 0;
  uint16_t activeConfig = 0;
  uint16_t activeInterface = 0;
  uint8_t controlDataStage = 0;
  uint16_t prgLoadAddr = 0;
  uint16_t prgLoadSize = 0;
  uint16_t prgLoadPos = 0;

  uint8_t tmpBuf[64];

  while (1) {
    struct Usb_ownerChangeMask changeMask = Usb_ownerPollReg();

    if (Usb_endpointOwnerGet(USB_ENDP_OUT_0) == USB_ENDP_OWNER_CPU) {
      if (controlDataStage) {
        unsigned size = Usb_sizeOutGet(USB_ENDP_OUT_0);
        // Write bytes to C64 RAM.
        volatile uint8_t *p = (volatile uint8_t *)MEM_BASE_RAM;
        for (int i = 0; i < size; i++) {
          WriteByte(prgLoadAddr++, p[i]);
          prgLoadPos++;
        }
        if (prgLoadPos == prgLoadSize) {
          controlDataStage = 0;
          Usb_sizeInSet(USB_ENDP_IN_0, 0);
          Usb_dataToggleSet(USB_ENDP_IN_0); // XXX: What does the standard say?
                                            // Need to check this.
          Usb_endpointOwnerSetUsb(USB_ENDP_IN_0);
        }
      } else if (Usb_sizeOutGet(USB_ENDP_OUT_0) == 8) {
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
            in0_data = (uint8_t *)&compoundConfigDescriptor;
            in0_len =
                MIN(setupPacket->wLength, sizeof(compoundConfigDescriptor));
            in0_idx = 0;
            Usb_dataToggleClear(USB_ENDP_IN_0);
            service_in0();
          } else if ((setupPacket->wValue & 0xff00) ==
                     0x0300) { // String descriptor
            if ((setupPacket->wValue & 0x00ff) == 0) {
              in0_data = (uint8_t *)&stringZeroDescriptor;
              in0_len = MIN(setupPacket->wLength, 4);
            } else {
              int stringIdx = (setupPacket->wValue & 0x00ff) - 1;
              struct Usb_stringDescriptor *stringDesc =
                  (struct Usb_stringDescriptor *)tmpBuf;
              int stringLen = strlen(strings[stringIdx]);
              // The strings are sent UNICODE
              stringDesc->bLength = 2 + stringLen * 2;
              stringDesc->bDescriptorType = 3;
              for (int i = 0; i < stringLen; i++) {
                stringDesc->bString[i * 2 + 1] = 0;
                stringDesc->bString[i * 2 + 0] = strings[stringIdx][i];
              }
              in0_data = tmpBuf;
              in0_len = MIN(setupPacket->wLength, stringDesc->bLength);
            }
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
        } else if (setupPacket->bmRequestType == 0x00 &&
                   setupPacket->bRequest == 9) { // SET_CONFIGURATION
          activeConfig = setupPacket->wValue;
          Usb_sizeInSet(USB_ENDP_IN_0, 0);
          Usb_dataToggleSet(USB_ENDP_IN_0);
          Usb_endpointOwnerSetUsb(USB_ENDP_IN_0);
        } else if (setupPacket->bmRequestType == 0x00 &&
                   setupPacket->bRequest == 11) { // SET_INTERFACE
          activeInterface = setupPacket->wIndex;
          Usb_sizeInSet(USB_ENDP_IN_0, 0);
          Usb_dataToggleSet(USB_ENDP_IN_0);
          Usb_endpointOwnerSetUsb(USB_ENDP_IN_0);
        } else if (setupPacket->bmRequestType == 0x42 &&
                   setupPacket->bRequest == 1) { // Vendor specific - Load .prg
          prgLoadAddr = setupPacket->wValue;
          prgLoadSize = setupPacket->wLength;
          prgLoadPos = 0;
          controlDataStage = 1;
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
    } else if (Usb_ownerChangeMaskTest(changeMask, USB_ENDP_OUT_1) &&
               Usb_endpointOwnerGet(USB_ENDP_OUT_1) == USB_ENDP_OWNER_CPU) {
      if (Usb_sizeOutGet(USB_ENDP_OUT_1) == 8) {
        volatile uint32_t *p = (volatile uint32_t *)(MEM_BASE_RAM + 8);
        volatile uint32_t *q = (volatile uint32_t *)MEM_BASE_SYS_REG;
        q[0] = p[0];
        q[1] = p[1];
      }
      Usb_dataToggleSet(USB_ENDP_OUT_1);
      Usb_endpointOwnerSetUsb(USB_ENDP_OUT_1);
    }
  }

  return 0;
}
