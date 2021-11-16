#include "SocketClient.h"
#include "utils.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <poll.h>
#include <unistd.h>

// If this is a full ITS message manage the low frequency container data
// Check if this CAM contains the low frequency container
// and if the ext. lights hack for older versions CAMs is disable
// If yes, store the exterior lights status
// If not, check if an older information about the exterior lights of the current vehicle already exist in the database (using m_db_ptr->lookup()),
// if this data exists, use this data, if not, just set the exterior lights information as unavailable
inline ldmmap::OptionalDataItem<uint8_t>
SocketClient::manage_LowfreqContainer(CAM_t *decoded_cam,uint32_t stationID) {

          if(decoded_cam->cam.camParameters.lowFrequencyContainer!=NULL) {
                  // In any normal, uncorrupted CAM, buf should never be NULL and it should contain at least one element (i.e. buf[0] always exists)
                  if(decoded_cam->cam.camParameters.lowFrequencyContainer->choice.basicVehicleContainerLowFrequency.exteriorLights.buf!=NULL) {
                          return ldmmap::OptionalDataItem<uint8_t>(decoded_cam->cam.camParameters.lowFrequencyContainer->choice.basicVehicleContainerLowFrequency.exteriorLights.buf[0]);
                  } else {
                          // Data from a corrupted decoded CAM is considered as unavailable, for the time being
                          return ldmmap::OptionalDataItem<uint8_t>(false);
                  }
          } else {
                  ldmmap::LDMMap::returnedVehicleData_t retveh;

                  if(m_db_ptr->lookup(stationID,retveh)==ldmmap::LDMMap::LDMMAP_OK) {
                          return retveh.vehData.exteriorLights;
                  } else {
                          return ldmmap::OptionalDataItem<uint8_t>(false);
                  }
          }
}

void
SocketClient::rxThr(void) {
	size_t msglen=-1;
	uint8_t msgbuf[MSGBUF_MAXSIZ];

	struct sockaddr_in rxSockAddr;
	socklen_t rxSockAddrLen=sizeof(rxSockAddr);

	m_receptionInProgress=true;

	struct pollfd socketMon[2];

	// Monitor the socket
	socketMon[0].fd=m_udp_rx_sock;
	socketMon[0].revents=0;
	socketMon[0].events=POLLIN;

	// Monitor the "unlock pipe"
	socketMon[1].fd=m_unlock_pd_rd;
	socketMon[1].revents=0;
	socketMon[1].events=POLLIN;

	fprintf(stdout,"[INFO] Message reception started on interface: %s:%ld. Socket: %d.\n",options_string_pop(m_opts_ptr->udp_interface),m_opts_ptr->udp_port,m_udp_rx_sock);

	while(m_stopflg==false) {
		if(poll(socketMon,2,-1)>0) {
			fprintf(stdout,"HHAHAHHAHAHAHAHHAHA\n");
			if(socketMon[0].revents>0) {
				msglen=recvfrom(m_udp_rx_sock,msgbuf,sizeof(msgbuf),0,(struct sockaddr *)&rxSockAddr,&rxSockAddrLen);

				if(msglen<0) {
					fprintf(stderr,"[ERROR] Unable to receive a message from the specified socket.\n");
				} else {
					manageMessage(msgbuf,msglen);
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
		runningRxThr.join();
	}
}

void 
SocketClient::stopReception(void) {
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
		double lat = decoded_cam->cam.camParameters.basicContainer.referencePosition.latitude/10000000.0;
		double lon = decoded_cam->cam.camParameters.basicContainer.referencePosition.longitude/10000000.0;
		uint32_t stationID = decoded_cam->header.stationID;
		double l_inst_period=0.0;

		if(m_logfile_name!="") {
			bf=get_timestamp_ns();
		}

		// Update the database
		ldmmap::vehicleData_t vehdata;
		ldmmap::LDMMap::LDMMap_error_t db_retval;

		vehdata.exteriorLights = manage_LowfreqContainer(decoded_cam,stationID);

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
		vehdata.elevation = decoded_cam->cam.camParameters.basicContainer.referencePosition.altitude.altitudeValue/100.0;
		vehdata.heading = decoded_cam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.heading.headingValue/10.0;
		vehdata.speed_ms = decoded_cam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.speed.speedValue/100.0;
		vehdata.gnTimestamp = gn_timestamp;
		vehdata.stationID = stationID; // It is very important to save also the stationID
		vehdata.camTimestamp = static_cast<long>(decoded_cam->cam.generationDeltaTime);
		vehdata.stationType = static_cast<ldmmap::e_StationTypeLDM>(decoded_cam->cam.camParameters.basicContainer.stationType);

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
				decoded_cam->cam.camParameters.highFrequencyContainer.choice.basicVehicleContainerHighFrequency.vehicleWidth != VehicleWidth_unavailable,
				db_retval,
				(af-bf)/1000000.0);
		}

		ASN_STRUCT_FREE(asn_DEF_CAM,decoded_cam);

		if(m_logfile_name!="") {
			main_af=get_timestamp_ns();

			logfprintf(m_logfile_file,std::string("FULL CAM PROCESSING (Client") + m_client_id + std::string(")"),"StationID=%u Coordinates=%.7lf:%.7lf Heading=%.1lf InstUpdatePeriod=%.3lf"
				" CAMTimestamp=%ld GNTimestamp=%lu CAMTimestampDiff=%ld GNTimestampDiff=%ld"
				" ProcTimeMilliseconds=%.6lf\n",
				stationID,lat,lon,
				vehdata.heading,
				l_inst_period,
				vehdata.camTimestamp,vehdata.gnTimestamp,get_timestamp_ms_cam()-vehdata.camTimestamp,get_timestamp_ms_gn()-vehdata.gnTimestamp,
				(main_af-main_bf)/1000000.0);
			
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