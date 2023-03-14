EXECNAME=AIM

OPENWRT_CROSSCOMPILER_C=x86_64-openwrt-linux-musl-g++
OPENWRT_CROSSCOMPILER_CXX=x86_64-openwrt-linux-musl-gcc
OPENWRT_CROSSCOMPILER_LD=x86_64-openwrt-linux-musl-ld
OPENWRT_TARGET=target-x86_64_musl
OPENWRT_TOOLCHAIN=toolchain-x86_64_gcc-8.4.0_musl
OPENWRT_LIBGPS_VER=3.23

OPENWRT_INCLUDE_DIR=/mnt/xtra/OpenWrt-V2X/staging_dir/$(OPENWRT_TARGET)/usr/include

SRC_DIR=src
OBJ_DIR=obj

SRC_VEHVIS_DIR=vehicle-visualizer/src
OBJ_VEHVIS_DIR=obj/vehicle-visualizer

SRC_OPTIONS_DIR=options
OBJ_OPTIONS_DIR=obj/options

SRC_DECODER_DIR=decoder-module/src
OBJ_DECODER_DIR=obj/decoder-module

SRC_ASN1_DIR=decoder-module/asn1/src
OBJ_ASN1_DIR=obj/asn1

SRC_JSON11_DIR=json11
OBJ_JSON11_DIR=obj/json11

SRC_GPSC_DIR=gnss
OBJ_GPSC_DIR=obj/gnss

