#include "simple_gpsc.h"
#include <stdexcept>
#include <iostream>
#include <math.h> 
#include <cfloat>

#define DECI                10
#define CENTI               100
#define MILLI               1000
#define MICRO               1000000
#define DOT_ONE_MICRO       10000000
#define NANO_TO_MILLI       1000000
#define NANO_TO_CENTI       10000000

#define GPSSTATUS(gpsdata) gpsdata.fix.status

void
GPSClient::openConnection() {
	int rval;

	if((rval=gps_open(m_server.c_str(),std::to_string(m_port).c_str(),&m_gps_data))!=0) {
		throw std::runtime_error("GNSS device open failed: " + std::string(gps_errstr(rval)));
	}

	(void)gps_stream(&m_gps_data, WATCH_ENABLE | WATCH_JSON, NULL);
	(void)gps_waiting(&m_gps_data, 5000000);
}

void
GPSClient::closeConnection() {
	(void)gps_stream(&m_gps_data, WATCH_DISABLE, NULL);
	(void)gps_close(&m_gps_data);
}


double
GPSClient::getHeadingValue() {
	int rval;
	rval=gps_read(&m_gps_data,nullptr,0);

	if(rval==-1) {
		throw std::runtime_error("Cannot read the heading from GNSS device: " + std::string(gps_errstr(rval)));
	} else {
		// Check if the mode is set and if a fix has been obtained
		if((m_gps_data.set & MODE_SET)==MODE_SET) { //&& GPSSTATUS(m_gps_data)!=STATUS_NO_FIX) {
			if(m_gps_data.fix.mode == MODE_2D || m_gps_data.fix.mode == MODE_3D) {
				if(static_cast<int>(m_gps_data.fix.track*DECI)<0 || static_cast<int>(m_gps_data.fix.track*DECI)>3601) {
					return -DBL_MAX;
				} else {
					return m_gps_data.fix.track;
				}
			}
		}
	}

	return -DBL_MAX;
}

double
GPSClient::getSpeedValue() {
	int rval;
	rval=gps_read(&m_gps_data,nullptr,0);

	if(rval==-1) {
		throw std::runtime_error("Cannot read the speed from GNSS device: " + std::string(gps_errstr(rval)));
	} else {
		// Check if the mode is set and if a fix has been obtained
		if((m_gps_data.set & MODE_SET)==MODE_SET) { // && GPSSTATUS(m_gps_data)!=STATUS_NO_FIX) {
			if(m_gps_data.fix.mode == MODE_2D || m_gps_data.fix.mode == MODE_3D) {
				return m_gps_data.fix.speed;
			}
		}
	}

	return -DBL_MAX;
}

std::pair<double,double>
GPSClient::getCurrentPosition() {
	int rval;
	rval=gps_read(&m_gps_data,nullptr,0);

	if(rval==-1) {
		throw std::runtime_error("Cannot read the position from GNSS device: " + std::string(gps_errstr(rval)));
	} else {
		// Check if the mode is set and if a fix has been obtained
		if((m_gps_data.set & MODE_SET)==MODE_SET) { // && GPSSTATUS(m_gps_data)!=STATUS_NO_FIX) {
			if(m_gps_data.fix.mode == MODE_2D || m_gps_data.fix.mode == MODE_3D) {
				if(!isnan(m_gps_data.fix.latitude) && !isnan(m_gps_data.fix.longitude)) {
					return std::pair<double,double>(m_gps_data.fix.latitude,m_gps_data.fix.longitude);
				}
			}
		}
	}

	return std::pair<double,double>(-DBL_MAX,-DBL_MAX);
}