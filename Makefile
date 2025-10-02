#
# Makefile to build TeamSpeak 3 Client Test Plugin
#

CFLAGS = -c -O2 -Wall -fPIC
INCLUDES = -Iinclude

SRC = src/plugin.c src/ini_wrapper.c src/ini.c
OBJS = plugin.o ini_wrapper.o ini.o

all: clean lh2mqtt install

lh2mqtt: $(OBJS)
	gcc -shared -o lh2mqtt.so $(OBJS)

plugin.o: src/plugin.c src/ini_wrapper.h
	gcc $(INCLUDES) $(CFLAGS) src/plugin.c -o plugin.o

ini_wrapper.o: src/ini_wrapper.c src/ini_wrapper.h src/ini.h
	gcc $(INCLUDES) $(CFLAGS) src/ini_wrapper.c -o ini_wrapper.o

ini.o: src/ini.c src/ini.h
	gcc $(INCLUDES) $(CFLAGS) src/ini.c -o ini.o

install: lh2mqtt
	@mkdir -p $(HOME)/.ts3client/plugins
	cp lh2mqtt.so $(HOME)/.ts3client/plugins/
	@echo "Plugin installiert nach $(HOME)/.ts3client/plugins/"

clean:
	rm -f *.o lh2mqtt.so