SRC=$(wildcard $(SRC_DIR)/*.cpp)
SRC_VEHVIS=$(wildcard $(SRC_VEHVIS_DIR)/*.cc)
SRC_OPTIONS=$(wildcard $(SRC_OPTIONS_DIR)/*.c)
SRC_DECODER=$(wildcard $(SRC_DECODER_DIR)/*.cpp)
SRC_ASN1=$(wildcard $(SRC_ASN1_DIR)/*.c)
SRC_JSON11=$(wildcard $(SRC_JSON11_DIR)/*.cpp)
SRC_GPSC=$(wildcard $(SRC_GPSC_DIR)/*.cpp)

OBJ=$(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
OBJ_VEHVIS=$(SRC_VEHVIS:$(SRC_VEHVIS_DIR)/%.cc=$(OBJ_VEHVIS_DIR)/%.o)
OBJ_OPTIONS=$(SRC_OPTIONS:$(SRC_OPTIONS_DIR)/%.c=$(OBJ_OPTIONS_DIR)/%.o)
OBJ_DECODER=$(SRC_DECODER:$(SRC_DECODER_DIR)/%.cpp=$(OBJ_DECODER_DIR)/%.o)
OBJ_ASN1=$(SRC_ASN1:$(SRC_ASN1_DIR)/%.c=$(OBJ_ASN1_DIR)/%.o)
OBJ_JSON11=$(SRC_JSON11:$(SRC_JSON11_DIR)/%.cpp=$(OBJ_JSON11_DIR)/%.o)
OBJ_GPSC=$(SRC_GPSC:$(SRC_GPSC_DIR)/%.cpp=$(OBJ_GPSC_DIR)/%.o)

OBJ_CC=$(OBJ)
OBJ_CC+=$(OBJ_VEHVIS)
OBJ_CC+=$(OBJ_OPTIONS)
OBJ_CC+=$(OBJ_DECODER)
OBJ_CC+=$(OBJ_ASN1)
OBJ_CC+=$(OBJ_JSON11)

OBJ_CC_W_GPSC=$(OBJ_CC)
OBJ_CC_W_GPSC+=$(OBJ_GPSC)
EXECNAME_W_GPSD:=$(EXECNAME)_gpsd

CXXFLAGS += -Wall -O3 -Iinclude -Ivehicle-visualizer/include -Ioptions -std=c++17 -Idecoder-module/include -Idecoder-module/asn1/include -Ijson11
CFLAGS += -Wall -O3 -Iinclude -Ioptions -Idecoder-module/asn1/include
LDLIBS += -lpthread -lm

.PHONY: all clean

all: compilePC

compilePC: CXX = g++
compilePC: CC = gcc

compileAPU: CXX = $(OPENWRT_CROSSCOMPILER_C)
compileAPU: CC = $(OPENWRT_CROSSCOMPILER_CXX)
compileAPU: LD = $(OPENWRT_CROSSCOMPILER_LD)
compileAPU: CXXFLAGS += -I$(OPENWRT_INCLUDE_DIR)

compilePC_gpsd: CXX = g++
compilePC_gpsd: CC = gcc
compilePC_gpsd: CXXFLAGS += -Ignss -DGPSD_ENABLED
compilePC_gpsd: CFLAGS += -DGPSD_ENABLED
compilePC_gpsd: LDLIBS += -lgps

compileAPU_gpsd: CXX = $(OPENWRT_CROSSCOMPILER_C)
compileAPU_gpsd: CC = $(OPENWRT_CROSSCOMPILER_CXX)
compileAPU_gpsd: LD = $(OPENWRT_CROSSCOMPILER_LD)
compileAPU_gpsd: CXXFLAGS += -I$(OPENWRT_INCLUDE_DIR) -Ignss -DGPSD_ENABLED
compileAPU_gpsd: CFLAGS += -DGPSD_ENABLED
compileAPU_gpsd: LDLIBS += -lgps

compilePCdebug: CXXFLAGS += -g
compilePCdebug: compilePC

compileAPUdebug: CXXFLAGS += -g
compileAPUdebug: compileAPU

compilePC compilePCdebug compileAPU compileAPUdebug: $(EXECNAME)
compilePC_gpsd: $(EXECNAME_W_GPSD)
compileAPU_gpsd: APUcopyfiles $(EXECNAME_W_GPSD)

APUcopyfiles:
	# This is just a "trick" to be able to compile OCABS for OpenWrt, properly linking with libgps, without writing a specific package Makefile
	@ cp $(OPENWRT_INCLUDE_DIR)/../../../../build_dir/$(OPENWRT_TARGET)/gpsd-$(OPENWRT_LIBGPS_VER)/ipkg-install/usr/lib/*.so* $(OPENWRT_INCLUDE_DIR)/../../../$(OPENWRT_TOOLCHAIN)/lib

# Standard targets
$(EXECNAME): $(OBJ_CC)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) $(CXXFLAGS) $(CFLAGS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@ mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_VEHVIS_DIR)/%.o: $(SRC_VEHVIS_DIR)/%.cc
	@ mkdir -p $(OBJ_VEHVIS_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_OPTIONS_DIR)/%.o: $(SRC_OPTIONS_DIR)/%.c
	@ mkdir -p $(OBJ_OPTIONS_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DECODER_DIR)/%.o: $(SRC_DECODER_DIR)/%.cpp
	@ mkdir -p $(OBJ_DECODER_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_ASN1_DIR)/%.o: $(SRC_ASN1_DIR)/%.c
	@ mkdir -p $(OBJ_ASN1_DIR)
	$(CC) $(CFLAGS) -c $< -o $@
	
$(OBJ_JSON11_DIR)/%.o: $(SRC_JSON11_DIR)/%.cpp
	@ mkdir -p $(OBJ_JSON11_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# gpsd support targets
$(EXECNAME_W_GPSD): $(OBJ_CC_W_GPSC)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) $(CXXFLAGS) $(CFLAGS) -o $@
	-mv $(EXECNAME_W_GPSD) $(EXECNAME)

$(OBJ_GPSC_DIR)/%.o: $(SRC_GPSC_DIR)/%.cpp
	@ mkdir -p $(OBJ_GPSC_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@


clean:
	$(RM) $(OBJ_DIR)/*.o $(OBJ_VEHVIS_DIR)/*.o $(OBJ_OPTIONS_DIR)/*.o$(OBJ_DECODER_DIR)/*.o $(OBJ_ASN1_DIR)/*.o $(OBJ_JSON11_DIR)/*.o $(OBJ_GPSC_DIR)/*.o
	-rm -rf $(OBJ_DIR)
	-rm -rf $(OBJ_VEHVIS_DIR)
	-rm -rf $(OBJ_OPTIONS_DIR)
	-rm -rf $(OBJ_DECODER_DIR)
	-rm -rf $(OBJ_ASN1_DIR)
	-rm -rf $(OBJ_JSON11)
	-rm -rf $(OBJ_GPSC)
	
fullclean: clean
	$(RM) $(EXECNAME)
