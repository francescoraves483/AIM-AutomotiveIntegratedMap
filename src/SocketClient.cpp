#include "SocketClient.h"
#include "utils.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <poll.h>
#include <unistd.h>
#include <linux/if_packet.h> // Needed for "struct sockaddr_ll"
#include <net/ethernet.h> // Needed for "struct ether_header"
#include <linux/wireless.h>
#include <sys/ioctl.h>

#define lowFreqContainerManager(ptr_name,stationID) \
		if(ptr_name->cam.camParameters.lowFrequencyContainer!=NULL) { \
		      if(ptr_name->cam.camParameters.lowFrequencyContainer->choice.basicVehicleContainerLowFrequency.exteriorLights.buf!=NULL) { \
		              return ldmmap::OptionalDataItem<uint8_t>(ptr_name->cam.camParameters.lowFrequencyContainer->choice.basicVehicleContainerLowFrequency.exteriorLights.buf[0]); \
		      } else { \
		              return ldmmap::OptionalDataItem<uint8_t>(false); \
		      } \
		} else { \
		      ldmmap::LDMMap::returnedVehicleData_t retveh; \
		      if(m_db_ptr->lookup(stationID,retveh)==ldmmap::LDMMap::LDMMAP_OK) { \
		              return retveh.vehData.exteriorLights; \
		      } else { \
		              return ldmmap::OptionalDataItem<uint8_t>(false); \
		      } \
		}

void
SocketClient::routerOS_RSSI_retriever(void) {
	// Create a new timer
	struct pollfd pollfddata;
	int clockFd;

	// Popen buffer
	char popen_buff[2000];

	if(timer_fd_create(pollfddata, clockFd, m_opts_ptr->rssi_aux_update_interval_msec*1e3)<0) {
		std::cerr << "[ERROR] Fatal error! Cannot create timer for the routerOS RSSI retriever thread. No RSSI data will be available." << std::endl;
		m_routeros_rssi={};
		return;
	}

	POLL_DEFINE_JUNK_VARIABLE();

	while(m_terminate_routeros_rssi_flag==false) {
		if(poll(&pollfddata,1,0)>0) {
			POLL_CLEAR_EVENT(clockFd);

			// Original command via ssh: ssh admin@192.168.88.2 interface w60g monitor wlan60-1 once | grep -E "rssi|remote-address"
			std::string ssh_command = "stdbuf -o L ssh admin@" + std::string(options_string_pop(m_opts_ptr->auxiliary_device_ip_addr)) + " interface w60g monitor wlan60-1 once | stdbuf -o L grep -E \"rssi|remote-address\"";
			FILE *ssh = popen(ssh_command.c_str(),"r");

			if(ssh==NULL) {
				// Sleep at least 1 second, and then try again after a timer expiration
				sleep(1);
				continue;
			}

			std::vector<std::string> m_remotes;

			while(fgets(popen_buff,2000,ssh)!=NULL) {
				char* pch;
				if(strstr(popen_buff,"remote-address")) {
					m_remotes.clear();

					pch=strtok(popen_buff,":");
					pch=strtok(NULL," ,");

					while(pch!=nullptr) {
						std::string pchstr=std::string(pch);
						pchstr.erase(std::remove_if(pchstr.begin(), pchstr.end(), isspace), pchstr.end());
						m_remotes.push_back(pchstr);

						pch=strtok(NULL, " ,");
				  	}
				} else if(strstr(popen_buff,"rssi")) {
					pch = strtok (popen_buff,":");
					pch = strtok (NULL," ,");

					m_routeros_rssi_mutex.lock();
					for(int i=0;i<m_remotes.size() && pch!=NULL;i++) {
						m_routeros_rssi[m_remotes[i]]=atoi(pch);
						pch=strtok(NULL, " ,");
					}
					m_routeros_rssi_mutex.unlock();
				}
			}
		}
	}

	close(clockFd);
}

