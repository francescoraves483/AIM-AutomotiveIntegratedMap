#ifndef GPSC_H
#define GPSC_H

#include <gps.h>
#include <string>
#include <stdint.h>

class GPSClient {
	public:
		GPSClient(std::string server, long port) :
			m_server(server), m_port(port) {};

		GPSClient() {};

		// The connection to the GNSS device is terminated when the object is destroyed
		~GPSClient() {};

		// The method will set up the connection to the GNSS device via gps_open() and gps_stream(), using the server and port stored as private attributes
		void openConnection();

		// This method shall be called after we no longer need to receive new data, and will take care of closing the connection to gpsd
		void closeConnection();

		// Standard types
		double getHeadingValue();
		double getSpeedValue();
		std::pair<double,double> getCurrentPosition();
	private:
		std::string m_server="localhost";
		long m_port=3000;
		struct gps_data_t m_gps_data;
};

#endif // GPSC_H