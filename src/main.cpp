#include <atomic>
#include <iostream>
#include <unistd.h>
#include <condition_variable>
#include <thread>

#include "LDMmap.h"
#include "vehicle-visualizer.h"
#include "SocketClient.h"
#include "utils.h"

// Linux net includes
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>

extern "C" {
	#include "options.h"
}

#include "etsiDecoderFrontend.h"

#define DB_CLEANER_INTERVAL_SECONDS 5
#define DB_DELETE_OLDER_THAN_SECONDS 7 // This value should NEVER be set greater than (5-DB_CLEANER_INTERVAL_SECONDS/60) minutes or (300-DB_CLEANER_INTERVAL_SECONDS) seconds - doing so may break the database age check functionality!

// Global atomic flag to terminate all the threads in case of errors
std::atomic<bool> terminatorFlag;

// Global pointer to a visualizer object (to be accessed by both DBcleaner_callback() and VehVizUpdater_callback())
vehicleVisualizer* globVehVizPtr=nullptr;
// Global mutex (plus condition variable) to synchronize the threads using the object pointer defined above
std::mutex syncmtx;
std::condition_variable synccv;

typedef struct vizOptions {
	ldmmap::LDMMap *db_ptr;
	options_t *opts_ptr;
} vizOptions_t;

void clearVisualizerObject(uint64_t id,void *vizObjVoidPtr) {
	vehicleVisualizer *vizObjPtr = static_cast<vehicleVisualizer *>(vizObjVoidPtr);

	vizObjPtr->sendObjectClean(std::to_string(id));
}

void *DBcleaner_callback(void *arg) {
	// Get the pointer to the database
	ldmmap::LDMMap *db_ptr = static_cast<ldmmap::LDMMap *>(arg);

	// Create a new timer
	struct pollfd pollfddata;
	int clockFd;

	std::cout << "[INFO] Database cleaner started. The DB will be garbage collected every " << DB_CLEANER_INTERVAL_SECONDS << " seconds." << std::endl;

	if(timer_fd_create(pollfddata, clockFd, DB_CLEANER_INTERVAL_SECONDS*1e6)<0) {
		std::cerr << "[ERROR] Fatal error! Cannot create timer for the DB cleaner thread!" << std::endl;
		terminatorFlag = true;
		pthread_exit(nullptr);
	}

	std::unique_lock<std::mutex> synclck(syncmtx);
	synccv.wait(synclck);

	POLL_DEFINE_JUNK_VARIABLE();

	while(terminatorFlag == false) {
		if(poll(&pollfddata,1,0)>0) {
			POLL_CLEAR_EVENT(clockFd);

			// ---- These operations will be performed periodically ----

			// db_ptr->deleteOlderThan(DB_DELETE_OLDER_THAN_SECONDS*1e3);
			db_ptr->deleteOlderThanAndExecute(DB_DELETE_OLDER_THAN_SECONDS*1e3,clearVisualizerObject,static_cast<void *>(globVehVizPtr));

			// --------

		}
	}

	if(terminatorFlag == true) {
		std::cerr << "[WARN] Database cleaner terminated due to error." << std::endl;
	}

	close(clockFd);

	pthread_exit(nullptr);
}

void updateVisualizer(ldmmap::vehicleData_t vehdata,void *vizObjVoidPtr) {
	vehicleVisualizer *vizObjPtr = static_cast<vehicleVisualizer *>(vizObjVoidPtr);

	vizObjPtr->sendObjectUpdate(std::to_string(vehdata.stationID),vehdata.lat,vehdata.lon,static_cast<int>(vehdata.stationType),vehdata.heading);
}

void *VehVizUpdater_callback(void *arg) {
	// Get the pointer to the visualizer options/parameters
	vizOptions_t *vizopts_ptr = static_cast<vizOptions_t *>(arg);
	// Get a direct pointer to the database
	ldmmap::LDMMap *db_ptr = vizopts_ptr->db_ptr;

	// Get the central lat and lon values stored in the DB
	std::pair<double,double> centralLatLon= db_ptr->getCentralLatLon();

	// Create a new veheicle visualizer object reading the (IPv4) address and port from the options (the default values are set as a macro in options/options.h)
	vehicleVisualizer vehicleVisObj(vizopts_ptr->opts_ptr->vehviz_nodejs_port,std::string(options_string_pop(vizopts_ptr->opts_ptr->vehviz_nodejs_addr)));

	// Start the node.js server and perform an initial connection with it
	vehicleVisObj.setHTTPPort(vizopts_ptr->opts_ptr->vehviz_web_interface_port);
	vehicleVisObj.startServer();
	vehicleVisObj.connectToServer ();
	vehicleVisObj.sendMapDraw(centralLatLon.first, centralLatLon.second);

	globVehVizPtr=&vehicleVisObj;

	synccv.notify_all();

	// Create a new timer
	struct pollfd pollfddata;
	int clockFd;

	std::cout << "[INFO] Vehicle visualizer updater started. Updated every " << vizopts_ptr->opts_ptr->vehviz_update_interval_sec << " seconds." << std::endl;

	if(timer_fd_create(pollfddata, clockFd, vizopts_ptr->opts_ptr->vehviz_update_interval_sec*1e6)<0) {
		std::cerr << "[ERROR] Fatal error! Cannot create timer for the Vehicle Visualizer update thread!" << std::endl;
		terminatorFlag = true;
		pthread_exit(nullptr);
	}

	POLL_DEFINE_JUNK_VARIABLE();

	while(terminatorFlag == false) {
		if(poll(&pollfddata,1,0)>0) {
			POLL_CLEAR_EVENT(clockFd);

			// ---- These operations will be performed periodically ----

			db_ptr->executeOnAllContents(&updateVisualizer, static_cast<void *>(&vehicleVisObj));

			// --------

		}
	}

	if(terminatorFlag == true) {
		std::cerr << "[WARN] Vehicle visualizer updater terminated due to error." << std::endl;
	}

	close(clockFd);

	pthread_exit(nullptr);
}

