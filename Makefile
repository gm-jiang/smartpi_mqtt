#makefile for smartpi

LDFLAGS += -lpthread
CFLAGS += -I. -I./main -I./mqttclient -I./mqttclient/MQTTLinux -I./mqttclient/MQTTPacket -I./log -I./debugtool

CFLAGS += -O0 -g

BIN=smartpi
CC=gcc
#CC=arm-linux-gnueabihf-gcc

SUBDIR=$(shell ls -d */)
ROOTSRC=$(wildcard *.c)
ROOTOBJ=$(ROOTSRC: %.c=%.o)
SUBSRC=$(shell find $(SUBDIR) -name '*.c')
SUBOBJ=$(SUBSRC:%.c=%.o)

$(BIN): $(ROOTOBJ) $(SUBOBJ)
	$(CC) $(CFLAGS) -Wl,-Map,smartpi.map $(LIB_DIR) $(INCLUDE_DIR) -o $(BIN) $(ROOTOBJ) $(SUBOBJ) $(LDFLAGS)
.c.o:
	$(CC) $(CFLAGS) $(LIB_DIR) $(INCLUDE_DIR) -c $< -o $@

clean:
	rm -rf *.map $(BIN) $(ROOTOBJ) $(SUBOBJ)
