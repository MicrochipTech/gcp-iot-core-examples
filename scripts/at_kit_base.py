#!/usr/bin/env python

import re
import binascii
import crcmod.predefined
import struct

#DEVICE_HID_VID = 0x04d8
#DEVICE_HID_PID = 0x0f32
DEVICE_HID_VID = 0x03eb
DEVICE_HID_PID = 0x2312
KIT_VERSION = "2.0.0"

KIT_APP_COMMAND_SET_TIME = 0

def kit_crc(data):
    """Return bytes object of the crc based on the input data bytes"""
    crc16 = crcmod.predefined.Crc('crc-16')
    crc16.update(bytes(data))
    return struct.pack('<H', int('{:016b}'.format(crc16.crcValue)[::-1], 2))


class KitError(Exception):
    def __init__(self, error_info):
        self.error_code = error_info['error_code']
        self.error_msg = error_info['error_msg']
        super(KitError, self).__init__('Kit error %d: %s' % (self.error_code, self.error_msg))


class KitDevice:
    def __init__(self, device):
        self.device = device
        self.report_size = 64
        self.next_app_cmd_id = 0
        self.app_responses = {}
        self.kit_reply_regex = re.compile('([0-9a-zA-Z]{2})\\(([^)]*)\\)')

    def open(self, vendor_id=DEVICE_HID_VID, product_id=DEVICE_HID_PID):
        """Opens HID device for the Kit. Adjusts default VID/PID for the kit."""
        self.app_responses = {}
        return self.device.open(vendor_id, product_id)

    def kit_write(self, target, data):
        """Write a kit protocol command to the device."""
        bytes_to_write = bytes('%s(%s)\n' % (target, binascii.b2a_hex(data).decode('ascii')), encoding='ascii')

        for i in range(0, len(bytes_to_write), self.report_size):
            chunk = bytes_to_write[i:i+self.report_size]
            # Prepend fixed report ID of 0, then pad with EOT characters
            self.device.write(b'\x00' + chunk + b'\x04'*(self.report_size - len(chunk)))

    def kit_read(self, timeout_ms=0):
        """Wait for a kit protocol response to be returned."""
        # Kit protocol data is all ascii
        data = []
        # Read until a newline is encountered after printable data
        while 10 not in data:
            chunk = self.device.read(self.report_size, timeout_ms=timeout_ms)
            if len(chunk) <= 0:
                raise RuntimeError('Timeout (>%d ms) waiting for reply from kit device.' % timeout_ms)
            if len(data) == 0:
                # Disregard any initial non-printable characters
                for i in range(0, len(chunk)):
                    if chunk[i] > 32:
                        break
                chunk = chunk[i:]
            data += chunk
        data = data[:data.index(10)+1] # Trim any data after the newline
        return ''.join(map(chr, data)) # Convert data from list of integers into string

    def kit_parse_reply(self, data):
        """Perform basic parsing of the kit protocol replies.

        - XX(YYZZ...)
        - where XX is the status code in hex and YYZZ is the reply data
        """
        match = self.kit_reply_regex.search(data)
        if match is None:
            raise ValueError('Unable to parse kit protocol reply: %s' % data)
        return {'status': int(match.group(1), 16), 'data': match.group(2)}

    def kit_list(self, idx):
        """Request the address of the device associated with the index"""
        self.kit_write('b:d', struct.pack("<B", idx))
        return int(self.kit_parse_reply(self.kit_read(0))['data'], 16)

    def kit_select(self, dev):
        """Select the device with the given address"""
        self.kit_write('e:p:s', struct.pack("<B", dev))
        return self.kit_read(0)
        
    def kit_info(self, param1):
        self.kit_write('b:f', struct.pack("<B", param1))
        return self.kit_read()

    def kit_command(self, opcode, param1, param2, data=b'', timeout_ms=0):
        l2s = len(data) + 7     # length(1) + opcode(1) + param1(1) + param2(2) + crc(2)
        # Make Packet
        d2s = struct.pack("<BBBH", l2s, opcode, param1, param2)
        # Append the data
        d2s += data
        # Append CRC
        d2s += kit_crc(d2s)
        # Send the command
        self.kit_write('e:t', d2s)
        # Get the response
        resp = self.kit_read(timeout_ms=timeout_ms)
        # Parse the response
        return self.kit_parse_reply(resp)
    
    @staticmethod
    def kit_parse_resp(resp):
        if 0 != resp['status']:
            raise ValueError('Command returned error {}'.format(resp['status']))
        else:
            return bytes.fromhex(resp['data'])[1:-2]
            
    def kit_command_resp(self, opcode, param1, param2, data=b'', timeout_ms=0):
        return self.kit_parse_resp(self.kit_command(opcode, param1, param2, data, timeout_ms))

    @staticmethod
    def _calc_addr(zone, slot, offset):
        block = int(offset/32)
        offset = int(offset/4) & 0x07
        if 2 == zone:
            addr = (slot << 3) | offset | (block << 8)
        elif 0 == zone:
            addr = (block << 3) | offset
        else:
            raise ValueError('Invalid Zone')

        return addr

    def read_bytes(self, zone, slot, offset, length, timeout_ms=0):
        resp = bytearray()
        for x in range(int(offset/4)*4, offset+length, 4):
            resp += self.kit_command_resp(0x02, zone, self._calc_addr(zone, slot, x), timeout_ms=timeout_ms)
        return bytes(resp[offset % 4:(offset % 4) + length])
    
    def write_bytes(self, zone, slot, offset, data, timeout_ms=0):
        idx = 0
        for x in range(int(offset/4)*4, offset+len(data), 4):
            self.kit_command_resp(0x12, zone, self._calc_addr(zone, slot, x), data=data[idx:idx+4], timeout_ms=timeout_ms)
            idx += 4

            
    
    
    
