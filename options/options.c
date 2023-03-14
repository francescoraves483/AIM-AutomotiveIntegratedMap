#include "options.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

// Names for the long options
#define LONGOPT_h "help"
#define LONGOPT_v "version"
#define LONGOPT_Z "vehviz-nodejs-address"
#define LONGOPT_z "vehviz-nodejs-port"
#define LONGOPT_w "vehviz-web-port"
#define LONGOPT_L "log-file-name"
#define LONGOPT_g "disable-age-check"
#define LONGOPT_i "interface"
#define LONGOPT_p "port"
#define LONGOPT_A "central-zone"
//#define LONGOPT_R "rssi-aux-retrieval-periodicity"
#define LONGOPT_E "enable-enhanced-CAMs"
#define LONGOPT_D "print-denms"
#if GPSD_ENABLED
#define LONGOPT_P "print-gnss-position"
#endif

// Long-only options
#define LONGOPT_vehviz_update_interval_sec "vehviz-update-interval"
#define LONGOPT_aux_dev_addr "aux-dev-addr"
// The corresponding "val"s are used internally and they should be set as sequential integers starting from 256 (the range 320-399 should not be used as it is reserved to the AMQP broker long options)
#define LONGOPT_vehviz_update_interval_sec_val 256
#define LONGOPT_aux_dev_addr_val 257

#define LONGOPT_STR_CONSTRUCTOR(LONGOPT_STR) "  --"LONGOPT_STR"\n"

#define STRINGIFY(value) STR(value)
#define STR(value) #value

// Long options "struct option" array for getopt_long
static const struct option long_opts[]={
	{LONGOPT_h,				no_argument, 		NULL, 'h'},
	{LONGOPT_v,				no_argument, 		NULL, 'v'},
	{LONGOPT_i,				required_argument,	NULL, 'i'},
	{LONGOPT_p,				required_argument,	NULL, 'p'},
	{LONGOPT_Z,				required_argument,	NULL, 'Z'},
	{LONGOPT_z,				required_argument,	NULL, 'z'},
	{LONGOPT_w,				required_argument,	NULL, 'w'},
	{LONGOPT_L,				required_argument,	NULL, 'L'},
	{LONGOPT_A,				required_argument,	NULL, 'A'},
	{LONGOPT_g,				no_argument,		NULL, 'g'},
//	{LONGOPT_R,				required_argument,	NULL, 'R'},
	{LONGOPT_E,				no_argument,		NULL, 'E'},
	{LONGOPT_D,				no_argument,		NULL, 'D'},
	#if GPSD_ENABLED
	{LONGOPT_P,				required_argument,	NULL, 'P'},
	#endif

	{LONGOPT_vehviz_update_interval_sec,	required_argument,	NULL, LONGOPT_vehviz_update_interval_sec_val},
	{LONGOPT_aux_dev_addr,					required_argument,	NULL, LONGOPT_aux_dev_addr_val},

	{NULL, 0, NULL, 0}
};

// Option strings: define here the description for each option to be then included inside print_long_info()
#define OPT_Z_description \
	LONGOPT_STR_CONSTRUCTOR(LONGOPT_Z) \
	"  -Z: advanced option: set the IPv4 address for the UDP connection to the Vehicle Visualizer\n" \
	"\t  Node.js server (excluding the port number).\n" \
	"\t  This is the address without port number. Default: ("DEFAULT_VEHVIZ_NODEJS_UDP_ADDR").\n"

#define OPT_z_description \
	LONGOPT_STR_CONSTRUCTOR(LONGOPT_z) \
	"  -z: advanced option: set the port number for the UDP connection to the Vehicle Visualizer\n" \
	"\t  Node.js server.\n" \
	"\t  Default: ("STRINGIFY(DEFAULT_VEHVIZ_NODEJS_UDP_PORT)").\n"

