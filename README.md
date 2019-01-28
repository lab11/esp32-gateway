ESP32 Gateway
=============

Source and configuration tools for an ESP32-based gateway.

The [Espressif ESP32](https://www.espressif.com/en/products/hardware/esp32/overview) is an SoC with integrated Bluetooth and Wi-Fi. 

We test and deploy the software on the [SparkFun ESP32 Thing](https://www.sparkfun.com/products/13907).

The software for the gateway consists of a number of separate services. 
- Configuration of Wi-Fi credentials via Bluetooth when Wi-Fi is not yet connected
- Serial logging of [PowerBlade](https://github.com/lab11/powerblade) data parsed from BLE advertisements
- Forwarding of PowerBlade data to a configurable Influx endpoint
- Serving of tools and settings as a web portal on the local network

The [ESP32 Applications repo](https://github.com/lab11/esp32-apps) contains single-purpose applications that demonstrate or test most of these services, as well as a number of other miscellaneous applications.


Setup
-----

To set up the software environment on your machine, follow the [Espressif documentation](https://esp-idf.readthedocs.io/en/latest/get-started/index.html).

Connect the device via USB and [find the appropriate serial port](https://esp-idf.readthedocs.io/en/latest/get-started/index.html#configure).

While in the ESP32 Gateway repo, `make menuconfig` can be used to access the configuration menu, and optionally set default Wi-Fi credentials and InfluxDB endpoint. Hit `Q` to exit.

Wi-Fi and InfluxDB credentials can also be changed after the gateway is programmed using a phone or browser. View [startup guide](https://docs.google.com/presentation/d/1h3oxfZpgNazQ42Ug1GeqB8SHxXWLkpt178d12G6gJQw/edit?usp=sharing) for instructions.


Run
---

Use `make all flash monitor` to generate necessary files, build the project, flash the binary to the device, and display serial output from the device.

Hit `CTRL + ]` to halt serial monitoring.

View [startup guide](https://docs.google.com/presentation/d/1h3oxfZpgNazQ42Ug1GeqB8SHxXWLkpt178d12G6gJQw/edit?usp=sharing) for tips on configuring and deploying the programmed gateway with PowerBlades.

<!--

The follow `make` commands are useful:

- `make` | `make build`: Builds project

- `make all` : Builds project & generates www.h header from www directory

- `make menuconfig`: Access configuration menu

- `make flash`: Build project, flash binary to device, & boot device

- `make monitor`: Restart device and display serial output from device

- `make flash monitor`: Build project, flash device

- `make all flash monitor` : Generate www, build project, flash device

-->