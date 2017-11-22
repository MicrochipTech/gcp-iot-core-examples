#!/usr/bin/env python
import hid
from at_kit_base import *
import base64

# Initialize the Kit Instance
dev = KitDevice(hid.device())
dev.open()

id = dev.kit_list(0)

# Select the device to communicate with
dev.kit_select(id)

# Send the command
resp = dev.kit_command(64, 0, 0, timeout_ms=5000)

public_key = bytearray.fromhex('3059301306072A8648CE3D020106082A8648CE3D03010703420004') + bytearray.fromhex(resp['data'][2:-4])
public_key = '-----BEGIN PUBLIC KEY-----\n' + base64.b64encode(public_key).decode('ascii') + '\n-----END PUBLIC KEY-----'

print(public_key)
