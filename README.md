ESP32 Gateway
=============

Source and configuration tools for an ESP32-based gateway.

The [Espressif ESP32](https://www.espressif.com/en/products/hardware/esp32/overview) is an SoC with integrated Bluetooth and Wi-Fi. 

We test and deploy the software on the [SparkFun ESP32 Thing](https://www.sparkfun.com/products/13907).

The software for the gateway consists of a number of separate services. 
- Configuration of Wi-Fi credentials via Bluetooth when Wi-Fi is not yet connected
- Forwarding of [PowerBlade](https://github.com/lab11/powerblade) data received in BLE advertisements to a configurable endpoint
- Serving of tools and settings as a web portal on the local network

The [ESP32 Applications repo](https://github.com/lab11/esp32-apps) contains single-purpose applications that demonstrate or test most of these services, as well as a number of other miscellaneous applications.


Setup & Run
-----------

To set up the software environment on your machine, follow the [Espressif documentation](https://esp-idf.readthedocs.io/en/latest/get-started/index.html).

While in the ESP32 Gateway repo, use `make menuconfig` to access the configuration menu.

Connect the device via USB and [set the appropriate serial port](https://esp-idf.readthedocs.io/en/latest/get-started/index.html#configure).

Optionally, Wi-Fi credentials and InfluxDB endpoint can be set as well.

Use `make flash monitor` to build the project, flash the binary to the device, and display serial output from the device.

Hit `CTRL + ]` to halt serial monitoring.

<!--

The follow `make` commands are useful:

- `make`: Builds project

- `make menuconfig`: Access configuration menu

- `make flash`: Build project, flash binary to device, & boot device

- `make monitor`: Restart device and display serial output from device

- `make flash monitor`: Build project, flash device

-->