/*
 * TeamSpeak 3 demo plugin
 *
 * Copyright (c) TeamSpeak Systems GmbH
 */

#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32)
#pragma warning(disable : 4100) /* Disable Unreferenced parameter warning */
#include <Windows.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "teamspeak/public_definitions.h"
#include "teamspeak/public_errors.h"
#include "teamspeak/public_errors_rare.h"
#include "teamspeak/public_rare_definitions.h"
#include "ts3_functions.h"

#include "plugin.h"
#include "ini_wrapper.h"

static struct TS3Functions ts3Functions;

static int LocalTimeSafe(const time_t* t, struct tm* out) {
#ifdef _WIN32
    return localtime_s(out, t);
#else
    return localtime_r(t, out) == NULL ? 1 : 0;
#endif
}

#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define _snprintf(dest, size, fmt, ...) sprintf_s(dest, size, fmt, __VA_ARGS__)
#else
#include <stdio.h>
#define _strcpy(dest, destSize, src) snprintf(dest, destSize, "%s", src)
#define _snprintf(dest, size, fmt, ...) snprintf(dest, size, fmt, ##__VA_ARGS__)
#endif


#define PLUGIN_API_VERSION 26
// set accordingly in project's postbuild event and in ts3plugin_version()
//  + 3.6.0 requires API 26
//  + 3.5.0 requires API 24
//  + 3.3.0 requires API 23

#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128
#define INFODATA_BUFSIZE 128
#define SERVERINFO_BUFSIZE 256
#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128
#define BIG_BUFSIZE 1024
#define TS3LOG_BUFSIZE 2000
#define SHELL_BUFSIZE 2000

static char* pluginID = NULL;

static char configIniFileName[BIG_BUFSIZE];

static char configMqttExe[PATH_LEN];
static char configMqttHost[HOST_LEN];
static char configMqttPort[PORT_LEN];
static char configMqttUser[USER_LEN];
static char configMqttPassword[PASSWORD_LEN];
static char configMqttQos[QOS_LEN];
static char configMqttCafile[CAFILE_LEN];
static char configMqttSendStart[LOG_LEN];
static char configMqttSendStop[LOG_LEN];
static char configMqttTopicStart[TOPIC_LEN];
static char configMqttTopicStop[TOPIC_LEN];

static char configLhShowStart[LOG_LEN];
static char configLhShowStop[LOG_LEN];
static char configLhColorStart[COLOR_LEN];
static char configLhColorStop[COLOR_LEN];
static char configLhPrefixStart[PREFIX_LEN];
static char configLhPrefixStop[PREFIX_LEN];

static char configLogMqttMsg[LOG_LEN];

static char configGeneralLanguage[LANG_LEN];

#ifdef _WIN32
/* Helper function to convert wchar_T to Utf-8 encoded strings on Windows */
static int wcharToUtf8(const wchar_t* str, char** result)
{
    int outlen = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
    *result    = (char*)malloc(outlen);
    if (WideCharToMultiByte(CP_UTF8, 0, str, -1, *result, outlen, 0, 0) == 0) {
        *result = NULL;
        return -1;
    }
    return 0;
}
#endif

/*********************************** Required functions ************************************/
/*
 * If any of these required functions is not implemented, TS3 will refuse to load the plugin
 */

/* Unique name identifying this plugin */
const char* ts3plugin_name()
{
#ifdef _WIN32
    /* TeamSpeak expects UTF-8 encoded characters. Following demonstrates a possibility how to convert UTF-16 wchar_t into UTF-8. */
    static char* result = NULL; /* Static variable so it's allocated only once */
    if (!result) {
        const wchar_t* name = L"lh2mqtt";
        if (wcharToUtf8(name, &result) == -1) { /* Convert name into UTF-8 encoded result */
            result = "lh2mqtt";                 /* Conversion failed, fallback here */
        }
    }
    return result;
#else
    return "lh2mqtt";
#endif
}

/* Plugin version */
const char* ts3plugin_version()
{
    return "1.26.2";
}

/* Plugin API version. Must be the same as the clients API major version, else the plugin fails to load. */
int ts3plugin_apiVersion()
{
    return PLUGIN_API_VERSION;
}

/* Plugin author */
const char* ts3plugin_author()
{
    /* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "Little.Ben, DH6BS";
}

/* Plugin description */
const char* ts3plugin_description()
{
    /* If you want to use wchar_t, see ts3plugin_name() on how to use */
    if (strcmp(configGeneralLanguage, "DE") == 0)
        return "Dieses Plugin sendet den aktuell sprechenden User (LastHeard) an einen MQTT-Broker und/oder an den Channel-Tab.";

    return "This plugin transmits the currently speaking user (LastHeard) to an MQTT broker and/or the channel tab.";
}

/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs)
{
    ts3Functions = funcs;
}

/*
 * Custom code called right after loading the plugin. Returns 0 on success, 1 on failure.
 * If the function returns 1 on failure, the plugin will be unloaded again.
 */
int ts3plugin_init()
{
    char appPath[PATH_BUFSIZE];
    char resourcesPath[PATH_BUFSIZE];
    char configPath[PATH_BUFSIZE];
    char pluginPath[PATH_BUFSIZE];

    /* Your plugin init code here */
    printf("PLUGIN: init\n");

    /* Example on how to query application, resources and configuration paths from client */
    /* Note: Console client returns empty string for app and resources path */
    ts3Functions.getAppPath(appPath, PATH_BUFSIZE);
    ts3Functions.getResourcesPath(resourcesPath, PATH_BUFSIZE);
    ts3Functions.getConfigPath(configPath, PATH_BUFSIZE);
    ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE, pluginID);

    printf("PLUGIN: App path: %s\nResources path: %s\nConfig path: %s\nPlugin path: %s\n", appPath, resourcesPath, configPath, pluginPath);

    char* sectionName = NULL;
    char* keyName     = NULL;

    snprintf(configIniFileName, sizeof(configIniFileName), "%slh2mqtt.ini", pluginPath);
 
    CreateDefaultIniFile(configIniFileName);

#ifndef _WIN32
    ReadCompleteIniFile(configIniFileName);
#endif

     //-------------------------------
    sectionName = "MQTT";

    keyName = "SEND_START";
    ReadIniValue(configIniFileName, sectionName, keyName, configMqttSendStart, sizeof(configMqttSendStart), FALSE);

    keyName = "SEND_STOP";
    ReadIniValue(configIniFileName, sectionName, keyName, configMqttSendStop, sizeof(configMqttSendStop), FALSE);

    keyName = "PATH";
    ReadIniValue(configIniFileName, sectionName, keyName, configMqttExe, sizeof(configMqttExe), FALSE);

    keyName = "HOST";
    ReadIniValue(configIniFileName, sectionName, keyName, configMqttHost, sizeof(configMqttHost), FALSE);

    keyName = "PORT";
    ReadIniValue(configIniFileName, sectionName, keyName, configMqttPort, sizeof(configMqttPort), FALSE);

    keyName = "USER";
    ReadIniValue(configIniFileName, sectionName, keyName, configMqttUser, sizeof(configMqttUser), FALSE);

    keyName = "PASSWORD";
    ReadIniValue(configIniFileName, sectionName, keyName, configMqttPassword, sizeof(configMqttPassword), TRUE);

    keyName = "TOPIC_START";
    ReadIniValue(configIniFileName, sectionName, keyName, configMqttTopicStart, sizeof(configMqttTopicStart), FALSE);

    keyName = "TOPIC_STOP";
    ReadIniValue(configIniFileName, sectionName, keyName, configMqttTopicStop, sizeof(configMqttTopicStop), FALSE);

    keyName = "QOS";
    ReadIniValue(configIniFileName, sectionName, keyName, configMqttQos, sizeof(configMqttQos), FALSE);

    keyName = "CAFILE";
    ReadIniValue(configIniFileName, sectionName, keyName, configMqttCafile, sizeof(configMqttCafile), FALSE);

    //-------------------------------
    sectionName = "CHANNELTAB";

    keyName = "SHOW_START";
    ReadIniValue(configIniFileName, sectionName, keyName, configLhShowStart, sizeof(configLhShowStart), FALSE);

    keyName = "SHOW_STOP";
    ReadIniValue(configIniFileName, sectionName, keyName, configLhShowStop, sizeof(configLhShowStop), FALSE);

    keyName = "COLOR_START";
    ReadIniValue(configIniFileName, sectionName, keyName, configLhColorStart, sizeof(configLhColorStart), FALSE);

    keyName = "COLOR_STOP";
    ReadIniValue(configIniFileName, sectionName, keyName, configLhColorStop, sizeof(configLhColorStop), FALSE);

    keyName = "PREFIX_START";
    ReadIniValue(configIniFileName, sectionName, keyName, configLhPrefixStart, sizeof(configLhPrefixStart), FALSE);

    keyName = "PREFIX_STOP";
    ReadIniValue(configIniFileName, sectionName, keyName, configLhPrefixStop, sizeof(configLhPrefixStop), FALSE);

    //-------------------------------
    sectionName = "LOGGING";

    keyName = "LOG_MQTT_MSG";
    ReadIniValue(configIniFileName, sectionName, keyName, configLogMqttMsg, sizeof(configLogMqttMsg), FALSE);

    // Adding missing key to older INI-files
    if (strlen(configLogMqttMsg) == 0)
    {
        BOOL result = WriteIniValue(configIniFileName, sectionName, keyName, "1");
        if (result == TRUE) {
            snprintf(configLogMqttMsg, sizeof(configLogMqttMsg), "%s", "1");
            printf("PLUGIN: missing key added to config file: [%s]%s=%s\n", sectionName, keyName, configLogMqttMsg);

            char msg[TS3LOG_BUFSIZE];
            snprintf(msg, sizeof(msg), "Konfigurationsdatei wurde um fehlenden Schluessel ergaenzt: [%s]%s=%s", sectionName, keyName, configLogMqttMsg);
            ts3Functions.logMessage(msg, LogLevel_INFO, "Plugin lh2mqtt", 0);
        }
        else
        {
            printf("PLUGIN: ERROR: missing key was NOT added to config file, key should be: [%s]%s=1\n", sectionName, keyName);
            char msg[TS3LOG_BUFSIZE];
            snprintf(msg, sizeof(msg), "Konfigurationsdatei konnte NICHT um fehlenden Schluessel ergaenzt werden - soll: [%s]%s=1 in %s", sectionName, keyName, configIniFileName);
            ts3Functions.logMessage(msg, LogLevel_ERROR, "Plugin lh2mqtt", 0);

        }
    }

    //-------------------------------
    sectionName = "GENERAL";

    keyName = "LANGUAGE";
    ReadIniValue(configIniFileName, sectionName, keyName, configGeneralLanguage, sizeof(configGeneralLanguage), FALSE);

    // Adding missing key to older INI-files
    if (strlen(configGeneralLanguage) == 0)
    {
        BOOL result = WriteIniValue(configIniFileName, sectionName, keyName, "DE");
        if (result == TRUE) {
            snprintf(configGeneralLanguage, sizeof(configGeneralLanguage), "%s", "DE");
            printf("PLUGIN: missing key added to config file: [%s]%s=%s\n", sectionName, keyName, configGeneralLanguage);

            char msg[TS3LOG_BUFSIZE];
            snprintf(msg, sizeof(msg), "Konfigurationsdatei wurde um fehlenden Schluessel ergaenzt: [%s]%s=%s", sectionName, keyName, configGeneralLanguage);
            ts3Functions.logMessage(msg, LogLevel_INFO, "Plugin lh2mqtt", 0);
        }
        else
        {
            printf("PLUGIN: ERROR: missing key was NOT added to config file, key should be: [%s]%s=DE oder EN\n", sectionName, keyName);
            char msg[TS3LOG_BUFSIZE];
            snprintf(msg, sizeof(msg), "Konfigurationsdatei konnte NICHT um fehlenden Schluessel ergaenzt werden - soll: [%s]%s=DE oder EN in %s", sectionName, keyName, configIniFileName);
            ts3Functions.logMessage(msg, LogLevel_ERROR, "Plugin lh2mqtt", 0);
        }
    }

    FlushIniFile();
    
    char msg0[TS3LOG_BUFSIZE];
    snprintf(msg0, sizeof(msg0), "Konfigurationsdatei neu einlesen: %s", configIniFileName);
    ts3Functions.logMessage(msg0, LogLevel_INFO, "Plugin lh2mqtt", 0);

    char msg1a[TS3LOG_BUFSIZE];
    snprintf(msg1a, sizeof(msg1a), "[INI-MQTT|1] Path=%s, Host=%s, Port=%s, User=%s, Password=***, Qos=%s, Cafile=%s", 
        configMqttExe, configMqttHost, configMqttPort, configMqttUser, configMqttQos, configMqttCafile);
        ts3Functions.logMessage(msg1a, LogLevel_INFO, "Plugin lh2mqtt", 0);

    char msg1b[TS3LOG_BUFSIZE];
        snprintf(msg1b, sizeof(msg1b), "[INI-MQTT|2] SendStart=%s, SendStop=%s, TopicStart=%s, TopicStop=%s",
            configMqttSendStart, configMqttSendStop, configMqttTopicStart, configMqttTopicStop);
        ts3Functions.logMessage(msg1b, LogLevel_INFO, "Plugin lh2mqtt", 0);

    char msg2[TS3LOG_BUFSIZE];
    snprintf(msg2, sizeof(msg2), "[INI-CHANNELTAB] ShowStart=%s, ShowStop=%s, ColorStart=%s, ColorStop=%s, PrefixStart=%s, PrefixStop=%s",
        configLhShowStart, configLhShowStop, configLhColorStart, configLhColorStop, configLhPrefixStart, configLhPrefixStop);
    ts3Functions.logMessage(msg2, LogLevel_INFO, "Plugin lh2mqtt", 0);

    char msg3[TS3LOG_BUFSIZE];
    snprintf(msg3, sizeof(msg3), "[INI-LOGGING] LogMqttMsg=%s", configLogMqttMsg);
    ts3Functions.logMessage(msg3, LogLevel_INFO, "Plugin lh2mqtt", 0);

    char msg4[TS3LOG_BUFSIZE];
    snprintf(msg4, sizeof(msg4), "[INI-GENERAL] Language=%s", configGeneralLanguage);
    ts3Functions.logMessage(msg4, LogLevel_INFO, "Plugin lh2mqtt", 0);


    return 0; /* 0 = success, 1 = failure, -2 = failure but client will not show a "failed to load" warning */
              /* -2 is a very special case and should only be used if a plugin displays a dialog (e.g. overlay) asking the user to disable
	 * the plugin again, avoiding the show another dialog by the client telling the user the plugin failed to load.
	 * For normal case, if a plugin really failed to load because of an error, the correct return value is 1. */
}

