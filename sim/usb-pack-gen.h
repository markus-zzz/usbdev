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

#pragma once

#include <fstream>
#include <iostream>
#include <vector>

class UsbSymbolVector;

enum class UsbSymbol { J, K, SE0, SE1 };

// Bits are stored lsb first
class UsbBitVector : public std::vector<bool> {
public:
  UsbBitVector() {}
  UsbBitVector(const char *bitStr);
  UsbBitVector(unsigned value, unsigned bits);
  UsbBitVector(const UsbSymbolVector &symVec);
  UsbBitVector(std::_Bit_iterator::iterator from,
               std::_Bit_iterator::iterator to);

  void print(std::ostream &os, int stride = -1) const;
  unsigned calcCRC5() const;
  unsigned calcCRC16() const;
  unsigned extractUint(std::_Bit_iterator::iterator from,
                       std::_Bit_iterator::iterator to);
};

static std::ostream &operator<<(std::ostream &os, const UsbBitVector &bv) {
  bv.print(os);
  return os;
}

class UsbSymbolVector : public std::vector<UsbSymbol> {
public:
  UsbSymbolVector() {}
  UsbSymbolVector(UsbBitVector &bitVector);
  void addEop();
  void stripEop();
  bool startsWithSync() const;
  bool endsWithEop() const;
  void print(std::ostream &os, int stride = -1) const;
  void printCsv(std::ostream &os) const;
  void sendUsb() const;
};

static std::ostream &operator<<(std::ostream &os, const UsbSymbolVector &sv) {
  sv.print(os);
  return os;
}

class UsbPacket {
public:
  virtual ~UsbPacket() = default;
  void printCsv(std::ostream &os) const;
  void sendUsb() const;
  virtual UsbSymbolVector encode() const = 0;
  virtual void print(std::ostream &os) const {
    os << "PRINT NOT IMPLEMENTED FOR THIS CLASS!!!\n";
  }

protected:
};

//
// Token Packets
//

class UsbTokenPacket : public UsbPacket {
public:
  UsbSymbolVector encode() const;

private:
  unsigned m_usbAddress;
  unsigned m_usbEndpoint;
  UsbBitVector m_pidBits;

protected:
  UsbTokenPacket(UsbBitVector pidBits, unsigned usbAddress,
                 unsigned usbEndpoint);
};

class UsbSetupPacket : public UsbTokenPacket {
  static constexpr char pidStr[] = "1011_0100";

public:
  UsbSetupPacket(unsigned usbAddress, unsigned usbEndpoint);
};

class UsbOutPacket : public UsbTokenPacket {
  static constexpr char pidStr[] = "1000_0111";

public:
  UsbOutPacket(unsigned usbAddress, unsigned usbEndpoint);
};

class UsbInPacket : public UsbTokenPacket {
  static constexpr char pidStr[] = "1001_0110";

public:
  UsbInPacket(unsigned usbAddress, unsigned usbEndpoint);
};

//
// Data packets
//

class UsbDataPacket : public UsbPacket {
protected:
  UsbDataPacket(UsbBitVector pidBits, std::vector<uint8_t> &payloadBytes);

public:
  std::vector<uint8_t> m_payloadBytes;
  UsbSymbolVector encode() const;
  void print(std::ostream &os) const;

private:
  UsbBitVector m_pidBits;
};

class UsbData0Packet : public UsbDataPacket {
  static constexpr char pidStr[] = "1100_0011";

public:
  UsbData0Packet(std::vector<uint8_t> &payloadBytes);
  static UsbData0Packet *tryDecode(UsbBitVector &bits);
};

class UsbData1Packet : public UsbDataPacket {
  static constexpr char pidStr[] = "1101_0010";

public:
  UsbData1Packet(std::vector<uint8_t> &payloadBytes);
  static UsbData1Packet *tryDecode(UsbBitVector &bits);
};

//
// Handshake packets
//

class UsbAckPacket : public UsbPacket {
  static constexpr char pidStr[] = "0100_1011";

public:
  UsbAckPacket() {}
  UsbSymbolVector encode() const;
  void print(std::ostream &os) const;
  static UsbAckPacket *tryDecode(UsbBitVector &bits);
};

class UsbNakPacket : public UsbPacket {
  static constexpr char pidStr[] = "0101_1010";

public:
  UsbNakPacket() {}
  UsbSymbolVector encode() const { return UsbSymbolVector(); }
  void print(std::ostream &os) const;
  static UsbNakPacket *tryDecode(UsbBitVector &bits);
};