#define OPT_w_description \
	LONGOPT_STR_CONSTRUCTOR(LONGOPT_w) \
	"  -w: set the port at which the web interface of the Vehicle Visualizer will be available.\n" \
	"\t  Default: ("STRINGIFY(DEFAULT_VEHVIZ_WEB_PORT)").\n"

#define OPT_L_description \
	LONGOPT_STR_CONSTRUCTOR(LONGOPT_L) \
	"  -L: enable log mode and set the name of the textual file where the data will be saved.\n" \
	"\t  'stdout' can be specified to output the log data on the screen. Default: (disabled).\n"

#define OPT_g_description \
	LONGOPT_STR_CONSTRUCTOR(LONGOPT_g) \
	"  -g: disable data age check when updating the database. When this option is active, the latest.\n" \
	"\t  received data is always saved to the database, for each object, no matter if the timestamps\n" \
	"\t  are older than the one of the data already stored.\n" \
	"\t  When this option is not specified, GeoNetworking timestamps are used to understand if the\n" \
	"\t  received data is up-to-date and should be saved to the database or not.\n"

#define OPT_vehviz_update_interval_sec_description \
	"  --"LONGOPT_vehviz_update_interval_sec" <interval in seconds>: advanced option: this option can be used to\n" \
	"\t  modify the update rate of the web-based GUI. \n" \
	"\t  Warning: decreasing too much this value will affect the S-LDM database performance!\n" \
	"\t  This value cannot be less than 0.05 s and more than 1 s. Default: ("STRINGIFY(DEFAULT_VEHVIZ_UPDATE_INTERVAL_SECONDS)")\n"

#define OPT_i_description \
	LONGOPT_STR_CONSTRUCTOR(LONGOPT_i) \
	"  -i: set the interface name from which messages should be received.\n" \
	"\t  Default: ("DEFAULT_IFACE").\n"

#define OPT_D_description \
	LONGOPT_STR_CONSTRUCTOR(LONGOPT_D) \
	"  -D: enable reception of DENMs. Received DENMs, for the time being, are only used to log\n" \
	"\t  some basic information for each message, and not yet to update the internal database.\n" \
	"\t  DENM reception is not enabled by default.\n"

#if GPSD_ENABLED
#define OPT_P_description \
	LONGOPT_STR_CONSTRUCTOR(LONGOPT_P) \
	"  -P <gpsd_port>: enable position logging. This option works only if the DENM reception is enabled,\n" \
	"\t  for the time being. Specifying this option will make AIM log the position of the vehicle\n" \
	"\t  for each received DENM. Specifying 0 as port number, will make AIM use the default port "STRINGIFY(GPSD_DEFAULT_PORT)".\n"
#endif

#define OPT_p_description \
	LONGOPT_STR_CONSTRUCTOR(LONGOPT_p) \
	"  -p: set the UDP port from which messages should be received.\n" \
	"\t  Default: ("STRINGIFY(DEFAULT_UDP_PORT)").\n"

#define OPT_A_description \
	LONGOPT_STR_CONSTRUCTOR(LONGOPT_A) \
	"  -A: set a central latitude and longitude around which the Vehicle Visualizer map will be drawn.\n" \
	"\t  This option is thought solely for visualization purposes. Default: ("STRINGIFY(DEFAULT_CENTRAL_LAT)"-"STRINGIFY(DEFAULT_CENTRAL_LON)").\n"

#define OPT_E_description \
	LONGOPT_STR_CONSTRUCTOR(LONGOPT_E) \
	"  -E: enable decoding of experimental enhanced CAMs containing an additional container with computational load and\n" \
	"\t  wireless channel information. This type of CAMs is identical to the standard one, with the addition of the\n" \
	"\t  optional additional container. It has been defined as part of an experimental research activity. Thus, do\n" \
	"\t  not enable this option unless you know what you are doing. Any fully standard-compliant usage of AIM should\n" \
	"\t  not specify this option. Default: (not specified).\n"

