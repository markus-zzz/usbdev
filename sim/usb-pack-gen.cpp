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

#include "usb-pack-gen.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

void ClockUsbSymbol(UsbSymbol sym);

UsbBitVector::UsbBitVector(const char *bitStr) {
  for (int i = 0; bitStr[i]; i++) {
    if (bitStr[i] == '1')
      push_back(true);
    else if (bitStr[i] == '0')
      push_back(false);
  }
}
UsbBitVector::UsbBitVector(unsigned value, unsigned bits) {
  for (unsigned i = 0; i < bits; i++) {
    if (value >> i & 1)
      push_back(true);
    else
      push_back(false);
  }
}

UsbBitVector::UsbBitVector(const UsbSymbolVector &symVector) {
  // Perform NRZI decoding and bit-destuffing
  UsbSymbol prevSymbol = UsbSymbol::J; // Idle state.
  int onesCntr = 0;
  for (auto sym : symVector) {
    if (onesCntr == 6) {
      // Discard stuff bit.
      onesCntr = 0;
    } else if (sym == prevSymbol) {
      push_back(true); // 1
      onesCntr++;
    } else {
      push_back(false); // 0
      onesCntr = 0;
    }
    prevSymbol = sym;
  }
}

UsbBitVector::UsbBitVector(std::_Bit_iterator::iterator from,
                           std::_Bit_iterator::iterator to) {
  insert(begin(), from, to);
}

void UsbBitVector::print(std::ostream &os, int stride) const {
  int idx = 0;
  for (auto bit : *this) {
    if (idx == stride) {
      idx = 0;
      os << "_";
    }
    idx++;
    os << (bit ? '1' : '0');
  }
}
unsigned UsbBitVector::calcCRC5() const {
  unsigned poly5 = 0x25;
  unsigned crc5 = 0x1f;
  for (auto bit : *this) {
    crc5 <<= 1;
    if ((bit ? 1 : 0) != (crc5 >> 5))
      crc5 ^= poly5;
    crc5 &= 0x1f;
  }
  crc5 ^= 0x1f;
  return crc5;
}
unsigned UsbBitVector::calcCRC16() const {
  unsigned poly16 = 0x18005;
  unsigned crc16 = 0xffff;
  for (auto bit : *this) {
    crc16 <<= 1;
    if ((bit ? 1 : 0) != (crc16 >> 16))
      crc16 ^= poly16;
    crc16 &= 0xffff;
  }
  crc16 ^= 0xffff;
  return crc16;
}

unsigned UsbBitVector::extractUint(std::_Bit_iterator::iterator from,
                                   std::_Bit_iterator::iterator to) {
  unsigned value = 0;
  int idx = 0;
  for (auto it = from; it != to; it++) {
    if (*it)
      value |= 1 << idx;
    idx++;
  }
  return value;
}

UsbSymbolVector::UsbSymbolVector(UsbBitVector &bitVector) {
  // Perform bit-stuffing and NRZI encoding.
  UsbSymbol prevSymbol = UsbSymbol::J; // Idle state.
  int onesCntr = 0;
  for (auto bit : bitVector) {
    if (bit) {
      onesCntr++;
      push_back(prevSymbol);
      if (onesCntr == 6) {
        // Need to insert 0 stuff-bit.
        onesCntr = 0;
        UsbSymbol invSym =
            prevSymbol == UsbSymbol::J ? UsbSymbol::K : UsbSymbol::J;
        push_back(invSym);
        prevSymbol = invSym;
      }
    } else {
      onesCntr = 0;
      UsbSymbol invSym =
          prevSymbol == UsbSymbol::J ? UsbSymbol::K : UsbSymbol::J;
      push_back(invSym);
      prevSymbol = invSym;
    }
  }
}
void UsbSymbolVector::addEop() {
  push_back(UsbSymbol::SE0);
  push_back(UsbSymbol::SE0);
  push_back(UsbSymbol::J);
}

void UsbSymbolVector::stripEop() {
  if (endsWithEop())
    erase(end() - 3, end());
}

bool UsbSymbolVector::startsWithSync() const {
  return *(begin() + 0) == UsbSymbol::K && *(begin() + 1) == UsbSymbol::J &&
         *(begin() + 2) == UsbSymbol::K && *(begin() + 3) == UsbSymbol::J &&
         *(begin() + 4) == UsbSymbol::K && *(begin() + 5) == UsbSymbol::J &&
         *(begin() + 6) == UsbSymbol::K && *(begin() + 7) == UsbSymbol::K;
}

