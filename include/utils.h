#ifndef SLDM_UTILS_H
#define SLDM_UTILS_H

#include <cinttypes>
#include <ctime>
#include <cmath>
#include <cstdio>
#include <poll.h>
#include <sys/timerfd.h>
#include <string>

#define POLL_DEFINE_JUNK_VARIABLE() long int junk
#define POLL_CLEAR_EVENT(clockFd) junk=read(clockFd,&junk,sizeof(junk))

uint64_t get_timestamp_us(void);
uint64_t get_timestamp_ns(void);
uint64_t get_timestamp_ms_gn(void);
uint64_t get_timestamp_ms_cam(void);
int timer_fd_create(struct pollfd &pollfd,int &clockFd,uint64_t time_us);
std::string exteriorLights_bit_to_string(uint8_t extLights);
bool doublecomp(double d1, double d2, double eps = 0.0001);
// logfprintf is an alternative to fprintf for logging purposes
// It works just like fprintf, by printing:
// "[LOG - <modulename>] (<current date and time>) <fprintf content>"
// As it is retrieving the time for each call to logfprintf() it is expected 
// be slightly slower than fprintf and should thus be used only when really needed
int logfprintf(FILE *stream,std::string modulename,const char *format,...);
double get_rssi_from_iw(uint8_t macaddr[6],std::string interface_name);

#endif // SLDM_UTILS_H