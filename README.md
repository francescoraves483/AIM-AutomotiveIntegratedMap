# AIM - Automotive Integrated Map

This is the main repository for AIM, the **Automotive Integrated Map**, an enriched, highly-efficient, in-vehicle local dynamic map implementation (V-LDM - Vehicle Local Dynamic Map), based on the [S-LDM (Server Local Dynamic Map)](https://github.com/francescoraves483/S-LDM) component implementation (*1.1.9-beta*). AIM is written in C++.

AIM is a V-LDM implementation which is able to store pre-processed information about nearby vehicles in a Vehicular Networking context, thanks to the reception of periodic standard compliant **version 2 Cooperative Awareness Messages (CAMs)**, as defined by [ETSI EN 302 637-2 V1.4.1](https://www.etsi.org/deliver/etsi_en/302600_302699/30263702/01.04.01_60/en_30263702v010401p.pdf). Compatibility with CAMs version 1 is not guaranteed.

AIM is able to work not only with standard-compliant CAMs, but also with a proposed kind of [Enhanced CAMs](https://github.com/francescoraves483/EnhancedCAMs-asn1), carrying additional information about nearby vehicles, such as their node load (in terms of CPU, RAM, GPU), channel RSSI, MAC and service IP addresses.
The ability to receive and decode Enhanced CAMs must be manually enabled through the `--enable-enhanced-CAMs` (in short, `-E`) option. This option **should not be specified** when using AIM in a fully standard-compliant scenario with normal version 2 CAMs only. Currently, Enhanced CAMs can be received from a properly configured instance of [the Open CA Basic Service (OCABS)](https://github.com/francescoraves483/OCABS-project). When using Enhanced CAMs, AIM will also store the additional node and channel load information about nearby vehicles.

If standard CAMs are used and `--enable-enhanced-CAMs` is not specified, AIM can be fully used without any limitation, and it will just not store the additional node and channel load information, being limited to the dynamic data available inside standard CAMs (i.e., to the same set of data which can be store inside the [S-LDM](https://github.com/francescoraves483/S-LDM)).

When `--enable-enhanced-CAMs`/`-E`is not specified, `--aux-dev-addr` will have no effect and should not be specified.

By default, AIM also launches a web-based user interface, on **port 8080**, displaying the content of the database (i.e., the stored nearby vehicles on a map), updated every 500 ms.
The behaviour of the web-based interface can be customized with the `--vehviz-update-interval`, `--vehviz-web-port`, `--vehviz-nodejs-port` and `--vehviz-nodejs-address` options.

# Compiling AIM

AIM can be easily compiled on the same device in which it will be run thanks to `make`.

As a pre-requisite, node.js must be installed (on Ubuntu, it can be installed with `sudo apt install nodejs`), as it is required for the web-based user interface.

To download and build the latest version of AIM, first, clone this repository:
```
git clone https://github.com/francescoraves483/AIM-AutomotiveIntegratedMap
``` 
Then, `cd` into the newly created directory:
```
cd AIM-AutomotiveIntegratedMap
```
Finally, build AIM:
```
make
```

The AIM binary file will be called `AIM`.

You can then launch AIM with:
```
./AIM --interface <interface from which CAMs will be received>
```

You can optionally enable a more verbose output on standard output with the `--log-file-name stdout` option.

The full AIM help, with all the available options, can be displayed with:
```
./AIM --help
```

If you get a permission denied error (as AIM is internally using *raw sockets*), you can try launching again AIM with `sudo` or from the `root` user.

# Cross-compiling AIM for OpenWrt

AIM is also thought to be easily cross-compiled for any embedded platform or router running OpenWrt.

The target platform must be able to run *Node.js*, as it is used by the AIM web-based graphical user interface. The `nodejs` package can be installed, on OpenWrt, with `opkg install node`.

Cross-compilation also leverages `make`.

First of all, an OpenWrt toolchain must be available in the device used for cross-compilation, with the `PATH` and `STAGING_DIR` environment variables being properly set. You can find additional information on how to set up the OpenWrt toolchain [here](https://openwrt.org/docs/guide-developer/toolchain/crosscompile).

After setting up a cross-compilation toolchain, `cd` into the AIM project main directory:
```
cd AIM-AutomotiveIntegratedMap
```
Finally, build AIM with the `compileAPU` target:
```
make compileAPU
```
As in the previous case, a binary file named `AIM` will be generated. The executable can be then transferred to the device running OpenWrt and launched with:
```
./AIM --interface <interface from which CAMs will be received>
``` 

You can also make AIM run as a service, by creating two new files inside the OpenWrt file system: 
- `/etc/init.d/AIM`, to create a new service
- `/etc/config/AIM`, to configure the AIM service

A sample of the two files is reported below:

## `/etc/init.d/AIM` sample

This sample shows how to create the AIM service through an `init.d` script. More details are available [here](https://openwrt.org/docs/techref/initscripts) and [here](https://openwrt.org/docs/guide-developer/procd-init-script-example).

For a basic working service setup (without Enhanced CAMs), you can freely copy the whole content reported below. This sample `init.d` script assumes that the AIM executable has been placed inside a directory named `/root/AIM` (see the `procd_set_param command` line).

```
#!/bin/sh /etc/rc.common
 
USE_PROCD=1
 
START=99
STOP=01
 
CONFIGURATION=AIM
 
start_service() {
    # Reading configuration parameters (i.e. interface and port)
    config_load "${CONFIGURATION}"
    local name
    local every
 
    config_get interface AIM interface
	config_get logfile AIM logfile
 
    procd_open_instance
 
    # pass config to script on start
    procd_set_param command /bin/bash -c "cd /root/AIM && ./AIM --interface $interface -L $logfile"
    procd_set_param file /etc/config/AIM
    procd_set_param stdout 1
    procd_set_param stderr 1
	procd_set_param respawn ${respawn_threshold:-3600} ${respawn_timeout:-60} ${respawn_retry:-0}
    procd_close_instance
}

stop_service() {
	killall -9 node
}
```

## `/etc/config/AIM` sample

This sample shows how to create an AIM configuration file, after setting up the proper `init.d` script.

The configuration file options should match the ones reported in the `config_get` lines of the `init.d` script.

```
config myservice 'AIM'
	option interface 'wlan1'
	option logfile 'stdout'
```

In general, this sample allows the user to set an additional verbose output log file (`logfile` option), and the interface from which CAMs are going to be received (`interface` option). This sample is related to an AIM service set to receive non-Enhanced CAMs only.

## Starting and stopping the OpenWrt service

After setting up the AIM OpenWrt service, you can start it with:
```
service AIM start
```
And stop it with:
```
service AIM stop
```
The service can also be enabled to run at start-up with:
```
service AIM enable
```
The execution at start-up can be disabled, instead, with:
```
service AIM disable
```

When running as a service, the output of AIM is not directly available, but it can be retrieved with:
```
logread
```


# Contact and License information

This project is licensed under a GPL-2.0 License. Please see also the `LICENSE` file for more details.

For any question, please write to _francescorav.es483@gmail.com_. Thanks!


# Used libraries

This project includes the **json11** C++ library by Dropbox: [github.com/dropbox/json11](https://github.com/dropbox/json11).