static void print_long_info(char *argv0) {
	fprintf(stdout,"\nUsage: %s [options]\n"
		"%s [-h | --"LONGOPT_h"]: print help and show options\n"
		"%s [-v | --"LONGOPT_v"]: print version information\n\n"

		"[options]:\n"
		OPT_i_description
		OPT_p_description
		OPT_A_description
		OPT_Z_description
		OPT_z_description
		OPT_w_description
		OPT_L_description
		OPT_g_description
		// OPT_R_description
		OPT_E_description
		OPT_D_description
		#if GPSD_ENABLED
		OPT_P_description
		#endif
		OPT_vehviz_update_interval_sec_description
 		,
		argv0,argv0,argv0);

	exit(EXIT_SUCCESS);
}

static void print_short_info_err(struct options *options,char *argv0) {
	options_free(options);

	fprintf(stdout,"\nUsage: %s [options]\n"
		"%s [-h | --"LONGOPT_h"]: print help and show options\n"
		"%s [-v | --"LONGOPT_v"]: print version information\n\n"
		,
		argv0,argv0,argv0);

	exit(EXIT_SUCCESS);
}

void options_initialize(struct options *options) {
	options->init_code=INIT_CODE;

	// Initialize here your options
	options->central_lon=DEFAULT_CENTRAL_LAT;
	options->central_lat=DEFAULT_CENTRAL_LON;

	options->vehviz_nodejs_addr=options_string_declare();
	options->vehviz_nodejs_port=DEFAULT_VEHVIZ_NODEJS_UDP_PORT;

	options->udp_interface=options_string_declare();
	options->udp_port=DEFAULT_UDP_PORT;

	options->vehviz_web_interface_port=DEFAULT_VEHVIZ_WEB_PORT;

	options->logfile_name=options_string_declare();

	options->vehviz_update_interval_sec=DEFAULT_VEHVIZ_UPDATE_INTERVAL_SECONDS;

	options->ageCheck_enabled=true;

//	options->rssi_aux_update_interval_msec=-1; // Disabled by default

	options->auxiliary_device_ip_addr=options_string_declare();

	options->enable_enhanced_CAMs=false;

	options->denm_printing_enabled=false;

	#if GPSD_ENABLED
	options->position_printing_enabled=false;
	options->position_printing_gnss_port=GPSD_DEFAULT_PORT;
	#endif
}