// If this is a full ITS message manage the low frequency container data
// Check if this CAM contains the low frequency container
// and if the ext. lights hack for older versions CAMs is disable
// If yes, store the exterior lights status
// If not, check if an older information about the exterior lights of the current vehicle already exist in the database (using m_db_ptr->lookup()),
// if this data exists, use this data, if not, just set the exterior lights information as unavailable
inline ldmmap::OptionalDataItem<uint8_t>
SocketClient::manage_LowfreqContainer(void *decoded_cam_void,uint32_t stationID) {
	if(m_opts_ptr->enable_enhanced_CAMs==true) {
		CAMEnhanced_t *decoded_cam = (CAMEnhanced_t *) decoded_cam_void;
		lowFreqContainerManager(decoded_cam,stationID);
	} else {
		CAM_t *decoded_cam = (CAM_t *) decoded_cam_void;
		lowFreqContainerManager(decoded_cam,stationID);
	}
}

inline bool compare_mac(uint8_t mac_a[6],uint8_t mac_b[6]) {
	return (mac_a[0]==mac_b[0] && 
		mac_a[1]==mac_b[1] && 
		mac_a[2]==mac_b[2] && 
		mac_a[3]==mac_b[3] && 
		mac_a[4]==mac_b[4] && 
		mac_a[5]==mac_b[5]);
}

void
SocketClient::rxThr(void) {
	size_t msglen=-1;
	uint8_t msgbuf[MSGBUF_MAXSIZ];

	struct sockaddr_ll addrll;
	socklen_t addrLen=sizeof(addrll);

	m_receptionInProgress=true;

	struct pollfd socketMon[2];

	// Monitor the socket
	socketMon[0].fd=m_raw_rx_sock;
	socketMon[0].revents=0;
	socketMon[0].events=POLLIN;

	// Monitor the "unlock pipe"
	socketMon[1].fd=m_unlock_pd_rd;
	socketMon[1].revents=0;
	socketMon[1].events=POLLIN;

	fprintf(stdout,"[INFO] Message reception started on interface: %s:%ld. Socket: %d.\n",options_string_pop(m_opts_ptr->udp_interface),m_opts_ptr->udp_port,m_raw_rx_sock);

	struct ether_header* etherHeaderPtr;

	while(m_stopflg==false) {
		if(poll(socketMon,2,-1)>0) {
			if(socketMon[0].revents>0) {
				msglen=recvfrom(m_raw_rx_sock,msgbuf,sizeof(msgbuf),0,(struct sockaddr *)&addrll,&addrLen);

				etherHeaderPtr=reinterpret_cast<struct ether_header *>(msgbuf);

				// Discard any non-GN packet
				if (ntohs(etherHeaderPtr->ether_type)!=GN_ETHERTYPE) { 
					continue;
				}

				// A message from myself has been received -> this message should be discarded
				if(m_self_mac_set==false && compare_mac(etherHeaderPtr->ether_shost,m_self_mac)==true) {
					continue;
				}

				if(msglen<0) {
					fprintf(stderr,"[ERROR] Unable to receive a message from the specified socket.\n");
				} else {
					// Process the received message, after removing the "Ethernet" header with the EtherType, and the source and destination MAC addresses
					manageMessage(msgbuf+sizeof(struct ether_header),msglen-sizeof(struct ether_header));
				}
			} else if(socketMon[1].revents>0) {
				break;
			}
		}
	}

	fprintf(stdout,"[INFO] Message reception terminated on interface: %s:%ld\n",options_string_pop(m_opts_ptr->udp_interface),m_opts_ptr->udp_port);

	m_unlock_pd_rd=-1;
	m_unlock_pd_wr=-1;
	m_receptionInProgress=false;
}

