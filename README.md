## Microchip Examples for Google Cloud Platform IOT Core 

### Summary

This project contains examples and tools for setting up a secure IOT node with IOT Core
using the ATECC508A or ATECC608A and WINC1500 with a selection of energy efficent microcontrollers.

#### Security Devices:
* [ATECC508A](http://www.microchip.com/wwwproducts/en/ATECC508A)
* [ATECC608A](http://www.microchip.com/wwwproducts/en/ATECC608A)

#### Connectivity:
* [ATWINC1500](http://www.microchip.com/wwwproducts/en/ATWINC1500) - 802.11 b/g/n Module with integrated TLS 1.2 stack
* [ATSAMW25](http://www.microchip.com/wwwproducts/en/ATSAMW25) - Integrated module featuring ATWINC1500 + ATSAMD21G18A + ATECC508A

#### Microcontrollers or SOCs:
* ATSAMD21  	(ARM Cortex-M0)
* ATSAMG55    (ARM Cortex-M4)
* ATSAMW25		(ARM Cortex-M0)
* Raspberry Pi

### Project Organization

These examples were built with [Atmel Studio 7](http://www.atmel.com/microsite/atmel-studio/)

ATSAMD21 Quick Start
```/boards/gcp_iot_core_samd21.atsln```

ATSAMG55 Quick Start
```/boards/gcp_iot_core_samg55.atsln```

ATSAMW25 Quick Start
```/boards/gcp_iot_core_samw25.atsln```

### Getting Started

The first step is ensuring that you have a [Google Cloud IoT Core](https://console.cloud.google.com) account set up with IoT core.

The tutorials will walk you through the initial configuration of your account and creation of your first device registry

1) [IoT Core Getting Started Guide](https://cloud.google.com/iot/docs/how-tos/getting-started)

2) [IoT Core Quick start](https://cloud.google.com/iot/docs/quickstart)



### Examples

#### Fan Controller

This example features a number of interconnected devices to demonstrate data acquisition, control, and integration into Google IoT Core.

Hardware Featured:
* ATECC508A - Security/Cryptographic Authentication
* ATWINC1500 - WIFI Connectivity
* EMC1414 - Temperature Sensor
* EMC2301 - 5V PWM Fan Controller


### License

For more details of the included software and licenses please reference LICENSE.md

The included software is covered by the Microchip License with the exception
of the following:

* ASF headers and WINC1500 driver software are covered by the BSD 3 clause license
* Eclipse Paho MQTT Client is covered by the Eclipse Distribution License v 1.0 (BSD 3 clause)
* Parson (JSON C library) is covered by the MIT free software license
