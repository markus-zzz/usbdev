/*
 * Copyright (C) 2019-2020 Markus Lavin (https://www.zzzconsulting.se/)
 *
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#if COMPILE_FIRMWARE

#include "usb-dev-driver.c"

#else

#include "../sw/usb-desc.h"
#include "usb-sim-tests.h"

int UsbControlTransfer(uint8_t devAddr,
                       struct Usb_controlSetupPacket setupPacket,
                       uint8_t *data) {

  const int timeOut = 8;
  const int maxPacketSize = 8;
  // SETUP followed by DATA0
  UsbSetupPacket setup(devAddr, 0);
  setup.sendUsb();

  std::vector<uint8_t> setupPayload;
  setupPayload.resize(sizeof(setupPacket));
  for (int i = 0; i < sizeof(setupPacket); i++)
    setupPayload[i] = ((uint8_t *)&setupPacket)[i];
  UsbData0Packet setupData(setupPayload);
  setupData.sendUsb();

  for (int tries = 0;; tries++) {
    if (dynamic_cast<UsbAckPacket *>(printResponse()))
      break;
    else if (tries == timeOut)
      return -1;
  }

  if (setupPacket.bmRequestType & 0x80) { // Data stage direction IN.
    UsbInPacket in(devAddr, 0);
    int dataToggle = 1;
    int dataLength = 0;
    while (true) {
      UsbPacket *packet;
      for (int tries = 0;; tries++) {
        in.sendUsb();
        packet = printResponse();
        if (((dataToggle & 1) == 0 && dynamic_cast<UsbData0Packet *>(packet)) ||
            ((dataToggle & 1) == 1 && dynamic_cast<UsbData1Packet *>(packet)))
          break;
        else if (tries == timeOut)
          return -1;
      }
      dataToggle++;
      UsbDataPacket *dataPacket = static_cast<UsbDataPacket *>(packet);
      for (auto byte : dataPacket->m_payloadBytes)
        data[dataLength++] = byte;

      UsbAckPacket ack;
      ack.sendUsb();

      if (dataLength >= setupPacket.wLength ||
          dataPacket->m_payloadBytes.size() < maxPacketSize)
        break;
    }
    // Status stage.
    UsbOutPacket out(devAddr, 0);
    out.sendUsb();
    std::vector<uint8_t> empty{};
    UsbData1Packet data1(empty);
    data1.sendUsb();

    for (int tries = 0;; tries++) {
      if (dynamic_cast<UsbAckPacket *>(printResponse()))
        break;
      else if (tries == timeOut)
        return -1;
    }

    return dataLength;
  } else { // Data stage direction OUT.
    assert(!data && "Not implemented yet.");
    // Status stage.
    UsbInPacket in(devAddr, 0);
    UsbDataPacket *statusPacket;
    for (int tries = 0; tries < timeOut; tries++) {
      in.sendUsb();
      if ((statusPacket = dynamic_cast<UsbData1Packet *>(printResponse())))
        break;
    }
    if (!statusPacket || statusPacket->m_payloadBytes.size() != 0)
      return -1;
    UsbAckPacket ack;
    ack.sendUsb();

    return 0;
  }
}

extern "C" void run_test() {
  int res;
  struct Usb_deviceDescriptor deviceDescriptor;
  res = UsbControlTransfer(0, {0x80, 0x06, 0x0100, 0x0000, 0x0040},
                           (uint8_t *)&deviceDescriptor);
  assert(res == sizeof(deviceDescriptor));

  uint8_t devAddr = 27;
  res = UsbControlTransfer(0, {0x00, 0x05, devAddr, 0x0000, 0x0000}, nullptr);
  assert(res == 0);

  res = UsbControlTransfer(devAddr, {0x80, 0x06, 0x0100, 0x0000, 0x0040},
                           (uint8_t *)&deviceDescriptor);
  assert(res == sizeof(deviceDescriptor));

  uint8_t data[512];
  struct Usb_configDescriptor *configDescriptor =
      (struct Usb_configDescriptor *)data;

  res = UsbControlTransfer(
      devAddr, {0x80, 0x06, 0x0200, 0x0000, sizeof(*configDescriptor)}, data);
  assert(res == sizeof(*configDescriptor));

  res = UsbControlTransfer(
      devAddr, {0x80, 0x06, 0x0200, 0x0000, configDescriptor->wTotalLength},
      data);
  assert(res == configDescriptor->wTotalLength);
  struct Usb_interfaceDescriptor *interfaceDescriptor =
      (struct Usb_interfaceDescriptor *)&data[sizeof(*configDescriptor)];
  struct Usb_endpointDescriptor *endpointDescriptor =
      (struct Usb_endpointDescriptor
           *)&data[sizeof(*configDescriptor) + sizeof(*interfaceDescriptor)];

  assert(configDescriptor->wTotalLength == sizeof(*configDescriptor) +
                                               sizeof(*interfaceDescriptor) +
                                               sizeof(*endpointDescriptor));

  res = UsbControlTransfer(devAddr,
                           {0x80, 0x06,
                            (uint16_t)(0x0300 | deviceDescriptor.iManufacturer),
                            0x0000, sizeof(data)},
                           data);
  assert(res > 0);
  return;
}

#endif