void 
SocketClient::startReception(void) {
	int m_unlock_pd[2]={-1,-1};

	if(m_opts_ptr->enable_enhanced_CAMs==true) {
		m_decodeFrontend.enableEnhancedCAMs();
	}

	if(m_logfile_name!="") {
		if(m_logfile_name=="stdout") {
			m_logfile_file=stdout;
		} else {
			// Opening the output file in write + append mode just to be safe in case the user does not change the file name
			// between different executions of the S-LDM
			m_logfile_file=fopen(m_logfile_name.c_str(),"wa");
		}
	}

	if(m_receptionInProgress==true) {
		fprintf(stderr,"[WARNING] Attempted to start a V2X message reception while the reception is already in progress.\n"
			"Nothing will be done.\n");
	} else {
		if(pipe(m_unlock_pd)>=0) {
			m_unlock_pd_rd=m_unlock_pd[0];
			m_unlock_pd_wr=m_unlock_pd[1];
		}
		
		std::thread runningRxThr(&SocketClient::rxThr,this);
		if(m_opts_ptr->rssi_aux_update_interval_msec>0) {
			std::thread runningRouterOSRSSIThr(&SocketClient::routerOS_RSSI_retriever,this);
			runningRouterOSRSSIThr.join();
		}

		runningRxThr.join();
	}
}

void 
SocketClient::stopReception(void) {
	m_terminate_routeros_rssi_flag=true;

	if(m_unlock_pd_rd!=-1 && m_unlock_pd_wr!=-1) {
		if(write(m_unlock_pd_wr,"\0",1)<0) {
			fprintf(stderr,"[ERROR] Failure to successfully execute stopReception(). The program behaviour may be unexpected from now on.\n");
		}
	}

	if(m_logfile_name!="" && m_logfile_name!="stdout") {
		fclose(m_logfile_file);
	}
}

