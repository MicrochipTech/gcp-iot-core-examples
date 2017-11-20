#!/usr/bin/env python3
from ctypes import *
import base64
import sys


cryptolib = cdll.LoadLibrary('../../.build/libcryptoauth.so')

status = cryptolib.atcab_init(cryptolib.cfg_ateccx08a_i2c_default)
assert 0 == status

serial = create_string_buffer(9)
status = cryptolib.atcab_read_serial_number(byref(serial))
assert 0 == status

serial = ''.join('%02X' % x for x in serial.raw)
print('Serial Number: %s\n' % serial, file=sys.stderr)

pubkey = create_string_buffer(64)
status = cryptolib.atcab_genkey_base(0, 0, None, byref(pubkey))
assert 0 == status

public_key =  bytearray.fromhex('3059301306072A8648CE3D020106082A8648CE3D03010703420004') + bytes(pubkey.raw)
public_key = '-----BEGIN PUBLIC KEY-----\n' + base64.b64encode(public_key).decode('ascii') + '\n-----END PUBLIC KEY-----'
print(public_key)

status = cryptolib.atcab_release(None)
assert 0 == status