int main(int argc, char **argv) {
	terminatorFlag = false;

	// DB cleaner thread ID
	pthread_t dbcleaner_tid;
	// Vehicle visualizer update thread ID
	pthread_t vehviz_tid;
	// Thread attributes (unused, for the time being)
	// pthread_attr_t tattr;

	// First of all, parse the options
	options_t aim_opts;

	// Read options from command line
	options_initialize(&aim_opts);
	if(parse_options(argc, argv, &aim_opts)) {
		fprintf(stderr,"Error while parsing the options.\n");
		exit(EXIT_FAILURE);
	}

	fprintf(stdout,"[INFO] AIM version: %s\n",VERSION_STR);

	// Create a new DB object
	ldmmap::LDMMap *db_ptr = new ldmmap::LDMMap();

	// Set a central latitude and longitude values to be used only for visualization purposes -
	// - it does not affect in any way the performance or the operations of the LDMMap DB module)
	db_ptr->setCentralLatLon(aim_opts.central_lat,aim_opts.central_lon);

	// Before starting the UDP client reception loop, we should create a parallel thread, reading periodically 
	// (e.g. every 5 s) the database through the pointer "db_ptr" and "cleaning" the entries which are too old
	// pthread_attr_init(&tattr);
	// pthread_attr_setdetachstate(&tattr,PTHREAD_CREATE_DETACHED);
	pthread_create(&dbcleaner_tid,NULL,DBcleaner_callback,(void *) db_ptr);
	// pthread_attr_destroy(&tattr);

	// We should also start here a second parallel thread, reading periodically the database (e.g. every 500 ms) and sending the vehicle data to
	// the vehicleVisualizer
	// pthread_attr_init(&tattr);
	// pthread_attr_setdetachstate(&tattr,PTHREAD_CREATE_DETACHED);
	vizOptions_t vizParams = {db_ptr,&aim_opts};
	pthread_create(&vehviz_tid,NULL,VehVizUpdater_callback,(void *) &vizParams);
	// pthread_attr_destroy(&tattr);

	// Get the log file name from the options, if available, to enable log mode inside the UDP SocketClient
	std::string logfile_name="";
	if(options_string_len(aim_opts.logfile_name)>0) {
		logfile_name=std::string(options_string_pop(aim_opts.logfile_name));
	}

	// Open a socket for the reception of raw (BTP + GeoNetworking) messages
	int raw_sockfd=-1;
	// sockfd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP); // Old UDP socket, no more used
	raw_sockfd=socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL));

	if(raw_sockfd<0) {
		fprintf(stderr,"Critical error: cannot open raw socket for message reception (expecting CAMs over BTP and GeoNetworking).\n");
		terminatorFlag = true;
	} else {
		fprintf(stdout,"Socket opened. Descriptor: %d\n",raw_sockfd);

		// Get the MAC address of the reception interface
		struct ifreq ifreq;
		uint8_t dissem_vif_mac[6]={0};
		strncpy(ifreq.ifr_name,options_string_pop(aim_opts.udp_interface),IFNAMSIZ); 
		if(ioctl(raw_sockfd,SIOCGIFHWADDR,&ifreq)!=-1) {
			memcpy(dissem_vif_mac,ifreq.ifr_hwaddr.sa_data,6);
		} else {
			std::cerr << "Critical error: cannot find a MAC address for interface: " << options_string_pop(aim_opts.udp_interface) << std::endl;
			exit(EXIT_FAILURE);
		}

		if(terminatorFlag==false) {
			// Bind the raw socket
			// 1. Get the index of the dissemination interface
			int ifindex=if_nametoindex(options_string_pop(aim_opts.udp_interface));
			if(ifindex<1) {
				std::cerr << "Critical error: cannot find an interface index for interface: " << options_string_pop(aim_opts.udp_interface) << std::endl;
				exit(EXIT_FAILURE);
			}

			// 2. Bind to the desired interface, by leveraging the previously obtained interface index
			struct sockaddr_ll bindSockAddr;
			memset(&bindSockAddr,0,sizeof(bindSockAddr));
			bindSockAddr.sll_ifindex=ifindex;
			bindSockAddr.sll_family=AF_PACKET;
			bindSockAddr.sll_protocol=htons(ETH_P_ALL);

			errno=0;
			if(bind(raw_sockfd,(struct sockaddr *) &bindSockAddr,sizeof(bindSockAddr))<0) {
				fprintf(stderr,"Critical error: cannot bind the raw socket for the reception of V2X messages. Error: %s\n",strerror(errno));
				exit(EXIT_FAILURE);
			}

			// Create the main SocketClient object for the reception of the V2X messages
			SocketClient mainRecvClient(raw_sockfd,&aim_opts, db_ptr, logfile_name);

			// Set the "self" MAC address, so that all the messages coming from this address will be discarded
			mainRecvClient.setSelfMAC(dissem_vif_mac);

			fprintf(stdout,"Reception is going to start very soon...\n");

			// Start the reception of V2X messages
			mainRecvClient.startReception();
		}
	}

	pthread_join(dbcleaner_tid,nullptr);
	pthread_join(vehviz_tid,nullptr);

	// Close the socket
	close(raw_sockfd);

	db_ptr->clear();

	// Freeing the options
	options_free(&aim_opts);

	return 0;
}