void 
SocketClient::manageMessage(uint8_t *message_bin_buf,size_t bufsize) {
	etsiDecoder::etsiDecodedData_t decodedData;

	uint64_t bf = 0.0,af = 0.0;
	uint64_t main_bf = 0.0,main_af = 0.0;

	if(m_logfile_name!="") {
		main_bf=get_timestamp_ns();

		// This additional log line has been commented out to avoid being too verbose
		// fprintf(m_logfile_file,"[NEW MESSAGE RX]\n");
	}

	if(m_printMsg == true) {
		fprintf(stdout,"Rx msg: ");
		for(size_t sti=0;sti<bufsize;sti++) {
			fprintf(stdout,"%02X",message_bin_buf[sti]);
		}
		fprintf(stdout,"\n");
	}

	if(m_logfile_name!="") {
		bf=get_timestamp_ns();
	}

	// Decode the content of the message, using the decoder-module frontend class
	// m_decodeFrontend.setPrintPacket(true); // <- uncomment to print the bytes of each received message. Should be used for debug only, and should be kept disabled when deploying the S-LDM.
	if(m_decodeFrontend.decodeEtsi(message_bin_buf, bufsize, decodedData, etsiDecoder::decoderFrontend::MSGTYPE_AUTO)!=ETSI_DECODER_OK) {
		std::cerr << "Error! Cannot decode ETSI packet!" << std::endl;
		return;
	}

	if(m_logfile_name!="") {
		af=get_timestamp_ns();

		fprintf(m_logfile_file,"[LOG - MESSAGE DECODER (Client %s)] ProcTimeMilliseconds=%.6lf\n",m_client_id.c_str(),(af-bf)/1000000.0);
	}

	// If a CAM has been received, it should be used to update the internal in-memory database
	if(decodedData.type == etsiDecoder::ETSI_DECODED_CAM || decodedData.type == etsiDecoder::ETSI_DECODED_CAM_NOGN) {
		CAM_t *decoded_cam = (CAM_t *) decodedData.decoded_msg;
		CAMEnhanced_t *decoded_encam = (CAMEnhanced_t *) decodedData.decoded_msg;

		double lat, lon;
		uint64_t stationID;

		if(m_opts_ptr->enable_enhanced_CAMs==true) {
			lat = decoded_encam->cam.camParameters.basicContainer.referencePosition.latitude/10000000.0;
			lon = decoded_encam->cam.camParameters.basicContainer.referencePosition.longitude/10000000.0;
			stationID = decoded_encam->header.stationID;
		} else {
			lat = decoded_cam->cam.camParameters.basicContainer.referencePosition.latitude/10000000.0;
			lon = decoded_cam->cam.camParameters.basicContainer.referencePosition.longitude/10000000.0;
			stationID = decoded_cam->header.stationID;
		}
		
		double l_inst_period=0.0;

		if(m_logfile_name!="") {
			bf=get_timestamp_ns();
		}

		// Update the database
		ldmmap::vehicleData_t vehdata;
		ldmmap::LDMMap::LDMMap_error_t db_retval;

		vehdata.exteriorLights = manage_LowfreqContainer(decodedData.decoded_msg,stationID);

		uint64_t gn_timestamp;
		if(decodedData.type == etsiDecoder::ETSI_DECODED_CAM) {
		    gn_timestamp = decodedData.gnTimestamp;
			// There is no need for an else if(), as we can enter here only if the decoded message type is either ETSI_DECODED_CAM or ETSI_DECODED_CAM_NOGN
		} else {
			gn_timestamp=UINT64_MAX;
            fprintf(stdout,"[WARNING] Current message contains no GN timestamp, ageCheck disabled.\n");
		}

		// Check the age of the data store inside the database (if the age check is enabled / -g option not specified)
		// before updating it with the new receive data
		if(m_opts_ptr->ageCheck_enabled == true && gn_timestamp != UINT64_MAX) {
			ldmmap::LDMMap::returnedVehicleData_t retveh;

			if(m_db_ptr->lookup(stationID,retveh)==ldmmap::LDMMap::LDMMAP_OK) {
				// According to the standard: GNTimestamp = (TAI timestamp since 2004-01-01 00:00:00) % 4294967296
				// Due to the modulo operation, it is not enough to consider the difference between the received GNTimestamp and the one
				// stored in the database, as this may cause issues when receiving data and the GNTimestamp values are cyclically reset
				// at the same time
				// We thus check the "gap" between the received numbers. Let's consider for instance: stored=4294967291, rx=3
				// In this case the "rx" data is the most up-to-date, but a cyclical reset occurred
				// We can then compute gap = 3 - 4294967291 = -429467288 < -300000 (-5 minutes) - ok! We keep this data even if 3 < 4294967291
				// Let's consider instead:
				// stored=3, rx=4294967291
				// In this case 'rx' is not the most up to date data (it is impossible to have '3' stored in the database and then receive
				// '4294967291', unless clock jumps occur in the car, bacause after all that time the data corresponding to '3' would have already
				// been garbage cleaned from the database)
				// We can then compute gap = 4294967291 - 3 = 429467288 > 300000 (5 minutes) - no! We should dicard the data we just received
				// Let's consider now a "normal" scenario:
				// stored=3, rx=114
				// gap = 114 - 3 = 111 < 300000 - ok! The data is kept (it would be discarded only if gap > 300000)
				// Finally, let's briefly analyze a final scenario:
				// stored=4294967292, rx=4294967291
				// It is evident how the rx data should be discarded because older than the stored one
				// gap = rx - stored = 4294967291 - 4294967292 = -1 > -300000 (-5 minutes) - The data is correctly discarded due to the second
				// condition in the if() clause
				long long int gap = static_cast<long long int>(gn_timestamp)-static_cast<long long int>(retveh.vehData.gnTimestamp);

				if((gn_timestamp>retveh.vehData.gnTimestamp && gap>300000) ||
					(gn_timestamp<retveh.vehData.gnTimestamp && gap>-300000)) {
					if(m_logfile_name!="") {
						fprintf(m_logfile_file,"[LOG - DATABASE UPDATE (Client %s)] Message discarded (data is too old). Rx = %lu, Stored = %lu, Gap = %lld\n",
							m_client_id.c_str(),
							gn_timestamp,retveh.vehData.gnTimestamp,gap);
						return;
					}
				}
			}
		}

		vehdata.lon = lon;
		vehdata.lat = lat;
		vehdata.timestamp_us = get_timestamp_us();

		if(m_opts_ptr->enable_enhanced_CAMs==true) {
			vehdata.elevation = decoded_encam->cam.camParameters.basicContainer.referencePosition.altitude.altitudeValue/100.0;
			vehdata.heading = decoded_encam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.heading.headingValue/10.0;
			vehdata.speed_ms = decoded_encam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.speed.speedValue/100.0;
			vehdata.camTimestamp = static_cast<long>(decoded_encam->cam.generationDeltaTime);
			vehdata.stationType = static_cast<ldmmap::e_StationTypeLDM>(decoded_encam->cam.camParameters.basicContainer.stationType);
		} else {
			vehdata.elevation = decoded_cam->cam.camParameters.basicContainer.referencePosition.altitude.altitudeValue/100.0;
			vehdata.heading = decoded_cam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.heading.headingValue/10.0;
			vehdata.speed_ms = decoded_cam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.speed.speedValue/100.0;
			vehdata.camTimestamp = static_cast<long>(decoded_cam->cam.generationDeltaTime);
			vehdata.stationType = static_cast<ldmmap::e_StationTypeLDM>(decoded_cam->cam.camParameters.basicContainer.stationType);
		}

		vehdata.gnTimestamp = gn_timestamp;
		vehdata.stationID = stationID; // It is very important to save also the stationID
		memcpy(vehdata.macaddr,decodedData.GNaddress,6); // Save the vehicle MAC address into the database

		if(m_opts_ptr->enable_enhanced_CAMs==true) {
			// Part specific to AIM: try to retrieve data also from a possible, proposed, channel load and node status container
			if(decoded_encam->cam.camParameters.channelNodeStatusContainer!=nullptr) {
				vehdata.cpu_load_perc = 
					decoded_encam->cam.camParameters.channelNodeStatusContainer->cpuLoad==CPULoad_unavailable ?
					CPULOAD_UNAVAILABLE :
					decoded_encam->cam.camParameters.channelNodeStatusContainer->cpuLoad/100.0;

				vehdata.ram_load_MB = 
					decoded_encam->cam.camParameters.channelNodeStatusContainer->ramLoad==RAMLoad_unavailable ?
					RAMLOAD_UNAVAILABLE :
					static_cast<double>(decoded_encam->cam.camParameters.channelNodeStatusContainer->ramLoad);

				if(decoded_encam->cam.camParameters.channelNodeStatusContainer->auxilliaryLinkMac!=nullptr) {
					// If the size is different than 6, this is not a MAC address, so it should not be processed
					if(decoded_encam->cam.camParameters.channelNodeStatusContainer->auxilliaryLinkMac->size!=6) {
						// NEED TO MANAGE THE OCTET STRING AND CONVERT THE HEX VALUES INTO AN HEX std::string
						// THE VALUES SHALL THEN BE STORED INTO vehdata.auxiliary_macaddr
						char c_str_macaddr[18]; // 12 characters + 5 ":" + "\0"

						std::snprintf(c_str_macaddr,18,"%02X:%02X:%02X:%02X:%02X:%02X",
							decoded_encam->cam.camParameters.channelNodeStatusContainer->auxilliaryLinkMac->buf[0],
							decoded_encam->cam.camParameters.channelNodeStatusContainer->auxilliaryLinkMac->buf[1],
							decoded_encam->cam.camParameters.channelNodeStatusContainer->auxilliaryLinkMac->buf[2],
							decoded_encam->cam.camParameters.channelNodeStatusContainer->auxilliaryLinkMac->buf[3],
							decoded_encam->cam.camParameters.channelNodeStatusContainer->auxilliaryLinkMac->buf[4],
							decoded_encam->cam.camParameters.channelNodeStatusContainer->auxilliaryLinkMac->buf[5]);

						vehdata.auxiliary_macaddr=std::string(c_str_macaddr);
					} else {
						vehdata.auxiliary_macaddr="unavailable";
					}
				} else {
					vehdata.auxiliary_macaddr="unavailable";
				}
			}
		} else {
			vehdata.auxiliary_macaddr="unavailable";
			vehdata.cpu_load_perc=CPULOAD_UNAVAILABLE;
			vehdata.ram_load_MB=RAMLOAD_UNAVAILABLE;
		}

		// Retrieve, if available, the information on the RSSI for the vehicle corresponding to the MAC address of the sender
		// This is the RSSI on the CAM dissemination interface
		struct iw_statistics wifistats;
		struct iwreq wifireq;
		strncpy(wifireq.ifr_name,options_string_pop(m_opts_ptr->udp_interface),IFNAMSIZ);
		wifireq.u.data.pointer=&wifistats;
		wifireq.u.data.length = sizeof(wifistats);

		if(ioctl(m_raw_rx_sock,SIOCGIWSTATS,&wifireq) == -1 || !(wifistats.qual.updated & IW_QUAL_DBM)) {
			vehdata.rssi_dBm=RSSI_UNAVAILABLE;
		} else {
			vehdata.rssi_dBm=wifistats.qual.level;
		}

		// Retrieve, if available, the information on the RSSI for a connected auxiliary device, via ssh
		// This information is retrieved only if specified by the user
		if(m_opts_ptr->rssi_aux_update_interval_msec>=0 && vehdata.auxiliary_macaddr!="unavailable") {
			m_routeros_rssi_mutex.lock();
			
			if(m_routeros_rssi.count(vehdata.auxiliary_macaddr)>0) {
				vehdata.rssi_auxiliary_dBm=m_routeros_rssi[vehdata.auxiliary_macaddr];
			} else {
				vehdata.rssi_auxiliary_dBm=RSSI_UNAVAILABLE;
			}

			m_routeros_rssi_mutex.unlock();
		}

		if(m_opts_ptr->enable_enhanced_CAMs==true) {
			if(decoded_encam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.vehicleWidth != VehicleWidth_unavailable) {
				vehdata.vehicleWidth = ldmmap::OptionalDataItem<long>(decoded_encam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.vehicleWidth*100);
			} else {
				vehdata.vehicleWidth = ldmmap::OptionalDataItem<long>(false);
			}

			if(decoded_encam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.vehicleLength.vehicleLengthValue != VehicleLengthValue_unavailable) {
				vehdata.vehicleLength = ldmmap::OptionalDataItem<long>(decoded_encam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.vehicleLength.vehicleLengthValue*100);
			} else {
				vehdata.vehicleLength = ldmmap::OptionalDataItem<long>(false);
			}
		} else {
			if(decoded_cam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.vehicleWidth != VehicleWidth_unavailable) {
				vehdata.vehicleWidth = ldmmap::OptionalDataItem<long>(decoded_cam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.vehicleWidth*100);
			} else {
				vehdata.vehicleWidth = ldmmap::OptionalDataItem<long>(false);
			}

			if(decoded_cam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.vehicleLength.vehicleLengthValue != VehicleLengthValue_unavailable) {
				vehdata.vehicleLength = ldmmap::OptionalDataItem<long>(decoded_cam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.vehicleLength.vehicleLengthValue*100);
			} else {
				vehdata.vehicleLength = ldmmap::OptionalDataItem<long>(false);
			}
		}

		// If logging is enabled, compute also an "instantaneous update period" metric (i.e., how much time has passed between two consecutive vehicle updates)
		if(m_logfile_name!="") {
			ldmmap::LDMMap::returnedVehicleData_t retveh;

			if(m_db_ptr->lookup(stationID,retveh)==ldmmap::LDMMap::LDMMAP_OK) {
				l_inst_period=(get_timestamp_us()-retveh.vehData.timestamp_us)/1000.0;
			} else {
				l_inst_period=-1.0;
			}
		}

		// std::cout << "[DEBUG] Updating vehicle with stationID: " << vehdata.stationID << std::endl;

		db_retval=m_db_ptr->insert(vehdata);

		if(db_retval!=ldmmap::LDMMap::LDMMAP_OK && db_retval!=ldmmap::LDMMap::LDMMAP_UPDATED) {
			std::cerr << "Warning! Insert on the database for vehicle " << (int) stationID << "failed!" << std::endl;
		}

		if(m_logfile_name!="") {
			af=get_timestamp_ns();

			fprintf(m_logfile_file,"[LOG - DATABASE UPDATE (Client %s)] LowFrequencyContainerAvail=%d InsertReturnValue=%d ProcTimeMilliseconds=%.6lf\n",
				m_client_id.c_str(),
				m_opts_ptr->enable_enhanced_CAMs==true ? 
					decoded_encam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.vehicleWidth != VehicleWidth_unavailable :
					decoded_cam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.vehicleWidth != VehicleWidth_unavailable,
				db_retval,
				(af-bf)/1000000.0);
		}

		if(m_opts_ptr->enable_enhanced_CAMs==true) {
			ASN_STRUCT_FREE(asn_DEF_CAM,decoded_cam);
		} else {
			ASN_STRUCT_FREE(asn_DEF_CAMEnhanced,decoded_encam);
		}

		std::cout << "[TBR] StationID = " << stationID << std::endl;

		if(m_logfile_name!="") {
			main_af=get_timestamp_ns();

			logfprintf(m_logfile_file,std::string("FULL CAM PROCESSING (Client ") + m_client_id + std::string(")"),"StationID=%u Coordinates=%.7lf:%.7lf Heading=%.1lf InstUpdatePeriod=%.3lf"
				" CAMTimestamp=%ld GNTimestamp=%lu CAMTimestampDiff=%ld GNTimestampDiff=%ld"
				" ProcTimeMilliseconds=%.6lf EnhancedCAMsEnabled=%d CPULoadPerc=%.2lf FreeRAMMB=%.2lf MAC_Addr=%02X:%02X:%02X:%02X:%02X:%02X"
				" Aux_MAC_Addr=%s\n",
				stationID,lat,lon,
				vehdata.heading,
				l_inst_period,
				vehdata.camTimestamp,vehdata.gnTimestamp,get_timestamp_ms_cam()-vehdata.camTimestamp,get_timestamp_ms_gn()-vehdata.gnTimestamp,
				(main_af-main_bf)/1000000.0,
				m_opts_ptr->enable_enhanced_CAMs,
				vehdata.cpu_load_perc,
				vehdata.ram_load_MB,
				vehdata.macaddr[0],vehdata.macaddr[1],vehdata.macaddr[2],vehdata.macaddr[3],vehdata.macaddr[4],vehdata.macaddr[5],
				vehdata.auxiliary_macaddr.c_str());
			
			// fprintf(m_logfile_file,"[LOG - FULL CAM PROCESSING] StationID=%u Coordinates=%.7lf:%.7lf InstUpdatePeriod=%.3lf"
			// 	" CAMTimestamp=%ld GNTimestamp=%lu CAMTimestampDiff=%ld GNTimestampDiff=%ld"
			// 	" ProcTimeMilliseconds=%.6lf\n",
			// 	stationID,lat,lon,
			// 	l_inst_period,
			// 	vehdata.camTimestamp,vehdata.gnTimestamp,get_timestamp_ms_cam()-vehdata.camTimestamp,get_timestamp_ms_gn()-vehdata.gnTimestamp,
			// 	(main_af-main_bf)/1000000.0);	
		}

	} else {
		std::cerr << "Warning! Only CAM messages are supported for the time being!" << std::endl;
		return;
	}
}
