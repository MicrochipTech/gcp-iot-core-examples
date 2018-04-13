#!/usr/bin/env python
import hid
import argparse
from at_kit_base import *
import base64

# Default configuration for ATECC508A
atecc508_config = bytearray.fromhex(
    'B0 00 55 00 8F 20 C4 44 87 20 87 20 8F 0F C4 36'
    '9F 0F 82 20 0F 0F C4 44 0F 0F 0F 0F 0F 0F 0F 0F'
    '0F 0F 0F 0F FF FF FF FF 00 00 00 00 FF FF FF FF'
    '00 00 00 00 FF FF FF FF FF FF FF FF FF FF FF FF'
    'FF FF FF FF 00 00 55 55 FF FF 00 00 00 00 00 00'
    '33 00 1C 00 13 00 13 00 7C 00 1C 00 3C 00 33 00'
    '3C 00 3C 00 3C 00 30 00 3C 00 3C 00 3C 00 30 00')

# Default configuration for ATECC608A
atecc608_config = bytearray.fromhex(
    'B0 00 00 01 8F 20 C4 44 87 20 87 20 8F 0F C4 36'
    '9F 0F 82 20 0F 0F C4 44 0F 0F 0F 0F 0F 0F 0F 0F'
    '0F 0F 0F 0F FF FF FF FF 00 00 00 00 FF FF FF FF'
    '00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00'
    '00 00 00 00 00 00 55 55 FF FF 00 00 00 00 00 00'
    '33 00 1C 00 13 00 13 00 7C 00 1C 00 3C 00 33 00'
    '3C 00 3C 00 3C 00 30 00 3C 00 3C 00 3C 00 30 00')
    
def configure_device(i2c_addr=0xC0):
    # Initialize the Kit Instance
    dev = KitDevice(hid.device())
    dev.open()

    # Get id of the first device
    id = dev.kit_list(0)

    # Select the device to communicate with
    dev.kit_select(id)

    # Get the kit type
    print('\nGetting the Kit Type')
    kit_version = dev.kit_info(0).split(' ')[0]
    print('    {} Found'.format(kit_version))
    
    # Check if this is the demo firmware - if not don't brick a CK590
    if ('GCP IoT Core Example' != dev.device.get_product_string()) and ('AT88CK590' == kit_version) and (0xC0 != i2c_addr):
        print('    Kit only supports I2C Address 0xC0')
        i2c_addr = 0xC0

    # Get the device type
    print('\nGetting the Device Info')
    resp = dev.kit_command_resp(0x30, 0, 0, timeout_ms=5000)
    dev_type = resp[2]
    print('    Device type is {}08'.format(dev_type >> 4))
    
    # Get Current I2C Address
    print('\nGetting the I2C Address')
    resp = dev.kit_command_resp(0x02, 0, 0x04, timeout_ms=5000)
    print('    Current Address: {:02X}'.format(resp[0]))
    print('    New Address: {:02X}'.format(i2c_addr))
    
    # Check the zone locks
    print('\nReading the Lock Status')
    resp = dev.kit_command_resp(0x02, 0, 0x15, timeout_ms=5000)
    data_zone_lock = (0x55 != resp[2])
    config_zone_lock = (0x55 != resp[3])
    print('    Config Zone: {}'.format('Locked' if config_zone_lock else 'Unlocked'))
    print('    Data Zone: {}'.format('Locked' if data_zone_lock else 'Unlocked'))
    
    # Program the configuration zone
    print('\nProgram Configuration')
    if not config_zone_lock:   
        if 0x50 == dev_type:
            print('    Programming ATECC508A Configuration')
            config = atecc508_config
        elif 0x60 == dev_type:
            print('    Programming ATECC608A Configuration')
            config = atecc608_config
        else:
            print('    Unknown Device')
            raise ValueError('Unknown Device Type: {:02X}'.format(dev_type))

        # Update with the target I2C Address
        config[0] = i2c_addr

        # Write configuration
        resp = dev.write_bytes(0, 0, 16, config, timeout_ms=5000)
        print('        Success')

        # Verify Config Zone
        print('    Verifying Configuration')
        resp = dev.read_bytes(0, 0, 16, len(config), timeout_ms=5000)    
    
        if resp != config:
            raise ValueError('Configuration read from the device does not match')
        print('        Success')

        print('    Locking Configuration')
        resp = dev.kit_command_resp(0x17, 0x80, 0, timeout_ms=5000)
        print('        Locked')
    else:
        print('    Locked, skipping')
    
    # Check data zone lock
    print('\nActivating Configuration')
    if not data_zone_lock:
        # Lock the data zone
        resp = dev.kit_command_resp(0x17, 0x81, 0, timeout_ms=5000)
        print('    Activated')
    else:
        print('    Already Active')

    # Generate new keys
    print('\nGenerating New Keys')
    resp = dev.kit_command_resp(0x40, 4, 0, timeout_ms=5000)
    print('    Key 0 Success')
    
    resp = dev.kit_command_resp(0x40, 4, 2, timeout_ms=5000)
    print('    Key 2 Success')
    
    resp = dev.kit_command_resp(0x40, 4, 3, timeout_ms=5000)
    print('    Key 3 Success')
    
    resp = dev.kit_command_resp(0x40, 4, 7, timeout_ms=5000)
    print('    Key 7 Success')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='GCP ECC Device Configuration Utility')
    parser.add_argument('--i2c', default='0xB0', help='I2C Address (in hex)')
    args = parser.parse_args()
    
    configure_device(int(args.i2c,16))
    print('\nDevice Successfully Configured')
    