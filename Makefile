#
# Makefile to build TeamSpeak 3 Client Test Plugin
#

CFLAGS = -c -O2 -Wall -fPIC
INCLUDES = -Iinclude

SRC = src/plugin.c src/ini_wrapper.c src/ini.c
OBJS = plugin.o ini_wrapper.o ini.o

PLUGINDIR = $(HOME)/.ts3client/plugins

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
	@mkdir -p $(PLUGINDIR)
	cp lh2mqtt.so $(PLUGINDIR)/
	@echo "Plugin installiert nach $(PLUGINDIR)/"
	@mkdir -p $(PLUGINDIR)/lh2mqtt
	cp src/icons/lh2mqtt/*.png $(PLUGINDIR)/lh2mqtt/
	@echo "Icons installiert nach $(PLUGINDIR)/lh2mqtt/"

clean:
	rm -f *.o lh2mqtt.so
