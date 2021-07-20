# Arduino SIM800L HTTP connector
Arduino driver for GSM/GPRS module SIMCom SIM800L to make HTTP/S connections with GET and POST methods

[![GitHub](https://img.shields.io/github/license/mashape/apistatus.svg)](https://github.com/ostaquet/Arduino-SIM800L-driver/blob/master/LICENSE)
[![GitHub release](https://img.shields.io/github/release/ostaquet/Arduino-SIM800L-driver.svg)](#releases)
[![GitHub issues](https://img.shields.io/github/issues/ostaquet/Arduino-SIM800L-driver.svg)](https://github.com/ostaquet/Arduino-SIM800L-driver/issues)

This is a comprehensive Arduino library to make HTTP or HTTPS communication through the SIMCom SIM800L module. The library has been designed to **limit the memory usage** by working with the same shared buffer all the time.

The [SIM800L](https://simcom.ee/modules/gsm-gprs/sim800/) is a GSM/GPRS module built by SIMCom. The communication with the SIM800L module relies on AT commands. The available AT commands are described in the [SIM800 series AT Command Manual](extras/SIM800%20Series_AT%20Command%20Manual_V1.09.pdf).

Supported features in this library:
 * Power management of the SIM800L module
 * Network registration and signal strengh measurement
 * GPRS connectivity and setup (APN with or without username and password)
 * HTTP and HTTPS (SSL based on in-built IP stack, see [Security concerns](https://github.com/ostaquet/Arduino-SIM800L-driver#security-concerns))
 * GET and POST methods
 * SoftwareSerial and HardwareSerial links
 * Configurable debug serial
 * Limited memory usage

## To know before starting...
 * The original module is working with an input voltage of 3.7V to 4.2V. So, __don't connect the naked module directly on the Arduino__. I personally use a module with voltage convertors from/to 5V like [this one](https://www.amazon.fr/dp/B073TF2QKL).
 * As the chipset can draw 2A maximum, it is better to use an external power source. __Using the USB power through the computer is not enough while HTTP communication__.
 * There are different firmware version of the SIM800L on the market. **The HTTPS connectivity is available only for R14 and above.** (You can check your version with the examples [BasicTest_HardwareSerial](https://github.com/ostaquet/Arduino-SIM800L-driver/blob/master/examples/BasicTest_HardwareSerial/BasicTest_HardwareSerial.ino) or [BasicTest_SoftSerial](https://github.com/ostaquet/Arduino-SIM800L-driver/blob/master/examples/BasicTest_SoftSerial/BasicTest_SoftSerial.ino))
 * The firmware of the SIM800L is quite old and **doesn't support the latest cryptographic protocols** which could lead to some issues while connecting backends (typically errors `605` and `606`). [Read also the section below about security concerns](https://github.com/ostaquet/Arduino-SIM800L-driver#security-concerns).

## How to install the library?
The easiest way to install the library is to go to the Library manager of the Arduino IDE and install the library.
 1. In the Arduino IDE, go into menu _Tools_ -> _Manage Libraries..._
 2. Search for _SIM800L_
 3. Install _SIM800L HTTP connector by Olivier Staquet_

## Examples
You will find [examples in the repository](https://github.com/ostaquet/Arduino-SIM800L-driver/tree/master/examples) to make HTTPS GET and HTTPS POST.

We are using the [Postman Echo service](https://docs.postman-echo.com) to illustrate the communication with an external API. By the way, if you need a pretty cool tool to test and validate API, I recommend [Postman](https://www.getpostman.com). They make really API devlopment simple.

## Usage

### Initiate the driver and the module
First, you have to initiate the driver by telling him the serial link and the RESET pin. The next two parameters defined the size of the internal buffer and the size of the reception buffer.
The size of the reception buffer is depending on the data you will receive from the web service/API. If the buffer is too small, you will receive only the first 512 bytes in the examples below. The driver has a buffer overflow protection.

To initiate with a SoftwareSerial link (on pin TX_PIN and RX_PIN):
```
SoftwareSerial* serial = new SoftwareSerial(TX_PIN, RX_PIN);
serial->begin(9600);
SIM800L* sim800l = new SIM800L((Stream *)serial, SIM800_RST_PIN, 200, 512);
```

To initiate with a hardware serial link (Serial1):
```
Serial1.begin(9600);
SIM800L* sim800l = new SIM800L((Stream *)&Serial1, SIM800_RST_PIN, 200, 512);
```

### Setup and check all aspects for the connectivity
Then, you have to initiate the basis for a GPRS connectivity.

The module has to be initalized and accept AT commands. You can test it and wait the module to be ready.
```
sim800l->isReady();
```
The GSM signal should be up. You can test the signed strenght and wait for a signal greater than 0.
```
sim800l->getSignal();
```

The module has to be registered on the network. You can obtain the registration status through a specific command and wait for a REGISTERED_HOME or REGISTERED_ROAMING depending on your operator and location.
```
sim800l->getRegistrationStatus();
```

Finally, you have to setup the APN for the GPRS connectivity. By example: *Internet.be* for Orange Belgium who is providing [SIM cards dedicated for IoT](https://orange-iotshop.allthingstalk.com).
```
sim800l->setupGPRS("Internet.be");
```

### Connecting GPRS
Before making any connection, you have to open the GPRS connection. It can be done easily. When the GPRS connectivity is UP, the LED is blinking fast on the SIM800L module.
```
sim800l->connectGPRS();
```

### HTTP communication GET
In order to make an HTTP GET connection to a server or the [Postman Echo service](https://docs.postman-echo.com), you just have to define the URL and the timeout in milli-seconds. The HTTP or the HTTPS protocol is set automatically depending on the URL. The URL should always start with *http://* or *https://*.
```
sim800l->doGet("https://postman-echo.com/get?foo1=bar1&foo2=bar2", 10000);
```
or
```
sim800l->doGet("https://postman-echo.com/get?foo1=bar1&foo2=bar2", "Header-1:value1\\r\\nHeader-2:value2", 10000);
```
If the method returns 200 (HTTP status code for OK), you can obtain the size of the data received.
```
sim800l->getDataSizeReceived();
```
And you can obtain the data received through a char array.
```
sim800l->getDataReceived();
```

### HTTP communication POST
In order to make an HTTP POST connection to a server or the [Postman Echo service](https://docs.postman-echo.com), you have to define a bit more information than the GET. Again, the HTTP or the HTTPS protocol is set automatically depending on the URL. The URL should always start with *http://* or *https://*.

The arguments of the method are:
 * The URL (*https://postman-echo.com/post*)
 * *(optional)* Headers *("Header-1:value1\\r\\nHeader-2:value2")*
 * The content type (*application/json*)
 * The content to POST (*{"name": "morpheus", "job": "leader"}*)
 * The write timeout while writing data to the server (*10000* ms)
 * The read timeout while reading data from the server (*10000* ms)
```
sim800l->doPost("https://postman-echo.com/post", "application/json", "{\"name\": \"morpheus\", \"job\": \"leader\"}", 10000, 10000);
```
or with headers
```
sim800l->doPost("https://postman-echo.com/post", "Header-1:value1\\r\\nHeader-2:value2", "application/json", "{\"name\": \"morpheus\", \"job\": \"leader\"}", 10000, 10000);
```
If the method returns 200 (HTTP status code for OK), you can obtain the size of the data received.
```
sim800l->getDataSizeReceived();
```
And you can obtain the data recieved through a char array. 
```
sim800l->getDataReceived();
```
### Disconnecting GPRS
At the end of the connection, don't forget to disconnect the GPRS to save power.
```
sim800l->disconnectGPRS();
```

## Security concerns

The SIM800L latest firmware update was in January 2016. It means that using the IP stack embedded on the SIM800L is convenient but not secure. **The embedded IP stack should not be used for the transfer of critical data.**

The embedded IP stack of the SIM800L only supports SSL2, SSL3 and TLS 1.0. These cryptographic protocols are considered deprecated for most of web browsers and the connection will be denied by modern backend (i.e. AWS). This will typically lead to an error `605` or `606` when you establish an HTTPS connection.

In order to secure your connectivity to the backend, we strongly recommend using an up-to-date SSL library like [WolfSSL](https://www.wolfssl.com).

## Links

 * [SIM800 series AT Command Manual](extras/SIM800%20Series_AT%20Command%20Manual_V1.09.pdf)
 * [SIM800 series IP Application Note](extras/SIM800%20Series_IP_Application%20Note_V1.02.pdf)
 * [SIM800 series SSL Application Note](extras/SIM800%20Series_SSL_Application%20Note_V1.00.pdf)