bool UsbSymbolVector::endsWithEop() const {
  return *(end() - 3) == UsbSymbol::SE0 && *(end() - 2) == UsbSymbol::SE0 &&
         *(end() - 1) == UsbSymbol::J;
}

void UsbSymbolVector::print(std::ostream &os, int stride) const {
  int idx = 0;
  for (auto sym : *this) {
    if (idx == stride) {
      idx = 0;
      os << "_";
    }
    idx++;
    switch (sym) {
    case UsbSymbol::J:
      os << "J";
      break;
    case UsbSymbol::K:
      os << "K";
      break;
    case UsbSymbol::SE0:
      os << "0";
      break;
    case UsbSymbol::SE1:
      os << "1";
      break;
    }
  }
}
void UsbSymbolVector::printCsv(std::ostream &os) const {
  for (auto sym : *this) {
    for (int i = 0; i < 4; i++) {
      switch (sym) {
      case UsbSymbol::J:
        os << "1,0\n";
        break;
      case UsbSymbol::K:
        os << "0,1\n";
        break;
      case UsbSymbol::SE0:
        os << "0,0\n";
        break;
      case UsbSymbol::SE1:
        os << "1,1\n";
        break;
      }
    }
  }
}

void UsbSymbolVector::sendUsb() const {
  for (auto sym : *this) {
    ClockUsbSymbol(sym);
  }
}

void UsbPacket::printCsv(std::ostream &os) const { /*m_symVector.printCsv(os);*/
}

void UsbPacket::sendUsb() const {
  UsbSymbolVector symVector = encode();
  symVector.sendUsb();
}

UsbTokenPacket::UsbTokenPacket(UsbBitVector pidBits, unsigned usbAddress,
                               unsigned usbEndpoint)
    : m_pidBits(pidBits), m_usbAddress(usbAddress), m_usbEndpoint(usbEndpoint) {
}

UsbSymbolVector UsbTokenPacket::encode() const {
  UsbBitVector syncBits("0000_0001");
  UsbBitVector addrBits(m_usbAddress, 7);
  UsbBitVector endpBits(m_usbEndpoint, 4);

  UsbBitVector payloadBits;
  payloadBits.insert(payloadBits.end(), addrBits.begin(), addrBits.end());
  payloadBits.insert(payloadBits.end(), endpBits.begin(), endpBits.end());

  UsbBitVector crcBits(payloadBits.calcCRC5(), 5);
  std::reverse(crcBits.begin(), crcBits.end());

  // Build the entire packet.
  UsbBitVector packetBits;
  packetBits.insert(packetBits.end(), syncBits.begin(), syncBits.end());
  packetBits.insert(packetBits.end(), m_pidBits.begin(), m_pidBits.end());
  packetBits.insert(packetBits.end(), payloadBits.begin(), payloadBits.end());
  packetBits.insert(packetBits.end(), crcBits.begin(), crcBits.end());

  UsbSymbolVector packetSyms(packetBits);

  // Add End-Of-Packet (EOP)
  packetSyms.addEop();

  return packetSyms;
}

constexpr char UsbSetupPacket::pidStr[];
UsbSetupPacket::UsbSetupPacket(unsigned usbAddress, unsigned usbEndpoint)
    : UsbTokenPacket(UsbBitVector(pidStr), usbAddress, usbEndpoint) {}

constexpr char UsbOutPacket::pidStr[];
UsbOutPacket::UsbOutPacket(unsigned usbAddress, unsigned usbEndpoint)
    : UsbTokenPacket(UsbBitVector(pidStr), usbAddress, usbEndpoint) {}

constexpr char UsbInPacket::pidStr[];
UsbInPacket::UsbInPacket(unsigned usbAddress, unsigned usbEndpoint)
    : UsbTokenPacket(UsbBitVector(pidStr), usbAddress, usbEndpoint) {}

UsbDataPacket::UsbDataPacket(UsbBitVector pidBits,
                             std::vector<uint8_t> &payloadBytes)
    : m_pidBits(pidBits), m_payloadBytes(payloadBytes) {}

