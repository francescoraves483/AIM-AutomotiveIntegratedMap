#ifndef VEHDATADEF_H
#define VEHDATADEF_H

#include <unordered_map>
#include <vector>
#include <shared_mutex>

#define vehicleDataVector_t(name) std::vector<ldmmap::vehicleData_t> name;

// Facility macro to convert from DEG to RAD
#define DEG_2_RAD(val) ((val)*M_PI/180.0)

// Unavailable value for the RSSI
#define RSSI_UNAVAILABLE -80000
#define CPULOAD_UNAVAILABLE 10001
#define GPULOAD_UNAVAILABLE 10001
#define RAMLOAD_UNAVAILABLE 512001

namespace ldmmap {
	// Class to store optional data
	// If the data is not available, m_available is 'false' and no actual data is stored (getData() does not return any meaningful data)
	// If the data is available (isAvailable() returns 'true'), then the actual data can be retrieved with getData()
	template <class T> class OptionalDataItem
	{
		private:
			bool m_available;
			T m_dataitem;

		public:
			OptionalDataItem(T data): m_dataitem(data) {m_available=true;}
			OptionalDataItem(bool availability) {m_available=availability;}
			OptionalDataItem() {m_available=false;}
			T getData() {return m_dataitem;}
			bool isAvailable() {return m_available;}
			T setData(T data) {m_dataitem=data; m_available=true;}
	};

	typedef enum StationTypeLDM {
		StationType_LDM_unknown = 0,
		StationType_LDM_pedestrian = 1,
		StationType_LDM_cyclist	= 2,
		StationType_LDM_moped = 3,
		StationType_LDM_motorcycle = 4,
		StationType_LDM_passengerCar = 5,
		StationType_LDM_bus	= 6,
		StationType_LDM_lightTruck = 7,
		StationType_LDM_heavyTruck = 8,
		StationType_LDM_trailer = 9,
		StationType_LDM_specialVehicles	= 10,
		StationType_LDM_tram = 11,
		StationType_LDM_roadSideUnit = 15,
		StationType_LDM_unspecified= 99
	} e_StationTypeLDM;

	// This structure contains all the data stored in the database for each vehicle (except for the PHPoints)
	typedef struct vehicleData {
		uint64_t stationID;
		double lat;
		double lon;
		double elevation;
		double heading; // Heading between 0 and 360 degrees
		double speed_ms;
		uint64_t gnTimestamp;
		long camTimestamp; // This is the CAM message GenerationDeltaTime
		uint64_t timestamp_us;
		OptionalDataItem<long> vehicleWidth;
		OptionalDataItem<long> vehicleLength;
		e_StationTypeLDM stationType;
		uint8_t macaddr[6];
		std::string auxiliary_macaddr;
		double rssi_dBm;
		double rssi_auxiliary_dBm;
		double cpu_load_perc;
		double ram_load_MB;
		double gpu_load_perc;
		double extradev_cpu_load_perc;
		double extradev_ram_load_MB;
		double extradev_gpu_load_perc;
		std::string ipaddr;
		std::string publicipaddr;

		// Low frequency container data
		OptionalDataItem<uint8_t> exteriorLights; // Bit string with exterior lights status
	} vehicleData_t;
}

#endif // VEHDATADEF_H