/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown()
{
    /* Your plugin cleanup code here */
    printf("PLUGIN: shutdown\n");

    /*
	 * Note:
	 * If your plugin implements a settings dialog, it must be closed and deleted here, else the
	 * TeamSpeak client will most likely crash (DLL removed but dialog from DLL code still open).
	 */

    /* Free pluginID if we registered it */
    if (pluginID) {
        free(pluginID);
        pluginID = NULL;
    }
}

/****************************** Optional functions ********************************/
/*
 * Following functions are optional, if not needed you don't need to implement them.
 */

/* Tell client if plugin offers a configuration window. If this function is not implemented, it's an assumed "does not offer" (PLUGIN_OFFERS_NO_CONFIGURE). */
int ts3plugin_offersConfigure()
{
    printf("PLUGIN: offersConfigure\n");
    /*
	 * Return values:
	 * PLUGIN_OFFERS_NO_CONFIGURE         - Plugin does not implement ts3plugin_configure
	 * PLUGIN_OFFERS_CONFIGURE_NEW_THREAD - Plugin does implement ts3plugin_configure and requests to run this function in an own thread
	 * PLUGIN_OFFERS_CONFIGURE_QT_THREAD  - Plugin does implement ts3plugin_configure and requests to run this function in the Qt GUI thread
	 */
    return PLUGIN_OFFERS_CONFIGURE_NEW_THREAD; /* In this case ts3plugin_configure does not need to be implemented */
}

/* Plugin might offer a configuration window. If ts3plugin_offersConfigure returns 0, this function does not need to be implemented. */
void ts3plugin_configure(void* handle, void* qParentWidget)
{
    printf("PLUGIN: configure\n");

    char command[SHELL_BUFSIZE];
    char pluginPath[PATH_BUFSIZE];
    ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE, pluginID);

    #ifdef _WIN32
        snprintf(command, sizeof(command), "notepad.exe \"%slh2mqtt.ini\"", pluginPath);
    #else
        snprintf(command, sizeof(command), "xdg-open \"%slh2mqtt.ini\"", pluginPath);
    #endif

    ExecuteCommandInBackground(command, "", 0);
    #ifdef _WIN32
        ts3plugin_init();
    #else
        if (strcmp(configGeneralLanguage, "DE") == 0)
            ts3Functions.printMessageToCurrentTab("[b]Bitte nach dem Speichern der Änderungen, die Konfiguration via Plugins-Menü neu laden[/b]");
        else
            ts3Functions.printMessageToCurrentTab("[b]Please reload configuration via plugins menu after saving changes to INI file[/b]");
    #endif
}

/*
 * If the plugin wants to use error return codes, plugin commands, hotkeys or menu items, it needs to register a command ID. This function will be
 * automatically called after the plugin was initialized. This function is optional. If you don't use these features, this function can be omitted.
 * Note the passed pluginID parameter is no longer valid after calling this function, so you must copy it and store it in the plugin.
 */
void ts3plugin_registerPluginID(const char* id)
{
    const size_t sz = strlen(id) + 1;
    pluginID        = (char*)malloc(sz * sizeof(char));
    _strcpy(pluginID, sz, id); /* The id buffer will invalidate after exiting this function */
    printf("PLUGIN: registerPluginID: %s\n", pluginID);
}

/* Plugin command keyword. Return NULL or "" if not used. */
const char* ts3plugin_commandKeyword()
{
    return "lh2mqtt";
}

static void print_and_free_bookmarks_list(struct PluginBookmarkList* list)
{
    int i;
    for (i = 0; i < list->itemcount; ++i) {
        if (list->items[i].isFolder) {
            printf("Folder: name=%s\n", list->items[i].name);
            print_and_free_bookmarks_list(list->items[i].folder);
            ts3Functions.freeMemory(list->items[i].name);
        } else {
            printf("Bookmark: name=%s uuid=%s\n", list->items[i].name, list->items[i].uuid);
            ts3Functions.freeMemory(list->items[i].name);
            ts3Functions.freeMemory(list->items[i].uuid);
        }
    }
    ts3Functions.freeMemory(list);
}

