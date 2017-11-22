#!/usr/bin/env python

import hid
import struct
import datetime

from at_kit_base import *

dev = KitDevice(hid.device())
dev.open()

now = datetime.datetime.utcnow()

data = struct.pack("<bHBBBBB", KIT_APP_COMMAND_SET_TIME, now.year, now.month, now.day, now.hour, now.minute, now.second)

dev.kit_write('b:a', data)