UsbSymbolVector UsbDataPacket::encode() const {
  UsbBitVector syncBits("0000_0001");

  UsbBitVector payloadBits;
  for (auto &byte : m_payloadBytes) {
    UsbBitVector byteBits(byte, 8);
    payloadBits.insert(payloadBits.end(), byteBits.begin(), byteBits.end());
  }

  UsbBitVector crcBits(payloadBits.calcCRC16(), 16);
  std::reverse(crcBits.begin(), crcBits.end());

  // Build the entire packet.
  UsbBitVector packetBits;
  packetBits.insert(packetBits.end(), syncBits.begin(), syncBits.end());
  packetBits.insert(packetBits.end(), m_pidBits.begin(), m_pidBits.end());
  packetBits.insert(packetBits.end(), payloadBits.begin(), payloadBits.end());
  packetBits.insert(packetBits.end(), crcBits.begin(), crcBits.end());

  UsbSymbolVector packetSyms(packetBits);

  // Add End-Of-Packet (EOP)
  packetSyms.addEop();

  return packetSyms;
}
void UsbDataPacket::print(std::ostream &os) const {
  os << "DATA" << (dynamic_cast<const UsbData0Packet *>(this) ? "0" : "1")
     << ":";
  for (auto &byte : m_payloadBytes)
    os << " " << std::setw(2) << std::setfill('0') << std::hex << (int)byte;
  os << std::endl;
}

constexpr char UsbData0Packet::pidStr[];

UsbData0Packet::UsbData0Packet(std::vector<uint8_t> &payloadBytes)
    : UsbDataPacket(UsbBitVector(pidStr), payloadBytes) {}

UsbData0Packet *UsbData0Packet::tryDecode(UsbBitVector &bits) {
  auto itPid = bits.begin() + 8;
  if (UsbBitVector(itPid, itPid + 8) == UsbBitVector(pidStr)) {
    auto itPayload = bits.begin() + 8 + 8;
    auto itCrc = bits.end() - 16;
    std::vector<uint8_t> payload;
    for (; itPayload != itCrc; itPayload += 8)
      payload.push_back(bits.extractUint(itPayload, itPayload + 8));
    return new UsbData0Packet(payload);
  } else
    return nullptr;
}

constexpr char UsbData1Packet::pidStr[];

UsbData1Packet::UsbData1Packet(std::vector<uint8_t> &payloadBytes)
    : UsbDataPacket(UsbBitVector(pidStr), payloadBytes) {}

UsbData1Packet *UsbData1Packet::tryDecode(UsbBitVector &bits) {
  auto itPid = bits.begin() + 8;
  if (UsbBitVector(itPid, itPid + 8) == UsbBitVector(pidStr)) {
    auto itPayload = bits.begin() + 8 + 8;
    auto itCrc = bits.end() - 16;
    std::vector<uint8_t> payload;
    for (; itPayload != itCrc; itPayload += 8)
      payload.push_back(bits.extractUint(itPayload, itPayload + 8));
    return new UsbData1Packet(payload);
  } else
    return nullptr;
}

constexpr char UsbAckPacket::pidStr[];
UsbAckPacket *UsbAckPacket::tryDecode(UsbBitVector &bits) {
  auto itPid = bits.begin() + 8;
  if (UsbBitVector(itPid, itPid + 8) == UsbBitVector(pidStr))
    return new UsbAckPacket();
  else
    return nullptr;
}

void UsbAckPacket::print(std::ostream &os) const {
  os << "ACK" << std::endl;
}

UsbSymbolVector UsbAckPacket::encode() const {
  UsbBitVector syncBits("0000_0001");

  UsbBitVector pidBits(pidStr);
  // Build the entire packet.
  UsbBitVector packetBits;
  packetBits.insert(packetBits.end(), syncBits.begin(), syncBits.end());
  packetBits.insert(packetBits.end(), pidBits.begin(), pidBits.end());

  UsbSymbolVector packetSyms(packetBits);

  // Add End-Of-Packet (EOP)
  packetSyms.addEop();

  return packetSyms;
}

constexpr char UsbNakPacket::pidStr[];
UsbNakPacket *UsbNakPacket::tryDecode(UsbBitVector &bits) {
  auto itPid = bits.begin() + 8;
  if (UsbBitVector(itPid, itPid + 8) == UsbBitVector(pidStr))
    return new UsbNakPacket();
  else
    return nullptr;
}

void UsbNakPacket::print(std::ostream &os) const {
  os << "NAK" << std::endl;
}
