# lh2mqtt - LastHeard To Mqtt
This TeamSpeak 3 plugin sends the name of the currently speaking user (LastHeard) to your MQTT broker and/or the channel tab.

It is a fork of the <i>official TeamSpeak &copy; helper repository for creating native plugins</i>, see https://github.com/teamspeak/ts3client-pluginsdk

## Pre-requisites
In order to use lh2mqtt plugin, you'll need the following:
- TS3 windows or linux client, see https://teamspeak.com
- Mosquitto commandline tools, see https://mosquitto.org/download/
- if you're using SSL to connect to your MQTT broker, you'll need the certfile (crt) of your server's certificate authority (CA), see <i>mosquitto_pub.exe</i>'s option <i>CAFILE</i>. You can e.g. download it in your browser (base64). It needs to be your main domain. If using alternative domains, it may fail (see <i>altnames</i> or <i>subjectAltName</i>, e.g. https://stackoverflow.com/questions/19787320/ssl-certificate-fails-for-ca-certificate-in-mosquitto-1-2-1-1-2-2).

## Configuration
For configuration see your TS3 plugin directory:<br/> 
- Linux: <code>$HOME/.ts3client/plugins/lh2mqtt.ini</code>
- Windows: <code>%APPDATA%\TS3Client\plugins\lh2mqtt.ini</code>

If no lh2mqtt.ini exists when starting TeamSpeak &copy;, a new file with template values will be generated.

You can open the config file via TS3 main menu 'Plugins/lh2mqtt'.

## Distribution: ts3_plugin file (windows)
Visual Studio community 2019 is used for compiling.

In order to build for different TS3 API versions (e.g. 23/24/26), change the minor version number in the following three places:
- plugin.c 
  - #define PLUGIN_API_VERSION ...
  - function ts3plugin_version
- project preferences / build events / post build: change commandline arguments


There are two batches that should be called through postbuild events. Correct build order to create ts3_plugin file is:
1) Release/x64 (only copies x64-dll)
2) Release/x32 (copies x32-dll and creates ts3_plugin file)

ts3_plugin file can be executed to copy/install DLL an icons to your ts3 plugin folder.

## Distribution Linux:
VS code and Makefile is used for compiling (Ubuntu).

After finishing the built, there will be the folowing files:
- lh2mqtt.so
- about.png
- config.png
- config_refresh.png
- lh2mqtt.png

Makefile takes care of copying these files to your ts3 plugins folder, sub-folder lh2mqtt.

## No Warranties in any way!
Please use this repository at your own risk and without any warranty.<br/>
Code is "as is" - if you have any trouble or ideas, feel free to fork and do your thing ;-)

I am NOT affiliated with TeamSpeak &copy; in any way.