/* Plugin processes console command. Return 0 if plugin handled the command, 1 if not handled. */
int ts3plugin_processCommand(uint64 serverConnectionHandlerID, const char* command)
{
    char  buf[COMMAND_BUFSIZE];
    char *s, *param1 = NULL, *param2 = NULL;
    int   i                                                                                                                                                                                                = 0;
    enum { CMD_NONE = 0, CMD_JOIN, CMD_COMMAND, CMD_SERVERINFO, CMD_CHANNELINFO, CMD_AVATAR, CMD_ENABLEMENU, CMD_SUBSCRIBE, CMD_UNSUBSCRIBE, CMD_SUBSCRIBEALL, CMD_UNSUBSCRIBEALL, CMD_BOOKMARKSLIST } cmd = CMD_NONE;
#ifdef _WIN32
    char* context = NULL;
#endif

    printf("PLUGIN: process command: '%s'\n", command);

    _strcpy(buf, COMMAND_BUFSIZE, command);
#ifdef _WIN32
    s = strtok_s(buf, " ", &context);
#else
    s = strtok(buf, " ");
#endif
    while (s != NULL) {
        if (i == 0) {
            if (!strcmp(s, "join")) {
                cmd = CMD_JOIN;
            } else if (!strcmp(s, "command")) {
                cmd = CMD_COMMAND;
            } else if (!strcmp(s, "serverinfo")) {
                cmd = CMD_SERVERINFO;
            } else if (!strcmp(s, "channelinfo")) {
                cmd = CMD_CHANNELINFO;
            } else if (!strcmp(s, "avatar")) {
                cmd = CMD_AVATAR;
            } else if (!strcmp(s, "enablemenu")) {
                cmd = CMD_ENABLEMENU;
            } else if (!strcmp(s, "subscribe")) {
                cmd = CMD_SUBSCRIBE;
            } else if (!strcmp(s, "unsubscribe")) {
                cmd = CMD_UNSUBSCRIBE;
            } else if (!strcmp(s, "subscribeall")) {
                cmd = CMD_SUBSCRIBEALL;
            } else if (!strcmp(s, "unsubscribeall")) {
                cmd = CMD_UNSUBSCRIBEALL;
            } else if (!strcmp(s, "bookmarkslist")) {
                cmd = CMD_BOOKMARKSLIST;
            }
        } else if (i == 1) {
            param1 = s;
        } else {
            param2 = s;
        }
#ifdef _WIN32
        s = strtok_s(NULL, " ", &context);
#else
        s = strtok(NULL, " ");
#endif
        i++;
    }

    switch (cmd) {
        case CMD_NONE:
            return 1;  /* Command not handled by plugin */
        case CMD_JOIN: /* /test join <channelID> [optionalCannelPassword] */
            if (param1) {
                uint64 channelID = (uint64)atoi(param1);
                char*  password  = param2 ? param2 : "";
                char   returnCode[RETURNCODE_BUFSIZE];
                anyID  myID;

                /* Get own clientID */
                if (ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
                    ts3Functions.logMessage("Error querying client ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
                    break;
                }

                /* Create return code for requestClientMove function call. If creation fails, returnCode will be NULL, which can be
				 * passed into the client functions meaning no return code is used.
				 * Note: To use return codes, the plugin needs to register a plugin ID using ts3plugin_registerPluginID */
                ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);

                /* In a real world plugin, the returnCode should be remembered (e.g. in a dynamic STL vector, if it's a C++ plugin).
				 * onServerErrorEvent can then check the received returnCode, compare with the remembered ones and thus identify
				 * which function call has triggered the event and react accordingly. */

                /* Request joining specified channel using above created return code */
                if (ts3Functions.requestClientMove(serverConnectionHandlerID, myID, channelID, password, returnCode) != ERROR_ok) {
                    ts3Functions.logMessage("Error requesting client move", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
                }
            } else {
                ts3Functions.printMessageToCurrentTab("Missing channel ID parameter.");
            }
            break;
        case CMD_COMMAND: /* /test command <command> */
            if (param1) {
                /* Send plugin command to all clients in current channel. In this case targetIds is unused and can be NULL. */
                if (pluginID) {
                    /* See ts3plugin_registerPluginID for how to obtain a pluginID */
                    printf("PLUGIN: Sending plugin command to current channel: %s\n", param1);
                    ts3Functions.sendPluginCommand(serverConnectionHandlerID, pluginID, param1, PluginCommandTarget_CURRENT_CHANNEL, NULL, NULL);
                } else {
                    printf("PLUGIN: Failed to send plugin command, was not registered.\n");
                }
            } else {
                ts3Functions.printMessageToCurrentTab("Missing command parameter.");
            }
            break;
        case CMD_SERVERINFO: { /* /test serverinfo */
            /* Query host, port and server password of current server tab.
			 * The password parameter can be NULL if the plugin does not want to receive the server password.
			 * Note: Server password is only available if the user has actually used it when connecting. If a user has
			 * connected with the permission to ignore passwords (b_virtualserver_join_ignore_password) and the password,
			 * was not entered, it will not be available.
			 * getServerConnectInfo returns 0 on success, 1 on error or if current server tab is disconnected. */
            char host[SERVERINFO_BUFSIZE];
            /*char password[SERVERINFO_BUFSIZE];*/
            char*          password = NULL; /* Don't receive server password */
            unsigned short port;
            if (!ts3Functions.getServerConnectInfo(serverConnectionHandlerID, host, &port, password, SERVERINFO_BUFSIZE)) {
                char msg[CHANNELINFO_BUFSIZE];
                snprintf(msg, sizeof(msg), "Server Connect Info: %s:%d", host, port);
                ts3Functions.printMessageToCurrentTab(msg);
            } else {
                ts3Functions.printMessageToCurrentTab("No server connect info available.");
            }
            break;
        }
        case CMD_CHANNELINFO: { /* /test channelinfo */
            /* Query channel path and password of current server tab.
			 * The password parameter can be NULL if the plugin does not want to receive the channel password.
			 * Note: Channel password is only available if the user has actually used it when entering the channel. If a user has
			 * entered a channel with the permission to ignore passwords (b_channel_join_ignore_password) and the password,
			 * was not entered, it will not be available.
			 * getChannelConnectInfo returns 0 on success, 1 on error or if current server tab is disconnected. */
            char path[CHANNELINFO_BUFSIZE];
            /*char password[CHANNELINFO_BUFSIZE];*/
            char* password = NULL; /* Don't receive channel password */

            /* Get own clientID and channelID */
            anyID  myID;
            uint64 myChannelID;
            if (ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
                ts3Functions.logMessage("Error querying client ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
                break;
            }
            /* Get own channel ID */
            if (ts3Functions.getChannelOfClient(serverConnectionHandlerID, myID, &myChannelID) != ERROR_ok) {
                ts3Functions.logMessage("Error querying channel ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
                break;
            }

            /* Get channel connect info of own channel */
            if (!ts3Functions.getChannelConnectInfo(serverConnectionHandlerID, myChannelID, path, password, CHANNELINFO_BUFSIZE)) {
                char msg[BIG_BUFSIZE];
                snprintf(msg, sizeof(msg), "Channel Connect Info: %s", path);
                ts3Functions.printMessageToCurrentTab(msg);
            } else {
                ts3Functions.printMessageToCurrentTab("No channel connect info available.");
            }
            break;
        }
        case CMD_AVATAR: { /* /test avatar <clientID> */
            char         avatarPath[PATH_BUFSIZE];
            anyID        clientID = (anyID)atoi(param1);
            unsigned int error;

            memset(avatarPath, 0, PATH_BUFSIZE);
            error = ts3Functions.getAvatar(serverConnectionHandlerID, clientID, avatarPath, PATH_BUFSIZE);
            if (error == ERROR_ok) {      /* ERROR_ok means the client has an avatar set. */
                if (strlen(avatarPath)) { /* Avatar path contains the full path to the avatar image in the TS3Client cache directory */
                    printf("Avatar path: %s\n", avatarPath);
                } else { /* Empty avatar path means the client has an avatar but the image has not yet been cached. The TeamSpeak
						  * client will automatically start the download and call onAvatarUpdated when done */
                    printf("Avatar not yet downloaded, waiting for onAvatarUpdated...\n");
                }
            } else if (error == ERROR_database_empty_result) { /* Not an error, the client simply has no avatar set */
                printf("Client has no avatar\n");
            } else { /* Other error occured (invalid server connection handler ID, invalid client ID, file io error etc) */
                printf("Error getting avatar: %d\n", error);
            }
            break;
        }
        case CMD_ENABLEMENU: /* /test enablemenu <menuID> <0|1> */
            if (param1) {
                int menuID = atoi(param1);
                int enable = param2 ? atoi(param2) : 0;
                ts3Functions.setPluginMenuEnabled(pluginID, menuID, enable);
            } else {
                ts3Functions.printMessageToCurrentTab("Usage is: /test enablemenu <menuID> <0|1>");
            }
            break;
        case CMD_SUBSCRIBE: /* /test subscribe <channelID> */
            if (param1) {
                char   returnCode[RETURNCODE_BUFSIZE];
                uint64 channelIDArray[2];
                channelIDArray[0] = (uint64)atoi(param1);
                channelIDArray[1] = 0;
                ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
                if (ts3Functions.requestChannelSubscribe(serverConnectionHandlerID, channelIDArray, returnCode) != ERROR_ok) {
                    ts3Functions.logMessage("Error subscribing channel", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
                }
            }
            break;
        case CMD_UNSUBSCRIBE: /* /test unsubscribe <channelID> */
            if (param1) {
                char   returnCode[RETURNCODE_BUFSIZE];
                uint64 channelIDArray[2];
                channelIDArray[0] = (uint64)atoi(param1);
                channelIDArray[1] = 0;
                ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
                if (ts3Functions.requestChannelUnsubscribe(serverConnectionHandlerID, channelIDArray, NULL) != ERROR_ok) {
                    ts3Functions.logMessage("Error unsubscribing channel", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
                }
            }
            break;
        case CMD_SUBSCRIBEALL: { /* /test subscribeall */
            char returnCode[RETURNCODE_BUFSIZE];
            ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
            if (ts3Functions.requestChannelSubscribeAll(serverConnectionHandlerID, returnCode) != ERROR_ok) {
                ts3Functions.logMessage("Error subscribing channel", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
            }
            break;
        }
        case CMD_UNSUBSCRIBEALL: { /* /test unsubscribeall */
            char returnCode[RETURNCODE_BUFSIZE];
            ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
            if (ts3Functions.requestChannelUnsubscribeAll(serverConnectionHandlerID, returnCode) != ERROR_ok) {
                ts3Functions.logMessage("Error subscribing channel", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
            }
            break;
        }
        case CMD_BOOKMARKSLIST: { /* test bookmarkslist */
            struct PluginBookmarkList* list;
            unsigned int               error = ts3Functions.getBookmarkList(&list);
            if (error == ERROR_ok) {
                print_and_free_bookmarks_list(list);
            } else {
                ts3Functions.logMessage("Error getting bookmarks list", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            }
            break;
        }
    }

    return 0; /* Plugin handled command */
}

/* Client changed current server connection handler */
void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID)
{
    printf("PLUGIN: currentServerConnectionChanged %llu (%llu)\n", (long long unsigned int)serverConnectionHandlerID, (long long unsigned int)ts3Functions.getCurrentServerConnectionHandlerID());
}

/*
 * Implement the following three functions when the plugin should display a line in the server/channel/client info.
 * If any of ts3plugin_infoTitle, ts3plugin_infoData or ts3plugin_freeMemory is missing, the info text will not be displayed.
 */

/* Static title shown in the left column in the info frame */
const char* ts3plugin_infoTitle()
{
    return "lh2mqtt info";
}

/*
 * Dynamic content shown in the right column in the info frame. Memory for the data string needs to be allocated in this
 * function. The client will call ts3plugin_freeMemory once done with the string to release the allocated memory again.
 * Check the parameter "type" if you want to implement this feature only for specific item types. Set the parameter
 * "data" to NULL to have the client ignore the info data.
 */
void ts3plugin_infoData(uint64 serverConnectionHandlerID, uint64 id, enum PluginItemType type, char** data)
{
    char* name;

    /* For demonstration purpose, display the name of the currently selected server, channel or client. */
    switch (type) {
        case PLUGIN_SERVER:
            if (ts3Functions.getServerVariableAsString(serverConnectionHandlerID, VIRTUALSERVER_NAME, &name) != ERROR_ok) {
                printf("Error getting virtual server name\n");
                return;
            }
            break;
        case PLUGIN_CHANNEL:
            if (ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, id, CHANNEL_NAME, &name) != ERROR_ok) {
                printf("Error getting channel name\n");
                return;
            }
            break;
        case PLUGIN_CLIENT:
            if (ts3Functions.getClientVariableAsString(serverConnectionHandlerID, (anyID)id, CLIENT_NICKNAME, &name) != ERROR_ok) {
                printf("Error getting client nickname\n");
                return;
            }
            break;
        default:
            printf("Invalid item type: %d\n", type);
            data = NULL; /* Ignore */
            return;
    }

    *data = (char*)malloc(INFODATA_BUFSIZE * sizeof(char));                   /* Must be allocated in the plugin! */
    snprintf(*data, INFODATA_BUFSIZE, "The nickname is [I]\"%s\"[/I]", name); /* bbCode is supported. HTML is not supported */
    ts3Functions.freeMemory(name);
}

/* Required to release the memory for parameter "data" allocated in ts3plugin_infoData and ts3plugin_initMenus */
void ts3plugin_freeMemory(void* data)
{
    free(data);
}

/*
 * Plugin requests to be always automatically loaded by the TeamSpeak 3 client unless
 * the user manually disabled it in the plugin dialog.
 * This function is optional. If missing, no autoload is assumed.
 */
int ts3plugin_requestAutoload()
{
    return 0; /* 1 = request autoloaded, 0 = do not request autoload */
}

/* Helper function to create a menu item */
static struct PluginMenuItem* createMenuItem(enum PluginMenuType type, int id, const char* text, const char* icon)
{
    struct PluginMenuItem* menuItem = (struct PluginMenuItem*)malloc(sizeof(struct PluginMenuItem));
    menuItem->type                  = type;
    menuItem->id                    = id;
    _strcpy(menuItem->text, PLUGIN_MENU_BUFSZ, text);
    _strcpy(menuItem->icon, PLUGIN_MENU_BUFSZ, icon);
    return menuItem;
}

/* Some makros to make the code to create menu items a bit more readable */
#define BEGIN_CREATE_MENUS(x)                                                                                                                                                                                                                                  \
    const size_t sz = x + 1;                                                                                                                                                                                                                                   \
    size_t       n  = 0;                                                                                                                                                                                                                                       \
    *menuItems      = (struct PluginMenuItem**)malloc(sizeof(struct PluginMenuItem*) * sz);
#define CREATE_MENU_ITEM(a, b, c, d) (*menuItems)[n++] = createMenuItem(a, b, c, d);
#define END_CREATE_MENUS                                                                                                                                                                                                                                       \
    (*menuItems)[n++] = NULL;                                                                                                                                                                                                                                  \
    assert(n == sz);

/*
 * Menu IDs for this plugin. Pass these IDs when creating a menuitem to the TS3 client. When the menu item is triggered,
 * ts3plugin_onMenuItemEvent will be called passing the menu ID of the triggered menu item.
 * These IDs are freely choosable by the plugin author. It's not really needed to use an enum, it just looks prettier.
 */
enum { MENU_ID_CLIENT_1 = 1, MENU_ID_CLIENT_2, MENU_ID_CHANNEL_1, MENU_ID_CHANNEL_2, MENU_ID_CHANNEL_3, MENU_ID_GLOBAL_1, MENU_ID_GLOBAL_2, MENU_ID_GLOBAL_3 };

/*
 * Initialize plugin menus.
 * This function is called after ts3plugin_init and ts3plugin_registerPluginID. A pluginID is required for plugin menus to work.
 * Both ts3plugin_registerPluginID and ts3plugin_freeMemory must be implemented to use menus.
 * If plugin menus are not used by a plugin, do not implement this function or return NULL.
 */
void ts3plugin_initMenus(struct PluginMenuItem*** menuItems, char** menuIcon)
{
    /*
	 * Create the menus
	 * There are three types of menu items:
	 * - PLUGIN_MENU_TYPE_CLIENT:  Client context menu
	 * - PLUGIN_MENU_TYPE_CHANNEL: Channel context menu
	 * - PLUGIN_MENU_TYPE_GLOBAL:  "Plugins" menu in menu bar of main window
	 *
	 * Menu IDs are used to identify the menu item when ts3plugin_onMenuItemEvent is called
	 *
	 * The menu text is required, max length is 128 characters
	 *
	 * The icon is optional, max length is 128 characters. When not using icons, just pass an empty string.
	 * Icons are loaded from a subdirectory in the TeamSpeak client plugins folder. The subdirectory must be named like the
	 * plugin filename, without dll/so/dylib suffix
	 * e.g. for "test_plugin.dll", icon "1.png" is loaded from <TeamSpeak 3 Client install dir>\plugins\test_plugin\1.png
	 */

    BEGIN_CREATE_MENUS(3); /* IMPORTANT: Number of menu items must be correct! */
    //CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CLIENT, MENU_ID_CLIENT_1, "Client item 1", "1.png");
    //CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CLIENT, MENU_ID_CLIENT_2, "Client item 2", "2.png");
    //CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CHANNEL, MENU_ID_CHANNEL_1, "Channel item 1", "1.png");
    //CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CHANNEL, MENU_ID_CHANNEL_2, "Channel item 2", "2.png");
    //CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_CHANNEL, MENU_ID_CHANNEL_3, "Channel item 3", "3.png");
    CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_1, "Konfiguration &editieren", "config.png");
    CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_2, "Konfiguration &neu laden", "config_refresh.png");
    CREATE_MENU_ITEM(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_GLOBAL_3, "&Info", "about.png");
    END_CREATE_MENUS; /* Includes an assert checking if the number of menu items matched */

    /*
	 * Specify an optional icon for the plugin. This icon is used for the plugins submenu within context and main menus
	 * If unused, set menuIcon to NULL
	 */
    *menuIcon = (char*)malloc(PLUGIN_MENU_BUFSZ * sizeof(char));
    _strcpy(*menuIcon, PLUGIN_MENU_BUFSZ, "lh2mqtt.png");

    /*
	 * Menus can be enabled or disabled with: ts3Functions.setPluginMenuEnabled(pluginID, menuID, 0|1);
	 * Test it with plugin command: /test enablemenu <menuID> <0|1>
	 * Menus are enabled by default. Please note that shown menus will not automatically enable or disable when calling this function to
	 * ensure Qt menus are not modified by any thread other the UI thread. The enabled or disable state will change the next time a
	 * menu is displayed.
	 */
    /* For example, this would disable MENU_ID_GLOBAL_2: */
    /* ts3Functions.setPluginMenuEnabled(pluginID, MENU_ID_GLOBAL_2, 0); */

    /* All memory allocated in this function will be automatically released by the TeamSpeak client later by calling ts3plugin_freeMemory */
}

/* Helper function to create a hotkey */
static struct PluginHotkey* createHotkey(const char* keyword, const char* description)
{
    struct PluginHotkey* hotkey = (struct PluginHotkey*)malloc(sizeof(struct PluginHotkey));
    _strcpy(hotkey->keyword, PLUGIN_HOTKEY_BUFSZ, keyword);
    _strcpy(hotkey->description, PLUGIN_HOTKEY_BUFSZ, description);
    return hotkey;
}

/* Some makros to make the code to create hotkeys a bit more readable */
#define BEGIN_CREATE_HOTKEYS(x)                                                                                                                                                                                                                                \
    const size_t sz = x + 1;                                                                                                                                                                                                                                   \
    size_t       n  = 0;                                                                                                                                                                                                                                       \
    *hotkeys        = (struct PluginHotkey**)malloc(sizeof(struct PluginHotkey*) * sz);
#define CREATE_HOTKEY(a, b) (*hotkeys)[n++] = createHotkey(a, b);
#define END_CREATE_HOTKEYS                                                                                                                                                                                                                                     \
    (*hotkeys)[n++] = NULL;                                                                                                                                                                                                                                    \
    assert(n == sz);

/*
 * Initialize plugin hotkeys. If your plugin does not use this feature, this function can be omitted.
 * Hotkeys require ts3plugin_registerPluginID and ts3plugin_freeMemory to be implemented.
 * This function is automatically called by the client after ts3plugin_init.
 */
void ts3plugin_initHotkeys(struct PluginHotkey*** hotkeys)
{
    /* Register hotkeys giving a keyword and a description.
	 * The keyword will be later passed to ts3plugin_onHotkeyEvent to identify which hotkey was triggered.
	 * The description is shown in the clients hotkey dialog. */
    BEGIN_CREATE_HOTKEYS(3); /* Create 3 hotkeys. Size must be correct for allocating memory. */
    CREATE_HOTKEY("keyword_1", "Test hotkey 1");
    CREATE_HOTKEY("keyword_2", "Test hotkey 2");
    CREATE_HOTKEY("keyword_3", "Test hotkey 3");
    END_CREATE_HOTKEYS;

    /* The client will call ts3plugin_freeMemory to release all allocated memory */
}

/************************** TeamSpeak callbacks ***************************/
/*
 * Following functions are optional, feel free to remove unused callbacks.
 * See the clientlib documentation for details on each function.
 */

/* Clientlib */

void ts3plugin_onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber)
{
    /* Some example code following to show how to use the information query functions. */

    if (newStatus == STATUS_CONNECTION_ESTABLISHED) { /* connection established and we have client and channels available */
        char*        s;
        char         msg[1024];
        anyID        myID;
        uint64*      ids;
        size_t       i;
        unsigned int error;

        /* Print clientlib version */
        if (ts3Functions.getClientLibVersion(&s) == ERROR_ok) {
            printf("PLUGIN: Client lib version: %s\n", s);
            ts3Functions.freeMemory(s); /* Release string */
        } else {
            ts3Functions.logMessage("Error querying client lib version", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }

        /* Write plugin name and version to log */
        snprintf(msg, sizeof(msg), "Plugin %s, Version %s, Author: %s", ts3plugin_name(), ts3plugin_version(), ts3plugin_author());
        ts3Functions.logMessage(msg, LogLevel_INFO, "Plugin", serverConnectionHandlerID);

        /* Print virtual server name */
        if ((error = ts3Functions.getServerVariableAsString(serverConnectionHandlerID, VIRTUALSERVER_NAME, &s)) != ERROR_ok) {
            if (error != ERROR_not_connected) { /* Don't spam error in this case (failed to connect) */
                ts3Functions.logMessage("Error querying server name", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            }
            return;
        }
        printf("PLUGIN: Server name: %s\n", s);
        ts3Functions.freeMemory(s);

        /* Print virtual server welcome message */
        if (ts3Functions.getServerVariableAsString(serverConnectionHandlerID, VIRTUALSERVER_WELCOMEMESSAGE, &s) != ERROR_ok) {
            ts3Functions.logMessage("Error querying server welcome message", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }
        printf("PLUGIN: Server welcome message: %s\n", s);
        ts3Functions.freeMemory(s); /* Release string */

        /* Print own client ID and nickname on this server */
        if (ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
            ts3Functions.logMessage("Error querying client ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }
        if (ts3Functions.getClientSelfVariableAsString(serverConnectionHandlerID, CLIENT_NICKNAME, &s) != ERROR_ok) {
            ts3Functions.logMessage("Error querying client nickname", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }
        printf("PLUGIN: My client ID = %d, nickname = %s\n", myID, s);
        ts3Functions.freeMemory(s);

        /* Print list of all channels on this server */
        if (ts3Functions.getChannelList(serverConnectionHandlerID, &ids) != ERROR_ok) {
            ts3Functions.logMessage("Error getting channel list", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }
        printf("PLUGIN: Available channels:\n");
        for (i = 0; ids[i]; i++) {
            /* Query channel name */
            if (ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, ids[i], CHANNEL_NAME, &s) != ERROR_ok) {
                ts3Functions.logMessage("Error querying channel name", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
                return;
            }
            printf("PLUGIN: Channel ID = %llu, name = %s\n", (long long unsigned int)ids[i], s);
            ts3Functions.freeMemory(s);
        }
        ts3Functions.freeMemory(ids); /* Release array */

        /* Print list of existing server connection handlers */
        printf("PLUGIN: Existing server connection handlers:\n");
        if (ts3Functions.getServerConnectionHandlerList(&ids) != ERROR_ok) {
            ts3Functions.logMessage("Error getting server list", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }
        for (i = 0; ids[i]; i++) {
            if ((error = ts3Functions.getServerVariableAsString(ids[i], VIRTUALSERVER_NAME, &s)) != ERROR_ok) {
                if (error != ERROR_not_connected) { /* Don't spam error in this case (failed to connect) */
                    ts3Functions.logMessage("Error querying server name", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
                }
                continue;
            }
            printf("- %llu - %s\n", (long long unsigned int)ids[i], s);
            ts3Functions.freeMemory(s);
        }
        ts3Functions.freeMemory(ids);
    }
}

void ts3plugin_onNewChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 channelParentID) {}

void ts3plugin_onNewChannelCreatedEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 channelParentID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {}

void ts3plugin_onDelChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {}

void ts3plugin_onChannelMoveEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 newChannelParentID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {}

void ts3plugin_onUpdateChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID) {}

void ts3plugin_onUpdateChannelEditedEvent(uint64 serverConnectionHandlerID, uint64 channelID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {}

void ts3plugin_onUpdateClientEvent(uint64 serverConnectionHandlerID, anyID clientID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier) {}

void ts3plugin_onClientMoveEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* moveMessage) {}

void ts3plugin_onClientMoveSubscriptionEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility) {}

void ts3plugin_onClientMoveTimeoutEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* timeoutMessage) {}

void ts3plugin_onClientMoveMovedEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID moverID, const char* moverName, const char* moverUniqueIdentifier, const char* moveMessage) {}

void ts3plugin_onClientKickFromChannelEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, const char* kickMessage) {}

void ts3plugin_onClientKickFromServerEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, const char* kickMessage) {}

void ts3plugin_onClientIDsEvent(uint64 serverConnectionHandlerID, const char* uniqueClientIdentifier, anyID clientID, const char* clientName) {}

void ts3plugin_onClientIDsFinishedEvent(uint64 serverConnectionHandlerID) {}

void ts3plugin_onServerEditedEvent(uint64 serverConnectionHandlerID, anyID editerID, const char* editerName, const char* editerUniqueIdentifier) {}

void ts3plugin_onServerUpdatedEvent(uint64 serverConnectionHandlerID) {}

int ts3plugin_onServerErrorEvent(uint64 serverConnectionHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, const char* extraMessage)
{
    printf("PLUGIN: onServerErrorEvent %llu %s %d %s\n", (long long unsigned int)serverConnectionHandlerID, errorMessage, error, (returnCode ? returnCode : ""));
    if (returnCode) {
        /* A plugin could now check the returnCode with previously (when calling a function) remembered returnCodes and react accordingly */
        /* In case of using a a plugin return code, the plugin can return:
		 * 0: Client will continue handling this error (print to chat tab)
		 * 1: Client will ignore this error, the plugin announces it has handled it */
        return 1;
    }
    return 0; /* If no plugin return code was used, the return value of this function is ignored */
}

void ts3plugin_onServerStopEvent(uint64 serverConnectionHandlerID, const char* shutdownMessage) {}

int ts3plugin_onTextMessageEvent(uint64 serverConnectionHandlerID, anyID targetMode, anyID toID, anyID fromID, const char* fromName, const char* fromUniqueIdentifier, const char* message, int ffIgnored)
{
    printf("PLUGIN: onTextMessageEvent %llu %d %d %s %s %d\n", (long long unsigned int)serverConnectionHandlerID, targetMode, fromID, fromName, message, ffIgnored);

    /* Friend/Foe manager has ignored the message, so ignore here as well. */
    if (ffIgnored) {
        return 0; /* Client will ignore the message anyways, so return value here doesn't matter */
    }

#if 0
	{
		/* Example code: Autoreply to sender */
		/* Disabled because quite annoying, but should give you some ideas what is possible here */
		/* Careful, when two clients use this, they will get banned quickly... */
		anyID myID;
		if(ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
			ts3Functions.logMessage("Error querying own client id", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
			return 0;
		}
		if(fromID != myID) {  /* Don't reply when source is own client */
			if(ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Text message back!", fromID, NULL) != ERROR_ok) {
				ts3Functions.logMessage("Error requesting send text message", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
			}
		}
	}
#endif

    return 0; /* 0 = handle normally, 1 = client will ignore the text message */
}

void ts3plugin_onTalkStatusChangeEvent(uint64 serverConnectionHandlerID, int status, int isReceivedWhisper, anyID clientID)
{
    /* Demonstrate usage of getClientDisplayName */
    char name[512];

    /* Query channel path and password of current server tab.
     * The password parameter can be NULL if the plugin does not want to receive the channel password.
     * Note: Channel password is only available if the user has actually used it when entering the channel. If a user has
     * entered a channel with the permission to ignore passwords (b_channel_join_ignore_password) and the password,
     * was not entered, it will not be available.
     * getChannelConnectInfo returns 0 on success, 1 on error or if current server tab is disconnected. */
    // char path[CHANNELINFO_BUFSIZE];
    /*char password[CHANNELINFO_BUFSIZE];*/
    // char* password = NULL; /* Don't receive channel password */

    /* Get own clientID and channelID */
    // anyID  myID;
    // uint64 myChannelID;

    char msg[BIG_BUFSIZE];
    char msgShell[SHELL_BUFSIZE];
    char colorStart[PATH_BUFSIZE] = "";
    char colorStop[PATH_BUFSIZE]  = "";
    char prefix[PATH_BUFSIZE]     = "";
    if (ts3Functions.getClientDisplayName(serverConnectionHandlerID, clientID, name, 512) == ERROR_ok) {

        if (status == STATUS_TALKING) {
            printf("PLUGIN: --> %s is currently SENDING\n", name);

            if (atoi(configLhShowStart) == 1) {
                if (strlen(configLhColorStart) > 0) {
                    snprintf(colorStart, PATH_BUFSIZE, "[color=%s]", configLhColorStart);
                    snprintf(colorStop, PATH_BUFSIZE, "[/color]");
                } else {
                    snprintf(colorStart, PATH_BUFSIZE, "[color=#5472CA]");
                    snprintf(colorStop, PATH_BUFSIZE, "[/color]");
                }

                if (strlen(configLhPrefixStart) > 0)
                    snprintf(prefix, PATH_BUFSIZE, "%s -> ", configLhPrefixStart);
                else
                    prefix[0] = '\0';

                char* timeStr = GetCurrDate("%H:%M:%S");
                if (timeStr != NULL) {
                    snprintf(msg, sizeof(msg), "%s[b]<%s> *** %s%s[/b]%s", colorStart, timeStr, prefix, name, colorStop);
                    free(timeStr);
                }
                //ts3Functions.logMessage(msg, LogLevel_INFO, "Plugin lh2mqtt", serverConnectionHandlerID);
                ts3Functions.printMessage(serverConnectionHandlerID, msg, PLUGIN_MESSAGE_TARGET_CHANNEL);
            }

            if (atoi(configMqttSendStart) == 1) {

                char mqttPort[PATH_BUFSIZE]   = "";
                char mqttQos[PATH_BUFSIZE]    = "";
                char mqttCafile[PATH_BUFSIZE] = "";

                if (strlen(configMqttPort) > 0)
                    snprintf(mqttPort, sizeof(mqttPort), "-p %s", configMqttPort);

                if (strlen(configMqttQos) > 0)
                    snprintf(mqttQos, sizeof(mqttQos), "-q %s", configMqttQos);

                if (strlen(configMqttCafile) > 0)
                    snprintf(mqttCafile, sizeof(mqttCafile), "--cafile \"%s\"", configMqttCafile);

                if (strlen(configMqttUser) > 0)
                    snprintf(msgShell, sizeof(msgShell), "\"%s\" -h %s %s -u %s -P %s -t %s %s -m \"%s\" %s", configMqttExe, configMqttHost, mqttPort, configMqttUser, configMqttPassword, configMqttTopicStart, mqttQos, name, mqttCafile);
                else
                    snprintf(msgShell, sizeof(msgShell), "\"%s\" -h %s %s -t %s %s -m \"%s\" %s", configMqttExe, configMqttHost, mqttPort, configMqttTopicStart, mqttQos, name, mqttCafile);

                ExecuteCommandInBackground(msgShell, name, serverConnectionHandlerID); //modifiedString
            }

        } else {
            printf("PLUGIN: --> %s has STOPPED sending\n", name);

            if (atoi(configLhShowStop) == 1) {
                if (strlen(configLhColorStop) > 0) {
                    snprintf(colorStart, PATH_BUFSIZE, "[color=%s]", configLhColorStop);
                    snprintf(colorStop, PATH_BUFSIZE, "[/color]");
                } else {
                    snprintf(colorStart, PATH_BUFSIZE, "[color=#5472CA]");
                    snprintf(colorStop, PATH_BUFSIZE, "[/color]");
                }

                if (strlen(configLhPrefixStop) > 0)
                    snprintf(prefix, PATH_BUFSIZE, "%s -> ", configLhPrefixStop);
                else
                    prefix[0] = '\0';

                char* timeStr = GetCurrDate("%H:%M:%S");
                if (timeStr != NULL) {
                    snprintf(msg, sizeof(msg), "%s[b]<%s> *** %s%s[/b]%s", colorStart, timeStr, prefix, name, colorStop);
                    free(timeStr);
                }
                //ts3Functions.logMessage(msg, LogLevel_INFO, "Plugin lh2mqtt", serverConnectionHandlerID);
                ts3Functions.printMessage(serverConnectionHandlerID, msg, PLUGIN_MESSAGE_TARGET_CHANNEL);
            }

            if (atoi(configMqttSendStop) == 1) {

                char mqttPort[PATH_BUFSIZE]   = "";
                char mqttQos[PATH_BUFSIZE]    = "";
                char mqttCafile[PATH_BUFSIZE] = "";

                if (strlen(configMqttPort) > 0)
                    snprintf(mqttPort, sizeof(mqttPort), "-p %s", configMqttPort);

                if (strlen(configMqttQos) > 0)
                    snprintf(mqttQos, sizeof(mqttQos), "-q %s", configMqttQos);

                if (strlen(configMqttCafile) > 0)
                    snprintf(mqttCafile, sizeof(mqttCafile), "--cafile \"%s\"", configMqttCafile);

                if (strlen(configMqttUser) > 0)
                    snprintf(msgShell, sizeof(msgShell), "\"%s\" -h %s %s -u %s -P %s -t %s %s -m \"%s\" %s", configMqttExe, configMqttHost, mqttPort, configMqttUser, configMqttPassword, configMqttTopicStop, mqttQos, name, mqttCafile);
                else
                    snprintf(msgShell, sizeof(msgShell), "\"%s\" -h %s %s -t %s %s -m \"%s\" %s", configMqttExe, configMqttHost, mqttPort, configMqttTopicStop, mqttQos, name, mqttCafile);

                ExecuteCommandInBackground(msgShell, name, serverConnectionHandlerID); //modifiedString
            }
        }
    }
}

void ts3plugin_onConnectionInfoEvent(uint64 serverConnectionHandlerID, anyID clientID) {}

void ts3plugin_onServerConnectionInfoEvent(uint64 serverConnectionHandlerID) {}

void ts3plugin_onChannelSubscribeEvent(uint64 serverConnectionHandlerID, uint64 channelID) {}

void ts3plugin_onChannelSubscribeFinishedEvent(uint64 serverConnectionHandlerID) {}

void ts3plugin_onChannelUnsubscribeEvent(uint64 serverConnectionHandlerID, uint64 channelID) {}

void ts3plugin_onChannelUnsubscribeFinishedEvent(uint64 serverConnectionHandlerID) {}

void ts3plugin_onChannelDescriptionUpdateEvent(uint64 serverConnectionHandlerID, uint64 channelID) {}

void ts3plugin_onChannelPasswordChangedEvent(uint64 serverConnectionHandlerID, uint64 channelID) {}

void ts3plugin_onPlaybackShutdownCompleteEvent(uint64 serverConnectionHandlerID) {}

void ts3plugin_onSoundDeviceListChangedEvent(const char* modeID, int playOrCap) {}

void ts3plugin_onEditPlaybackVoiceDataEvent(uint64 serverConnectionHandlerID, anyID clientID, short* samples, int sampleCount, int channels) {}

void ts3plugin_onEditPostProcessVoiceDataEvent(uint64 serverConnectionHandlerID, anyID clientID, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask) {}

void ts3plugin_onEditMixedPlaybackVoiceDataEvent(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask) {}

void ts3plugin_onEditCapturedVoiceDataEvent(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, int* edited) {}

void ts3plugin_onCustom3dRolloffCalculationClientEvent(uint64 serverConnectionHandlerID, anyID clientID, float distance, float* volume) {}

void ts3plugin_onCustom3dRolloffCalculationWaveEvent(uint64 serverConnectionHandlerID, uint64 waveHandle, float distance, float* volume) {}

void ts3plugin_onUserLoggingMessageEvent(const char* logMessage, int logLevel, const char* logChannel, uint64 logID, const char* logTime, const char* completeLogString) {}

/* Clientlib rare */

void ts3plugin_onClientBanFromServerEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, uint64 time,
                                          const char* kickMessage)
{
}

int ts3plugin_onClientPokeEvent(uint64 serverConnectionHandlerID, anyID fromClientID, const char* pokerName, const char* pokerUniqueIdentity, const char* message, int ffIgnored)
{
    anyID myID;

    printf("PLUGIN onClientPokeEvent: %llu %d %s %s %d\n", (long long unsigned int)serverConnectionHandlerID, fromClientID, pokerName, message, ffIgnored);

    /* Check if the Friend/Foe manager has already blocked this poke */
    if (ffIgnored) {
        return 0; /* Client will block anyways, doesn't matter what we return */
    }

    /* Example code: Send text message back to poking client */
    if (ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) { /* Get own client ID */
        ts3Functions.logMessage("Error querying own client id", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
        return 0;
    }
    if (fromClientID != myID) { /* Don't reply when source is own client */
        if (ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Received your poke!", fromClientID, NULL) != ERROR_ok) {
            ts3Functions.logMessage("Error requesting send text message", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
        }
    }

    return 0; /* 0 = handle normally, 1 = client will ignore the poke */
}

void ts3plugin_onClientSelfVariableUpdateEvent(uint64 serverConnectionHandlerID, int flag, const char* oldValue, const char* newValue) {}

void ts3plugin_onFileListEvent(uint64 serverConnectionHandlerID, uint64 channelID, const char* path, const char* name, uint64 size, uint64 datetime, int type, uint64 incompletesize, const char* returnCode) {}

void ts3plugin_onFileListFinishedEvent(uint64 serverConnectionHandlerID, uint64 channelID, const char* path) {}

void ts3plugin_onFileInfoEvent(uint64 serverConnectionHandlerID, uint64 channelID, const char* name, uint64 size, uint64 datetime) {}

void ts3plugin_onServerGroupListEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID, const char* name, int type, int iconID, int saveDB) {}

void ts3plugin_onServerGroupListFinishedEvent(uint64 serverConnectionHandlerID) {}

void ts3plugin_onServerGroupByClientIDEvent(uint64 serverConnectionHandlerID, const char* name, uint64 serverGroupList, uint64 clientDatabaseID) {}

void ts3plugin_onServerGroupPermListEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {}

void ts3plugin_onServerGroupPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID) {}

void ts3plugin_onServerGroupClientListEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID, uint64 clientDatabaseID, const char* clientNameIdentifier, const char* clientUniqueID) {}

void ts3plugin_onChannelGroupListEvent(uint64 serverConnectionHandlerID, uint64 channelGroupID, const char* name, int type, int iconID, int saveDB) {}

void ts3plugin_onChannelGroupListFinishedEvent(uint64 serverConnectionHandlerID) {}

void ts3plugin_onChannelGroupPermListEvent(uint64 serverConnectionHandlerID, uint64 channelGroupID, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {}

void ts3plugin_onChannelGroupPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 channelGroupID) {}

void ts3plugin_onChannelPermListEvent(uint64 serverConnectionHandlerID, uint64 channelID, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {}

void ts3plugin_onChannelPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 channelID) {}

void ts3plugin_onClientPermListEvent(uint64 serverConnectionHandlerID, uint64 clientDatabaseID, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {}

void ts3plugin_onClientPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 clientDatabaseID) {}

void ts3plugin_onChannelClientPermListEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 clientDatabaseID, unsigned int permissionID, int permissionValue, int permissionNegated, int permissionSkip) {}

void ts3plugin_onChannelClientPermListFinishedEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 clientDatabaseID) {}

void ts3plugin_onClientChannelGroupChangedEvent(uint64 serverConnectionHandlerID, uint64 channelGroupID, uint64 channelID, anyID clientID, anyID invokerClientID, const char* invokerName, const char* invokerUniqueIdentity) {}

int ts3plugin_onServerPermissionErrorEvent(uint64 serverConnectionHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, unsigned int failedPermissionID)
{
    return 0; /* See onServerErrorEvent for return code description */
}

void ts3plugin_onPermissionListGroupEndIDEvent(uint64 serverConnectionHandlerID, unsigned int groupEndID) {}

void ts3plugin_onPermissionListEvent(uint64 serverConnectionHandlerID, unsigned int permissionID, const char* permissionName, const char* permissionDescription) {}

void ts3plugin_onPermissionListFinishedEvent(uint64 serverConnectionHandlerID) {}

void ts3plugin_onPermissionOverviewEvent(uint64 serverConnectionHandlerID, uint64 clientDatabaseID, uint64 channelID, int overviewType, uint64 overviewID1, uint64 overviewID2, unsigned int permissionID, int permissionValue, int permissionNegated,
                                         int permissionSkip)
{
}

void ts3plugin_onPermissionOverviewFinishedEvent(uint64 serverConnectionHandlerID) {}

void ts3plugin_onServerGroupClientAddedEvent(uint64 serverConnectionHandlerID, anyID clientID, const char* clientName, const char* clientUniqueIdentity, uint64 serverGroupID, anyID invokerClientID, const char* invokerName,
                                             const char* invokerUniqueIdentity)
{
}

void ts3plugin_onServerGroupClientDeletedEvent(uint64 serverConnectionHandlerID, anyID clientID, const char* clientName, const char* clientUniqueIdentity, uint64 serverGroupID, anyID invokerClientID, const char* invokerName,
                                               const char* invokerUniqueIdentity)
{
}

void ts3plugin_onClientNeededPermissionsEvent(uint64 serverConnectionHandlerID, unsigned int permissionID, int permissionValue) {}

void ts3plugin_onClientNeededPermissionsFinishedEvent(uint64 serverConnectionHandlerID) {}

void ts3plugin_onFileTransferStatusEvent(anyID transferID, unsigned int status, const char* statusMessage, uint64 remotefileSize, uint64 serverConnectionHandlerID) {}

void ts3plugin_onClientChatClosedEvent(uint64 serverConnectionHandlerID, anyID clientID, const char* clientUniqueIdentity) {}

void ts3plugin_onClientChatComposingEvent(uint64 serverConnectionHandlerID, anyID clientID, const char* clientUniqueIdentity) {}

void ts3plugin_onServerLogEvent(uint64 serverConnectionHandlerID, const char* logMsg) {}

void ts3plugin_onServerLogFinishedEvent(uint64 serverConnectionHandlerID, uint64 lastPos, uint64 fileSize) {}

void ts3plugin_onMessageListEvent(uint64 serverConnectionHandlerID, uint64 messageID, const char* fromClientUniqueIdentity, const char* subject, uint64 timestamp, int flagRead) {}

void ts3plugin_onMessageGetEvent(uint64 serverConnectionHandlerID, uint64 messageID, const char* fromClientUniqueIdentity, const char* subject, const char* message, uint64 timestamp) {}

void ts3plugin_onClientDBIDfromUIDEvent(uint64 serverConnectionHandlerID, const char* uniqueClientIdentifier, uint64 clientDatabaseID) {}

void ts3plugin_onClientNamefromUIDEvent(uint64 serverConnectionHandlerID, const char* uniqueClientIdentifier, uint64 clientDatabaseID, const char* clientNickName) {}

void ts3plugin_onClientNamefromDBIDEvent(uint64 serverConnectionHandlerID, const char* uniqueClientIdentifier, uint64 clientDatabaseID, const char* clientNickName) {}

void ts3plugin_onComplainListEvent(uint64 serverConnectionHandlerID, uint64 targetClientDatabaseID, const char* targetClientNickName, uint64 fromClientDatabaseID, const char* fromClientNickName, const char* complainReason, uint64 timestamp) {}

void ts3plugin_onBanListEvent(uint64 serverConnectionHandlerID, uint64 banid, const char* ip, const char* name, const char* uid, const char* mytsid, uint64 creationTime, uint64 durationTime, const char* invokerName, uint64 invokercldbid,
                              const char* invokeruid, const char* reason, int numberOfEnforcements, const char* lastNickName)
{
}

void ts3plugin_onClientServerQueryLoginPasswordEvent(uint64 serverConnectionHandlerID, const char* loginPassword) {}

void ts3plugin_onPluginCommandEvent(uint64 serverConnectionHandlerID, const char* pluginName, const char* pluginCommand, anyID invokerClientID, const char* invokerName, const char* invokerUniqueIdentity)
{
    printf("ON PLUGIN COMMAND: %s %s %d %s %s\n", pluginName, pluginCommand, invokerClientID, invokerName, invokerUniqueIdentity);
}

void ts3plugin_onIncomingClientQueryEvent(uint64 serverConnectionHandlerID, const char* commandText) {}

void ts3plugin_onServerTemporaryPasswordListEvent(uint64 serverConnectionHandlerID, const char* clientNickname, const char* uniqueClientIdentifier, const char* description, const char* password, uint64 timestampStart, uint64 timestampEnd,
                                                  uint64 targetChannelID, const char* targetChannelPW)
{
}

/* Client UI callbacks */

/*
 * Called from client when an avatar image has been downloaded to or deleted from cache.
 * This callback can be called spontaneously or in response to ts3Functions.getAvatar()
 */
void ts3plugin_onAvatarUpdated(uint64 serverConnectionHandlerID, anyID clientID, const char* avatarPath)
{
    /* If avatarPath is NULL, the avatar got deleted */
    /* If not NULL, avatarPath contains the path to the avatar file in the TS3Client cache */
    if (avatarPath != NULL) {
        printf("onAvatarUpdated: %llu %d %s\n", (long long unsigned int)serverConnectionHandlerID, clientID, avatarPath);
    } else {
        printf("onAvatarUpdated: %llu %d - deleted\n", (long long unsigned int)serverConnectionHandlerID, clientID);
    }
}

/*
 * Called when a plugin menu item (see ts3plugin_initMenus) is triggered. Optional function, when not using plugin menus, do not implement this.
 *
 * Parameters:
 * - serverConnectionHandlerID: ID of the current server tab
 * - type: Type of the menu (PLUGIN_MENU_TYPE_CHANNEL, PLUGIN_MENU_TYPE_CLIENT or PLUGIN_MENU_TYPE_GLOBAL)
 * - menuItemID: Id used when creating the menu item
 * - selectedItemID: Channel or Client ID in the case of PLUGIN_MENU_TYPE_CHANNEL and PLUGIN_MENU_TYPE_CLIENT. 0 for PLUGIN_MENU_TYPE_GLOBAL.
 */
void ts3plugin_onMenuItemEvent(uint64 serverConnectionHandlerID, enum PluginMenuType type, int menuItemID, uint64 selectedItemID)
{
    printf("PLUGIN: onMenuItemEvent: serverConnectionHandlerID=%llu, type=%d, menuItemID=%d, selectedItemID=%llu\n", (long long unsigned int)serverConnectionHandlerID, type, menuItemID, (long long unsigned int)selectedItemID);

    char pluginPath[PATH_BUFSIZE];
    ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE, pluginID);

    switch (type) {
        case PLUGIN_MENU_TYPE_GLOBAL:
            /* Global menu item was triggered. selectedItemID is unused and set to zero. */
            switch (menuItemID) {
                case MENU_ID_GLOBAL_1:
                    /* Menu global 1 was triggered */
                    //MessageBox(NULL, L"Hello from plugin", L"lh2mqtt",0);

                    char command[BIG_BUFSIZE];
                    #ifdef _WIN32
                        snprintf(command, sizeof(command), "notepad.exe \"%slh2mqtt.ini\"", pluginPath);
                    #else
                        snprintf(command, sizeof(command), "xdg-open \"%slh2mqtt.ini\"", pluginPath);
                    #endif
                    ExecuteCommandInBackground(command, "", serverConnectionHandlerID);

                    #ifndef _WIN32
                        if (strcmp(configGeneralLanguage, "DE") == 0)
                            ts3Functions.printMessageToCurrentTab("[b]Bitte nach dem Speichern der Änderungen die Konfiguration via Plugins-Menü neu laden[/b]");
                        else
                            ts3Functions.printMessageToCurrentTab("[b]Please reload configuration via plugins menu after saving changes to INI file[/b]");
                        
                        break;
                    #endif

                    case MENU_ID_GLOBAL_2:
                    ts3plugin_init();
                    break;

                case MENU_ID_GLOBAL_3:
                    /* Menu global 2 was triggered */
                    char  content[1500];
                    char  year[20];
                    char* currentYear = GetCurrDate("%Y");

                    if (currentYear != NULL) {
                        if (strcmp(currentYear, "2023") == 0)
                            snprintf(year, sizeof(year), "%s", currentYear);
                        else
                            snprintf(year, sizeof(year), "2023 - %s", currentYear);

                        #ifdef _WIN32
                            const char* platform = "Windows";
                        #else
                            const char* platform = "Linux";
                        #endif

                        if (strcmp(configGeneralLanguage, "DE") == 0)
                            snprintf(content, sizeof(content), "%s - TeamSpeak 3 %s Plugin\n\n%s\n\nAutor: \t\t%s\nPlugin Version: \t%s\nTS3 API Version: \t%d\nCopyright: \t%s\nLizenz: \t\tLGPL\n\nQuellcode:\nhttps://github.com/Little-Ben/ts3client-pluginsdk-lh2mqtt\n\nKonfigurationsdatei:\n%s", ts3plugin_name(), platform, ts3plugin_description(),
                                 ts3plugin_author(), ts3plugin_version(), ts3plugin_apiVersion(), year, configIniFileName);
                        else
                            snprintf(content, sizeof(content), "%s - TeamSpeak 3 %s plugin\n\n%s\n\nAuthor: \t\t%s\nPlugin version: \t%s\nTS3 API version: \t%d\nCopyright: \t%s\nLicense: \t\tLGPL\n\nSource code:\nhttps://github.com/Little-Ben/ts3client-pluginsdk-lh2mqtt\n\nConfig file:\n%s", ts3plugin_name(), platform, ts3plugin_description(),
                                 ts3plugin_author(), ts3plugin_version(), ts3plugin_apiVersion(), year, configIniFileName);

                        #ifdef _WIN32
                            LPWSTR wideStr = ConvertToUnicode(content);
                            MessageBox(NULL, wideStr, L"Plugin lh2mqtt", 0);
                            FreeWideString(wideStr);
                        #else
                            // MsgBox gibt es nicht unter Linux, daher in den aktiven Tab ausgeben
                            ts3Functions.printMessageToCurrentTab(content);
                        #endif

                        free(currentYear); // release memory
                    }
                    break;
                default:
                    break;
            }
            break;
        case PLUGIN_MENU_TYPE_CHANNEL:
            /* Channel contextmenu item was triggered. selectedItemID is the channelID of the selected channel */
            switch (menuItemID) {
                case MENU_ID_CHANNEL_1:
                    /* Menu channel 1 was triggered */
                    break;
                case MENU_ID_CHANNEL_2:
                    /* Menu channel 2 was triggered */
                    break;
                case MENU_ID_CHANNEL_3:
                    /* Menu channel 3 was triggered */
                    break;
                default:
                    break;
            }
            break;
        case PLUGIN_MENU_TYPE_CLIENT:
            /* Client contextmenu item was triggered. selectedItemID is the clientID of the selected client */
            switch (menuItemID) {
                case MENU_ID_CLIENT_1:
                    /* Menu client 1 was triggered */
                    break;
                case MENU_ID_CLIENT_2:
                    /* Menu client 2 was triggered */
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

/* This function is called if a plugin hotkey was pressed. Omit if hotkeys are unused. */
void ts3plugin_onHotkeyEvent(const char* keyword)
{
    printf("PLUGIN: Hotkey event: %s\n", keyword);
    /* Identify the hotkey by keyword ("keyword_1", "keyword_2" or "keyword_3" in this example) and handle here... */
}

/* Called when recording a hotkey has finished after calling ts3Functions.requestHotkeyInputDialog */
void ts3plugin_onHotkeyRecordedEvent(const char* keyword, const char* key) {}

// This function receives your key Identifier you send to notifyKeyEvent and should return
// the friendly device name of the device this hotkey originates from. Used for display in UI.
const char* ts3plugin_keyDeviceName(const char* keyIdentifier)
{
    return NULL;
}

// This function translates the given key identifier to a friendly key name for display in the UI
const char* ts3plugin_displayKeyText(const char* keyIdentifier)
{
    return NULL;
}

// This is used internally as a prefix for hotkeys so we can store them without collisions.
// Should be unique across plugins.
const char* ts3plugin_keyPrefix()
{
    return NULL;
}

/* Called when client custom nickname changed */
void ts3plugin_onClientDisplayNameChanged(uint64 serverConnectionHandlerID, anyID clientID, const char* displayName, const char* uniqueClientIdentifier) {}

// Execute the command in a shell/terminal in background
void ExecuteCommandInBackground(const char* command, const char* name, uint64 serverConnectionHandlerID)
{
#ifdef _WIN32
    STARTUPINFO         si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    LPWSTR wideStr = ConvertToUnicode(command);

    // execute command in background
    if (CreateProcess(NULL, wideStr, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        //ts3Functions.logMessage(command, LogLevel_INFO, "Plugin lh2mqtt", serverConnectionHandlerID);
        char msg[CHANNELINFO_BUFSIZE];
        if (strlen(name) > 0) {
            if (atoi(configLogMqttMsg) == 1)
            {
                snprintf(msg, sizeof(msg), "[MQTT] Topic=%s, Msg=%s", configMqttTopicStart, name);
                ts3Functions.logMessage(msg, LogLevel_INFO, "Plugin lh2mqtt", serverConnectionHandlerID);
                printf("PLUGIN: LOG MQTT MSG: %s\n",msg);
            }
            else
            {
                printf("PLUGIN: NO LOG MQTT MSG: %s\n", name);
            }
        }
    } else {
        DWORD error = GetLastError();
        char  errorMsg[1024];
        snprintf(errorMsg, sizeof(errorMsg), "Fehler(%d) beim Ausfuehren des Befehls: %s", error, command);
        ts3Functions.logMessage(errorMsg, LogLevel_ERROR, "Plugin lh2mqtt", serverConnectionHandlerID);
        printf("PLUGIN: ERROR(%d): could not run command: %s\n", error, command);
    }
    FreeWideString(wideStr);
#else
    printf("PLUGIN EXEC: %s\n", command);
    int ret = system(command);
    if (ret != 0) {
        printf("PLUGIN EXEC: ERROR: shell command execution failed, code: %d\n",ret);
    }
    
    char msg[CHANNELINFO_BUFSIZE];
    if (strlen(name) > 0) {
        if (atoi(configLogMqttMsg) == 1)
        {
            snprintf(msg, sizeof(msg), "[MQTT] Topic=%s, Msg=%s", configMqttTopicStart, name);
            ts3Functions.logMessage(msg, LogLevel_INFO, "Plugin lh2mqtt", serverConnectionHandlerID);
            printf("PLUGIN: LOG MQTT MSG: %s\n",msg);
        }
        else
        {
            printf("PLUGIN: NO LOG MQTT MSG: %s\n", name);
        }
    }
#endif
}

// Reads a value out of an INI file, bHideLog suppresses output to TS3 console
void ReadIniValue(const char* iniFileName, const char* sectionName, const char* keyName, char* returnValue, size_t bufferSize, BOOL bHideLog)
{
    // read value from INI file
    unsigned int bytesRead = GetPrivateProfileStringAWrapper(sectionName, keyName, "", returnValue, bufferSize, iniFileName);
    
    if (bytesRead == 0) {
        // set value to an empty string, if it was not found
        returnValue[0] = '\0';
    }
    if (bHideLog == 1)
        printf("PLUGIN: ReadIniValue: [%s]%s=%s\n", sectionName, keyName, "***");
    else
        printf("PLUGIN: ReadIniValue: [%s]%s=%s\n", sectionName, keyName, returnValue);
}

// Writes a value to an INI file
BOOL WriteIniValue(const char* iniFileName, const char* sectionName, const char* keyName, const char* value)
{
    // Write value to the INI file
    BOOL result = WritePrivateProfileStringAWrapper(sectionName, keyName, value, iniFileName);

    if (!result) {
        // Handle the error if the write operation fails
        // You can add error handling code here, such as logging the error.
        // For now, we'll just print an error message.
        printf("PLUGIN: ERROR writing to config file\n");
    }
    else
        printf("PLUGIN: WriteIniValue: [%s]%s=%s\n", sectionName, keyName, value);

    return result;
}

#ifdef _WIN32
// Converts a string (char*) to a wide string (unicode).
// Caller needs to free returning string's memory if not used anymore.
LPWSTR ConvertToUnicode(const char* str)
{
    int    len     = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    LPWSTR wideStr = (LPWSTR)malloc((len + 1) * sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, str, -1, wideStr, len);
    return wideStr;
}
#endif

// freeing wide string's memory
void FreeWideString(LPWSTR str)
{
    if (str != NULL) {
        free(str);
    }
}

// Creating a template INI file with default values, if file does not exist
void CreateDefaultIniFile(const char* fileName)
{
    FILE* datei = NULL;
    char* random_hex = GetRandomHex(7);
    char topicStart[100];
    char topicStop[100];

    
    datei = fopen(fileName, "r");
    if (!datei) {
        // file does not exist, so create it and fill it

        datei = fopen(fileName, "w");
        if (datei) {
            #ifdef _WIN32
                fprintf(datei, "; lh2mqtt - LastHeard To Mqtt - TeamSpeak 3 Plugin (Windows)\n");
            #else
                fprintf(datei, "; lh2mqtt - LastHeard To Mqtt - TeamSpeak 3 Plugin (Linux)\n");
            #endif
            fprintf(datei, ";-------------------------------------------------------------------------------\n");
            fprintf(datei, "; Dieses Plugin ermoeglicht die Anzeige des zuletzt aktiven Sprechers \n");
            fprintf(datei, ";   1) via MQTT (INI-Bereich: MQTT)\n");
            fprintf(datei, ";   2) im Channel-Tab in TeamSpeak 3 (INI-Bereich: CHANNELTAB)\n");
            fprintf(datei, ";\n");
            fprintf(datei, ";-------------------------------------------------------------------------------\n");
            fprintf(datei, "; MQTT:\n");
            fprintf(datei, ";-------------------------------------------------------------------------------\n");
            fprintf(datei, "; Fuer MQTT wird eine Installation von Mosquitto benoetigt, \n");
            fprintf(datei, "; siehe: https://mosquitto.org/download/\n");
            fprintf(datei, ";\n");
            #ifdef _WIN32
                fprintf(datei, "; PATH beinhaltet den kompletten Pfad zur mosquitto_pub.exe (inkl. EXE)\n");
            #else
                fprintf(datei, "; PATH beinhaltet den kompletten Pfad zur mosquitto_pub Binary\n");
                fprintf(datei, ";   siehe: whereis mosquitto_pub\n");
            #endif
            fprintf(datei, "; HOST kann eine IP-Adresse oder ein Hostname (FQDN) sein\n");
            fprintf(datei, ";   also 127.0.0.1 oder meine-broker-fqdn\n");
            fprintf(datei, "; PORT des Brokers (anzugeben, falls abweichend von 1883)\n");
            fprintf(datei, "; USER/PASSWORD/QOS sind optional\n");
            fprintf(datei, "; CAFILE: kompletter Pfad und Dateiname des Server-CA-Bundles (SSL)\n");
            fprintf(datei, "; SEND_START/SEND_STOP: 1 gibt an, dass die Info via MQTT gesendet wird\n");
            fprintf(datei, "; TOPIC_START/TOPIC_STOP: Topic auf dem die Info veroeffentlicht wird\n");
            fprintf(datei, ";\n");
            fprintf(datei, ";-------------------------------------------------------------------------------\n");
            fprintf(datei, "; CHANNELTAB:\n");
            fprintf(datei, ";-------------------------------------------------------------------------------\n");
            fprintf(datei, "; Farbwerte sind RGB-HEX-Codes (z.B.: #FF00FF) \n");
            fprintf(datei, "; oder manche, einfache Farbnamen auch direkt (z.B.: green, red, yellow)\n");
            fprintf(datei, "; siehe: https://www.farb-tabelle.de/de/farbtabelle.htm\n");
            fprintf(datei, ";\n");
            fprintf(datei, "; SHOW_START/SHOW_STOP: 1 gibt an, dass Info im Channel-Tab ausgegeben wird\n");
            fprintf(datei, "; COLOR_START/COLOR_STOP: RGB-Farbwert (bei HEX-Code mit # angeben!)\n");
            fprintf(datei, ";\n");
            fprintf(datei, ";-------------------------------------------------------------------------------\n");
            fprintf(datei, "; LOGGING:\n");
            fprintf(datei, ";-------------------------------------------------------------------------------\n");
            fprintf(datei, "; LOG_MQTT_MSG: 1 gibt an, dass die gesendete MQTT-Message mitgeloggt wird\n");
            fprintf(datei, ";\n");
            fprintf(datei, ";-------------------------------------------------------------------------------\n");
            fprintf(datei, "; Diese Config wird automatisch beim Programmstart von TeamSpeak 3\n");
            fprintf(datei, "; oder nach 'Plugins|lh2mqtt|Konfiguration editieren' neu eingelesen!\n");
            fprintf(datei, "; Sollte keine Config existieren, wird ein Standardinhalt als Vorlage erzeugt.\n");
            fprintf(datei, ";-------------------------------------------------------------------------------\n");
            fprintf(datei, "; Autor: Little.Ben, DH6BS\n");
            fprintf(datei, "; Quellcode siehe: https://github.com/Little-Ben/ts3client-pluginsdk-lh2mqtt\n");
            fprintf(datei, "; Lizenz: LGPL\n");
            fprintf(datei, ";-------------------------------------------------------------------------------\n");
            fprintf(datei, "\n");

            fprintf(datei, "[MQTT]\n");
            #ifdef _WIN32
                fprintf(datei, "PATH=C:\\Programme\\mosquitto\\mosquitto_pub.exe\n");
            #else
                fprintf(datei, "PATH=/usr/bin/mosquitto_pub\n");
            #endif
            fprintf(datei, "HOST=test.mosquitto.org\n");
            fprintf(datei, "PORT=\n");
            fprintf(datei, "USER=\n");
            fprintf(datei, "PASSWORD=\n");
            fprintf(datei, "QOS=0\n");
            fprintf(datei, "CAFILE=\n");
            fprintf(datei, "SEND_START=0\n");
            fprintf(datei, "SEND_STOP=0\n");
            if (random_hex != NULL)
            {
                snprintf(topicStart, sizeof(topicStart), "TOPIC_START=lh2mqtt/%s/start\n", random_hex);
                fprintf(datei, "%s", topicStart);
                snprintf(topicStop, sizeof(topicStop), "TOPIC_STOP=lh2mqtt/%s/stop\n", random_hex);
                fprintf(datei, "%s", topicStop);
                free(random_hex);
            }
            fprintf(datei, "\n");

            fprintf(datei, "[CHANNELTAB]\n");
            fprintf(datei, "SHOW_START=1\n");
            fprintf(datei, "SHOW_STOP=1\n");
            fprintf(datei, "COLOR_START=red\n");
            fprintf(datei, "COLOR_STOP=#008000\n");
            fprintf(datei, "PREFIX_START=Start talking\n");
            fprintf(datei, "PREFIX_STOP=Stop talking\n");
            fprintf(datei, "\n");

            fprintf(datei, "[LOGGING]\n");
            fprintf(datei, "LOG_MQTT_MSG=1\n");
            fprintf(datei, "\n");

            fprintf(datei, "[GENERAL]\n");
            fprintf(datei, "LANGUAGE=DE\n");
            fprintf(datei, "\n");

            fclose(datei);
            printf("PLUGIN: lh2mqtt config file was created and filled with template values.\n");
        } else {
            printf("PLUGIN: ERROR: lh2mqtt config file could not be created %s\n", fileName);
        }
    } else {
        // file already exists, close it and return
        printf("PLUGIN: lh2mqtt config file already exists.\n");
        fclose(datei);
    }
}

// Get's the current date/time according to format parameter
// Caller needs to free returning string's memory
char* GetCurrDate(const char* format)
{
    time_t    currentTime;
    struct tm timeInfo;
    char*     dateStr = NULL;

    // get current time
    time(&currentTime);

    // get local time
    if (LocalTimeSafe(&currentTime, &timeInfo) != 0) {
        // Error within localtime_s
        return NULL;
    }

    // get date/time according to format parameter in a char*
    dateStr = (char*)malloc(64); // plenty of buffer size
    if (dateStr != NULL) {
        if (strftime(dateStr, 64, format, &timeInfo) == 0) {
            // error in strftime
            free(dateStr);
            return NULL;
        }
    }

    return dateStr;
}

// Get a rondom hex number with given length
// Caller needs to free returning string's memory
char* GetRandomHex(int length) {
    if (length <= 0) {
        return NULL;
    }

    char hex_chars[] = "0123456789ABCDEF";
    char* hex_string = (char*)malloc((length + 1) * sizeof(char));

    if (hex_string == NULL) {
        return NULL; // Error
    }

    srand((unsigned int)time(NULL)); // init random number generator with current time

    for (int i = 0; i < length; i++) {
        int random_index = rand() % 16; // choose a random index postion for current hex position
        hex_string[i] = hex_chars[random_index];
    }

    hex_string[length] = '\0'; // null terminating the string

    return hex_string;
}