unsigned int parse_options(int argc, char **argv, struct options *options) {
	int char_option;
	bool version_flg=false;
	char *sPtr; // String pointer for strtod() calls

	if(options->init_code!=INIT_CODE) {
		fprintf(stderr,"parse_options: you are trying to parse the options without initialiting\n"
			"struct options, this is not allowed.\n");
		return 1;
	}

	while ((char_option=getopt_long(argc, argv, VALID_OPTS, long_opts, NULL)) != EOF) {
		switch(char_option) {
			case 0:
				fprintf(stderr,"Error. An unexpected error occurred when parsing the options.\n"
					"Please report to the developers that getopt_long() returned 0. Thank you.\n");
				exit(EXIT_FAILURE);
				break;

			case 'A':
				if(sscanf(optarg,"%lf:%lf",
					&options->central_lat,
					&options->central_lon)<2) {

					fprintf(stderr,"Error parsing the central latitude and longitude values after the --central-zone/-A option.");
					print_short_info_err(options,argv[0]);
				}
				break;

			case 'Z':
				if(!options_string_push(&(options->vehviz_nodejs_addr),optarg)) {
					fprintf(stderr,"Error in parsing the IPv4 address for the UDP connection to the Vehicle Visualizer Node.js server.\n");
					print_short_info_err(options,argv[0]);
				}
				break;

			case 'z':
				errno=0; // Setting errno to 0 as suggested in the strtol() man page
				options->vehviz_nodejs_port=strtol(optarg,&sPtr,10);

				if(sPtr==optarg) {
					fprintf(stderr,"Cannot find any digit in the specified value (-z/" LONGOPT_z ").\n");
					print_short_info_err(options,argv[0]);
				} else if(errno || options->vehviz_nodejs_port<1 || options->vehviz_nodejs_port>65535) {
					// Only port numbers from 1 to 65535 are valid and can be accepted
					fprintf(stderr,"Error in parsing the port number for the UDP connection to the Vehicle Visualizer Node.js server.\n");
					print_short_info_err(options,argv[0]);
				}
				break;

			case 'w':
				errno=0; // Setting errno to 0 as suggested in the strtol() man page
				options->vehviz_web_interface_port=strtol(optarg,&sPtr,10);

				if(sPtr==optarg) {
					fprintf(stderr,"Cannot find any digit in the specified value (-w/" LONGOPT_w ").\n");
					print_short_info_err(options,argv[0]);
				} else if(errno || options->vehviz_web_interface_port<1 || options->vehviz_web_interface_port>65535) {
					// Only port numbers from 1 to 65535 are valid and can be accepted
					fprintf(stderr,"Error in parsing the port number for the web interface of the Vehicle Visualizer.\n");
					print_short_info_err(options,argv[0]);
				}
				break;

			case 'L':
				if(!options_string_push(&(options->logfile_name),optarg)) {
					fprintf(stderr,"Error in parsing the log file name: %s.\n",optarg);
					print_short_info_err(options,argv[0]);
				}
				break;

			case 'i':
				if(!options_string_push(&(options->udp_interface),optarg)) {
					fprintf(stderr,"Error in parsing the interface name: %s.\n",optarg);
					print_short_info_err(options,argv[0]);
				}
				break;

			// case 'R':
			// 	errno=0; // Setting errno to 0 as suggested in the strtod() man page
			// 	options->rssi_aux_update_interval_msec=strtod(optarg,&sPtr);

			// 	if(sPtr==optarg) {
			// 		fprintf(stderr,"Cannot find any digit in the specified value (-R/--" LONGOPT_R ").\n");
			// 		print_short_info_err(options,argv[0]);
			// 	} else if(errno || options->rssi_aux_update_interval_msec>10000.0) {
			// 		fprintf(stderr,"Error in parsing the refresh rate for the auxiliary device RSSI retrieval. Remember that it must be smaller than 10 seconds (10000 ms).\n");
			// 		print_short_info_err(options,argv[0]);
			// 	}
			// 	break;

			case 'p':
				errno=0; // Setting errno to 0 as suggested in the strtol() man page
				options->udp_port=strtol(optarg,&sPtr,10);

				if(sPtr==optarg) {
					fprintf(stderr,"Cannot find any digit in the specified value (-p/" LONGOPT_p ").\n");
					print_short_info_err(options,argv[0]);
				} else if(errno || options->udp_port<1 || options->udp_port>65535) {
					// Only port numbers from 1 to 65535 are valid and can be accepted
					fprintf(stderr,"Error in parsing the port number for the receiving UDP socket.\n");
					print_short_info_err(options,argv[0]);
				}
				break;

			case 'g':
				if(options->ageCheck_enabled==false) {
					fprintf(stderr,"Error. The age check (-g/--"LONGOPT_g") has already been disabled once.\n");
					print_short_info_err(options,argv[0]);
				}

				options->ageCheck_enabled=false;
				break;

			case 'E':
				if(options->enable_enhanced_CAMs==true) {
					fprintf(stderr,"Error. The enhanced CAMs decoding (-E/--"LONGOPT_E") has already been enabled once.\n");
					print_short_info_err(options,argv[0]);
				}

				options->enable_enhanced_CAMs=true;
				break;

			case 'D':
				if(options->denm_printing_enabled==true) {
					fprintf(stderr,"Error. The DENM printing feature (-D/--"LONGOPT_D") has already been enabled once.\n");
					print_short_info_err(options,argv[0]);
				}

				options->denm_printing_enabled=true;
				break;

			#if GPSD_ENABLED
			case 'P':
				if(options->position_printing_enabled==true) {
					fprintf(stderr,"Error. The position printing feature (-P/--"LONGOPT_P") has already been enabled once.\n");
					print_short_info_err(options,argv[0]);
				}

				options->position_printing_enabled=true;

				// Parse the port
				errno=0; // Setting errno to 0 as suggested in the strtol() man page
				options->position_printing_gnss_port=strtol(optarg,&sPtr,10);

				if(sPtr==optarg) {
					fprintf(stderr,"Cannot find any digit in the specified value (-P/" LONGOPT_P ").\n");
					print_short_info_err(options,argv[0]);
				} else if(errno || options->position_printing_gnss_port<0 || options->position_printing_gnss_port>65535) {
					// Only port numbers from 1 to 65535 are valid and can be accepted ('0' will make, instead, AIM use the default port GPSD_DEFAULT_PORT)
					fprintf(stderr,"Error in parsing the port number for connecting to gpsd.\n");
					print_short_info_err(options,argv[0]);
				}

				if(options->position_printing_gnss_port==0) {
					options->position_printing_gnss_port=GPSD_DEFAULT_PORT;
				}
				break;
			#endif

			case LONGOPT_vehviz_update_interval_sec_val:
				errno=0; // Setting errno to 0 as suggested in the strtod() man page
				options->vehviz_update_interval_sec=strtod(optarg,&sPtr);

				if(sPtr==optarg) {
					fprintf(stderr,"Cannot find any digit in the specified value (--" LONGOPT_vehviz_update_interval_sec ").\n");
					print_short_info_err(options,argv[0]);
				} else if(errno || options->vehviz_update_interval_sec<0.05 || options->vehviz_update_interval_sec>1.0) {
					fprintf(stderr,"Error in parsing the update rate for the web-based GUI. Remember that it must be within [0.05,1] seconds.\n");
					print_short_info_err(options,argv[0]);
				}
				break;

			case LONGOPT_aux_dev_addr_val:
				if(!options_string_push(&(options->auxiliary_device_ip_addr),optarg)) {
					fprintf(stderr,"Error in parsing the auxiliary communication device IP address: %s.\n",optarg);
					print_short_info_err(options,argv[0]);
				}
				break;

			case 'v':
				version_flg = true;
				break;

			case 'h':
				print_long_info(argv[0]);
				break;

			default:
				print_short_info_err(options,argv[0]);
				return 1;

		}

	}

	if(version_flg==true) {
		fprintf(stdout,"Version: %s\n",VERSION_STR);
		exit(EXIT_SUCCESS);
	}

	// Set the default values for the "options_string" options, if they have not been set before
	// An unset "options_string" has a length which is <= 0
	if(options_string_len(options->vehviz_nodejs_addr)<=0) {
		if(!options_string_push(&(options->vehviz_nodejs_addr),DEFAULT_VEHVIZ_NODEJS_UDP_ADDR)) {
			fprintf(stderr,"Error! Cannot set the default IPv4 address for the UDP connection to the Vehicle Visualizer Node.js server.\nPlease report this bug to the developers.\n");
			exit(EXIT_FAILURE);
		}
	}

	if(options_string_len(options->udp_interface)<=0) {
		if(!options_string_push(&(options->udp_interface),DEFAULT_IFACE)) {
			fprintf(stderr,"Error! Cannot set the default interface to receive the messages from.\nPlease report this bug to the developers.\n");
			exit(EXIT_FAILURE);
		}
	}

	return 0;
}

void options_free(struct options *options) {
	// If you allocated any memory with malloc() (remember that this module is written in C, not C++)
	// or if you used "options_string_push", you need to free the allocated memory here, either with
	// free() (for "manual" malloc()) or with options_string_free() if you are using "options_string"
	// data structures

	options_string_free(options->udp_interface);
	options_string_free(options->vehviz_nodejs_addr);
	options_string_free(options->auxiliary_device_ip_addr);
}
