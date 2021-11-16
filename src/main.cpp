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

	// Open a socket for the reception of UDP messages
	int udp_sockfd=-1;
	udp_sockfd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);

	if(udp_sockfd<0) {
		fprintf(stderr,"Critical error: cannot open UDP socket for message reception.\n");
		terminatorFlag = true;
	} else {
		fprintf(stdout,"Socket opened. Descriptor: %d\n",udp_sockfd);
		// Get the IP address of the dissemination interface
		struct ifreq ifreq;
		struct in_addr dissem_vif_addr;
		strncpy(ifreq.ifr_name,options_string_pop(aim_opts.udp_interface),IFNAMSIZ);

		ifreq.ifr_addr.sa_family=AF_INET;

		if(ioctl(udp_sockfd,SIOCGIFADDR,&ifreq)!=-1) {
			dissem_vif_addr=((struct sockaddr_in*)&ifreq.ifr_addr)->sin_addr;
		} else {
			fprintf(stderr,"Critical error: cannot find an IP address for interface: %s\n",options_string_pop(aim_opts.udp_interface));
			terminatorFlag = true;
		}

		if(terminatorFlag==false) {
			// Bind UDP socket
			struct sockaddr_in bindSockAddr;
			memset(&bindSockAddr,0,sizeof(struct sockaddr_in));
			bindSockAddr.sin_family=AF_INET;
			bindSockAddr.sin_port=htons(aim_opts.udp_port);
			bindSockAddr.sin_addr.s_addr=INADDR_ANY;

			errno=0;
			if(bind(udp_sockfd,(struct sockaddr *) &bindSockAddr,sizeof(bindSockAddr))<0) {
				fprintf(stderr,"Critical error: cannot bind the UDP socket for the reception of V2X messages. Error: %s\n",strerror(errno));
				exit(EXIT_FAILURE);
			}

			// Create the main SocketClient object for the reception of the V2X messages
			SocketClient mainRecvClient(udp_sockfd,&aim_opts, db_ptr, logfile_name);

			// Start the reception of V2X messages
			mainRecvClient.startReception();
		}
	}

	pthread_join(dbcleaner_tid,nullptr);
	pthread_join(vehviz_tid,nullptr);

	// Close the socket
	close(udp_sockfd);

	db_ptr->clear();

	// Freeing the options
	options_free(&aim_opts);

	return 0;
}