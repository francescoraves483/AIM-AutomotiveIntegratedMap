/*
*	Options C module
*	
*	C module for easy and efficient command-line options management
*	The code is derived from LaTe, out project for an advanced latency measurement tool: https://github.com/francescoraves483/LaMP_LaTe
*
*	Copyright (C) 2019-2021 Francesco Raviglione (CNIT - Politecnico di Torino), Marco Malinverno (CNIT - Politecnico di Torino)
*	
*	This program is free software; you can redistribute it and/or
*	modify it under the terms of the GNU General Public License
*	as published by the Free Software Foundation; either version 2
*	of the License, or (at your option) any later version.
*	
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*	
*	You should have received a copy of the GNU General Public License
*	along with this program; if not, write to the Free Software
*	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef OPTIONSMODULE_H_INCLUDED
#define OPTIONSMODULE_H_INCLUDED

#include <inttypes.h>
#include <stdbool.h>
#include <float.h>
#include "options_strings.h"

// Insert here the version string
#define VERSION_STR "AIM 0.1.0-beta (der. S-LDM 1.1.9-beta)" // 1.0.0 -> first (initial) cross-border version

#define DEFAULT_VEHVIZ_NODEJS_UDP_ADDR "127.0.0.1"
#define DEFAULT_VEHVIZ_NODEJS_UDP_PORT 48110

#define DEFAULT_VEHVIZ_WEB_PORT 8080

// Default Vehicle Visualizer web-based GUI update rate, in seconds
#define DEFAULT_VEHVIZ_UPDATE_INTERVAL_SECONDS 0.5

// Default central latitude and longitude values for the Vehicle Visualizer
// This is used solely for visualization purposes
#define DEFAULT_CENTRAL_LAT 43.659064
#define DEFAULT_CENTRAL_LON -70.256955

// Default interface name and UDP port to listen to
#define DEFAULT_IFACE "wlan0"
#define DEFAULT_UDP_PORT 47101

// Default IP address for the connected auxiliary device
#define DEFAULT_AUX_IP "192.168.88.2"

// Valid options
// Any new option should be handled in the switch-case inside parse_options() and the corresponding char should be added to VALID_OPTS
// If an option accepts an additional argument, it is followed by ':'
#define VALID_OPTS "hvZ:z:w:L:gi:p:R:E"

#define INIT_CODE 0xAE

#define INVALID_LONLAT -DBL_MAX

typedef struct options {
	// = INIT_CODE if 'struct options' has been initialized via options_initialize()
	uint8_t init_code;

	double central_lat;
	double central_lon;

	options_string vehviz_nodejs_addr; // Advanced option: IPv4 address for the UDP connection to the Node.js server (excluding the port number)
	long vehviz_nodejs_port; // Advanced option: port number for the UDP connection to the Node.js server

	long vehviz_web_interface_port; // Port number for the Vehicle Visualizer web interface

	options_string logfile_name; // Name of the log file where performance data should be store (if specified)

	// UDP SocketClient options
	options_string udp_interface;
	long udp_port;

	double vehviz_update_interval_sec; // Advanced option: modifies the update rate of the web-based GUI. Warning: decreasing this too much will affect performance! This value cannot be less than 0.05 s and more than 1 s.

	bool ageCheck_enabled; // (-g option to set this to 'false') 'true' if an 'age check' on the received data should be performed before updating the database, 'false' otherwise. Default: 'true'.

	// Disabled as the RSSI information is sent by the other device, and not directly retrieved by us
	// double rssi_aux_update_interval_msec; // If set to <=0 (default value), the auxialiary RouterOS-based device RSSI retrieval will be disabled

	options_string auxiliary_device_ip_addr;

	bool enable_enhanced_CAMs;
} options_t;

void options_initialize(struct options *options);
unsigned int parse_options(int argc, char **argv, struct options *options);
void options_free(struct options *options);

#endif // OPTIONSMODULE_H_INCLUDED
