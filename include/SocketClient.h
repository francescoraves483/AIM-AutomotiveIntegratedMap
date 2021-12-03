#ifndef AIM_SOCKET_CLIENT_H
#define AIM_SOCKET_CLIENT_H

#include <memory>
#include <atomic>
#include <thread>
#include <cstring>
#include <map>
#include <mutex>

#include "etsiDecoderFrontend.h"
#include "LDMmap.h"
#include "utils.h"

extern "C" {
	#include "options.h"
	#include "CAM.h"
}

// Maxium possible size of messages which could be received from the socket
#define MSGBUF_MAXSIZ 2048

// GeoNetworking Ethertype value
#define GN_ETHERTYPE 0x8947

class SocketClient {
	private:
		int m_raw_rx_sock=-1;
		etsiDecoder::decoderFrontend m_decodeFrontend;
		struct options *m_opts_ptr;
		ldmmap::LDMMap *m_db_ptr;

		std::string m_logfile_name;
		FILE *m_logfile_file;

		std::string m_client_id;

		bool m_printMsg; // If 'true' each received message will be printed (default: 'false' - enable only for debugging purposes)

		std::atomic<bool> m_stopflg;

		std::atomic<bool> m_receptionInProgress;

		inline ldmmap::OptionalDataItem<uint8_t> manage_LowfreqContainer(CAM_t *decoded_cam,uint32_t stationID);
		void manageMessage(uint8_t *buf,size_t bufsize);

		// Reception thread method
		void rxThr(void);

		std::atomic<int> m_unlock_pd_rd;
		std::atomic<int> m_unlock_pd_wr;

		std::map<std::string,double> m_routeros_rssi; // Auxiliary RouterOS-based device RSSI map (<MAC address>,<RSSI value>)
		std::mutex m_routeros_rssi_mutex;

		uint8_t m_self_mac[6]={0}; // Self MAC address; if specified, all the received messages coming from this MAC address will be discarded
		bool m_self_mac_set;

		std::atomic<bool> m_terminate_routeros_rssi_flag;
	public:
		SocketClient(const int &raw_rx_sock,struct options *opts_ptr, ldmmap::LDMMap *db_ptr, std::string logfile_name):
			m_raw_rx_sock(raw_rx_sock), m_opts_ptr(opts_ptr), m_db_ptr(db_ptr), m_logfile_name(logfile_name) {
				m_client_id="unset";
				m_logfile_file=nullptr;
				m_printMsg=false;
				m_stopflg=false;
				m_receptionInProgress=false;
				m_unlock_pd_rd=-1;
				m_unlock_pd_wr=-1;
				m_self_mac_set=false;
				memset(m_self_mac,0,6);
				m_routeros_rssi={};
				m_terminate_routeros_rssi_flag=false;
			}

			void setPrintMsg(bool printMsgEnable) {m_printMsg = printMsgEnable;}

			void setClientID(std::string id) {m_client_id=id;}

			void setSelfMAC(uint8_t self_mac[6]) {memcpy(m_self_mac,self_mac,6); m_self_mac_set=true;}

			void startReception(void);
			void stopReception(void);

			// Special function to retrieve RSSI values from a connected RouterOS-based device
			void routerOS_RSSI_retriever();
};

#endif // AIM_SOCKET_CLIENT_H
