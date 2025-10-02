#include "ini_wrapper.h"

#ifdef _WIN32
// -------------------- Windows --------------------
#include <windows.h>

void ReadCompleteIniFile(const char configIniFileName[512]) {
    (void)configIniFileName; // Windows benötigt keine Struktur
}

LH2MQTT_INI* GetIniStruct(void) {
    return NULL; // Windows nutzt direkt die Windows-API
}

unsigned int GetPrivateProfileStringA(
    const char* lpAppName,
    const char* lpKeyName,
    const char* lpDefault,
    char* lpReturnedString,
    unsigned int nSize,
    const char* lpFileName
) {
    return ::GetPrivateProfileStringA(lpAppName, lpKeyName, lpDefault, lpReturnedString, nSize, lpFileName);
}

int WritePrivateProfileStringA(
    const char* lpAppName,
    const char* lpKeyName,
    const char* lpString,
    const char* lpFileName
) {
    return ::WritePrivateProfileStringA(lpAppName, lpKeyName, lpString, lpFileName);
}

#else
// -------------------- Linux / Unix --------------------
#include "ini.h"
#include <string.h>
#include <stdlib.h>

static LH2MQTT_INI iniData;
static char currentIniFile[512];

// Handler für ini_parse um Struktur zu füllen
static int ini_wrapper_handler(void* user, const char* section, const char* name, const char* value) {
    LH2MQTT_INI* cfg = (LH2MQTT_INI*)user;

    if (strcmp(section, "MQTT") == 0) {
        if (strcmp(name, "PATH") == 0) strncpy(cfg->mqtt.PATH, value, sizeof(cfg->mqtt.PATH));
        else if (strcmp(name, "HOST") == 0) strncpy(cfg->mqtt.HOST, value, sizeof(cfg->mqtt.HOST));
        else if (strcmp(name, "PORT") == 0) strncpy(cfg->mqtt.PORT, value, sizeof(cfg->mqtt.PORT));
        else if (strcmp(name, "USER") == 0) strncpy(cfg->mqtt.USER, value, sizeof(cfg->mqtt.USER));
        else if (strcmp(name, "PASSWORD") == 0) strncpy(cfg->mqtt.PASSWORD, value, sizeof(cfg->mqtt.PASSWORD));
        else if (strcmp(name, "QOS") == 0) strncpy(cfg->mqtt.QOS, value, sizeof(cfg->mqtt.QOS));
        else if (strcmp(name, "CAFILE") == 0) strncpy(cfg->mqtt.CAFILE, value, sizeof(cfg->mqtt.CAFILE));
        else if (strcmp(name, "SEND_START") == 0) strncpy(cfg->mqtt.SEND_START, value, sizeof(cfg->mqtt.SEND_START));
        else if (strcmp(name, "SEND_STOP") == 0) strncpy(cfg->mqtt.SEND_STOP, value, sizeof(cfg->mqtt.SEND_STOP));
        else if (strcmp(name, "TOPIC_START") == 0) strncpy(cfg->mqtt.TOPIC_START, value, sizeof(cfg->mqtt.TOPIC_START));
        else if (strcmp(name, "TOPIC_STOP") == 0) strncpy(cfg->mqtt.TOPIC_STOP, value, sizeof(cfg->mqtt.TOPIC_STOP));
    } else if (strcmp(section, "CHANNELTAB") == 0) {
        if (strcmp(name, "SHOW_START") == 0) strncpy(cfg->channelTab.SHOW_START, value, sizeof(cfg->channelTab.SHOW_START));
        else if (strcmp(name, "SHOW_STOP") == 0) strncpy(cfg->channelTab.SHOW_STOP, value, sizeof(cfg->channelTab.SHOW_STOP));
        else if (strcmp(name, "COLOR_START") == 0) strncpy(cfg->channelTab.COLOR_START, value, sizeof(cfg->channelTab.COLOR_START));
        else if (strcmp(name, "COLOR_STOP") == 0) strncpy(cfg->channelTab.COLOR_STOP, value, sizeof(cfg->channelTab.COLOR_STOP));
        else if (strcmp(name, "PREFIX_START") == 0) strncpy(cfg->channelTab.PREFIX_START, value, sizeof(cfg->channelTab.PREFIX_START));
        else if (strcmp(name, "PREFIX_STOP") == 0) strncpy(cfg->channelTab.PREFIX_STOP, value, sizeof(cfg->channelTab.PREFIX_STOP));
    } else if (strcmp(section, "LOGGING") == 0) {
        if (strcmp(name, "LOG_MQTT_MSG") == 0) strncpy(cfg->logging.LOG_MQTT_MSG, value, sizeof(cfg->logging.LOG_MQTT_MSG));
    } else if (strcmp(section, "GENERAL") == 0) {
        if (strcmp(name, "LANGUAGE") == 0) strncpy(cfg->general.LANGUAGE, value, sizeof(cfg->general.LANGUAGE));
    }

    // Null-terminieren
    cfg->mqtt.PATH[sizeof(cfg->mqtt.PATH)-1] = '\0';
    cfg->mqtt.HOST[sizeof(cfg->mqtt.HOST)-1] = '\0';
    cfg->mqtt.PORT[sizeof(cfg->mqtt.PORT)-1] = '\0';
    cfg->mqtt.USER[sizeof(cfg->mqtt.USER)-1] = '\0';
    cfg->mqtt.PASSWORD[sizeof(cfg->mqtt.PASSWORD)-1] = '\0';
    cfg->mqtt.QOS[sizeof(cfg->mqtt.QOS)-1] = '\0';
    cfg->mqtt.CAFILE[sizeof(cfg->mqtt.CAFILE)-1] = '\0';
    cfg->mqtt.SEND_START[sizeof(cfg->mqtt.SEND_START)-1] = '\0';
    cfg->mqtt.SEND_STOP[sizeof(cfg->mqtt.SEND_STOP)-1] = '\0';
    cfg->mqtt.TOPIC_START[sizeof(cfg->mqtt.TOPIC_START)-1] = '\0';
    cfg->mqtt.TOPIC_STOP[sizeof(cfg->mqtt.TOPIC_STOP)-1] = '\0';

    cfg->channelTab.SHOW_START[sizeof(cfg->channelTab.SHOW_START)-1] = '\0';
    cfg->channelTab.SHOW_STOP[sizeof(cfg->channelTab.SHOW_STOP)-1] = '\0';
    cfg->channelTab.COLOR_START[sizeof(cfg->channelTab.COLOR_START)-1] = '\0';
    cfg->channelTab.COLOR_STOP[sizeof(cfg->channelTab.COLOR_STOP)-1] = '\0';
    cfg->channelTab.PREFIX_START[sizeof(cfg->channelTab.PREFIX_START)-1] = '\0';
    cfg->channelTab.PREFIX_STOP[sizeof(cfg->channelTab.PREFIX_STOP)-1] = '\0';

    cfg->logging.LOG_MQTT_MSG[sizeof(cfg->logging.LOG_MQTT_MSG)-1] = '\0';

    cfg->general.LANGUAGE[sizeof(cfg->general.LANGUAGE)-1] = '\0';

    return 1;
}

void ReadCompleteIniFile(const char configIniFileName[512]) {
    printf("PLUGIN::INI_WRAPPER: ReadCompleteIniFile\n");
    strncpy(currentIniFile, configIniFileName, sizeof(currentIniFile));
    currentIniFile[sizeof(currentIniFile)-1] = '\0';
    memset(&iniData, 0, sizeof(iniData));
    ini_parse(currentIniFile, ini_wrapper_handler, &iniData);
}

LH2MQTT_INI* GetIniStruct(void) {
    return &iniData;
}

unsigned int GetPrivateProfileStringA(
    const char* lpAppName,
    const char* lpKeyName,
    const char* lpDefault,
    char* lpReturnedString,
    unsigned int nSize,
    const char* lpFileName
) {
    (void)lpFileName; // Datei wird nicht mehr direkt gelesen
    if (!lpReturnedString || nSize == 0) return 0;
    lpReturnedString[0] = '\0';

    LH2MQTT_INI* cfg = &iniData;

    if (strcmp(lpAppName, "MQTT") == 0) {
        if (strcmp(lpKeyName, "PATH") == 0) strncpy(lpReturnedString, cfg->mqtt.PATH, nSize);
        else if (strcmp(lpKeyName, "HOST") == 0) strncpy(lpReturnedString, cfg->mqtt.HOST, nSize);
        else if (strcmp(lpKeyName, "PORT") == 0) strncpy(lpReturnedString, cfg->mqtt.PORT, nSize);
        else if (strcmp(lpKeyName, "USER") == 0) strncpy(lpReturnedString, cfg->mqtt.USER, nSize);
        else if (strcmp(lpKeyName, "PASSWORD") == 0) strncpy(lpReturnedString, cfg->mqtt.PASSWORD, nSize);
        else if (strcmp(lpKeyName, "QOS") == 0) strncpy(lpReturnedString, cfg->mqtt.QOS, nSize);
        else if (strcmp(lpKeyName, "CAFILE") == 0) strncpy(lpReturnedString, cfg->mqtt.CAFILE, nSize);
        else if (strcmp(lpKeyName, "SEND_START") == 0) strncpy(lpReturnedString, cfg->mqtt.SEND_START, nSize);
        else if (strcmp(lpKeyName, "SEND_STOP") == 0) strncpy(lpReturnedString, cfg->mqtt.SEND_STOP, nSize);
        else if (strcmp(lpKeyName, "TOPIC_START") == 0) strncpy(lpReturnedString, cfg->mqtt.TOPIC_START, nSize);
        else if (strcmp(lpKeyName, "TOPIC_STOP") == 0) strncpy(lpReturnedString, cfg->mqtt.TOPIC_STOP, nSize);
        else strncpy(lpReturnedString, lpDefault, nSize);
    } else if (strcmp(lpAppName, "CHANNELTAB") == 0) {
        if (strcmp(lpKeyName, "SHOW_START") == 0) strncpy(lpReturnedString, cfg->channelTab.SHOW_START, nSize);
                else if (strcmp(lpKeyName, "SHOW_STOP") == 0) strncpy(lpReturnedString, cfg->channelTab.SHOW_STOP, nSize);
        else if (strcmp(lpKeyName, "COLOR_START") == 0) strncpy(lpReturnedString, cfg->channelTab.COLOR_START, nSize);
        else if (strcmp(lpKeyName, "COLOR_STOP") == 0) strncpy(lpReturnedString, cfg->channelTab.COLOR_STOP, nSize);
        else if (strcmp(lpKeyName, "PREFIX_START") == 0) strncpy(lpReturnedString, cfg->channelTab.PREFIX_START, nSize);
        else if (strcmp(lpKeyName, "PREFIX_STOP") == 0) strncpy(lpReturnedString, cfg->channelTab.PREFIX_STOP, nSize);
        else strncpy(lpReturnedString, lpDefault, nSize);
    } else if (strcmp(lpAppName, "LOGGING") == 0) {
        if (strcmp(lpKeyName, "LOG_MQTT_MSG") == 0) strncpy(lpReturnedString, cfg->logging.LOG_MQTT_MSG, nSize);
        else strncpy(lpReturnedString, lpDefault, nSize);
    } else if (strcmp(lpAppName, "GENERAL") == 0) {
        if (strcmp(lpKeyName, "LANGUAGE") == 0) strncpy(lpReturnedString, cfg->general.LANGUAGE, nSize);
        else strncpy(lpReturnedString, lpDefault, nSize);
    } else {
        strncpy(lpReturnedString, lpDefault, nSize);
    }

    lpReturnedString[nSize - 1] = '\0';
    return strlen(lpReturnedString);
}

int WritePrivateProfileStringA(
    const char* lpAppName,
    const char* lpKeyName,
    const char* lpString,
    const char* lpFileName
) {
    (void)lpFileName; // Datei wird nicht direkt verändert, nur Struktur
    if (!lpAppName || !lpKeyName || !lpString) return 0;

    LH2MQTT_INI* cfg = &iniData;

    if (strcmp(lpAppName, "MQTT") == 0) {
        if (strcmp(lpKeyName, "PATH") == 0) strncpy(cfg->mqtt.PATH, lpString, sizeof(cfg->mqtt.PATH));
        else if (strcmp(lpKeyName, "HOST") == 0) strncpy(cfg->mqtt.HOST, lpString, sizeof(cfg->mqtt.HOST));
        else if (strcmp(lpKeyName, "PORT") == 0) strncpy(cfg->mqtt.PORT, lpString, sizeof(cfg->mqtt.PORT));
        else if (strcmp(lpKeyName, "USER") == 0) strncpy(cfg->mqtt.USER, lpString, sizeof(cfg->mqtt.USER));
        else if (strcmp(lpKeyName, "PASSWORD") == 0) strncpy(cfg->mqtt.PASSWORD, lpString, sizeof(cfg->mqtt.PASSWORD));
        else if (strcmp(lpKeyName, "QOS") == 0) strncpy(cfg->mqtt.QOS, lpString, sizeof(cfg->mqtt.QOS));
        else if (strcmp(lpKeyName, "CAFILE") == 0) strncpy(cfg->mqtt.CAFILE, lpString, sizeof(cfg->mqtt.CAFILE));
        else if (strcmp(lpKeyName, "SEND_START") == 0) strncpy(cfg->mqtt.SEND_START, lpString, sizeof(cfg->mqtt.SEND_START));
        else if (strcmp(lpKeyName, "SEND_STOP") == 0) strncpy(cfg->mqtt.SEND_STOP, lpString, sizeof(cfg->mqtt.SEND_STOP));
        else if (strcmp(lpKeyName, "TOPIC_START") == 0) strncpy(cfg->mqtt.TOPIC_START, lpString, sizeof(cfg->mqtt.TOPIC_START));
        else if (strcmp(lpKeyName, "TOPIC_STOP") == 0) strncpy(cfg->mqtt.TOPIC_STOP, lpString, sizeof(cfg->mqtt.TOPIC_STOP));
        else return 0;
    } else if (strcmp(lpAppName, "CHANNELTAB") == 0) {
        if (strcmp(lpKeyName, "SHOW_START") == 0) strncpy(cfg->channelTab.SHOW_START, lpString, sizeof(cfg->channelTab.SHOW_START));
        else if (strcmp(lpKeyName, "SHOW_STOP") == 0) strncpy(cfg->channelTab.SHOW_STOP, lpString, sizeof(cfg->channelTab.SHOW_STOP));
        else if (strcmp(lpKeyName, "COLOR_START") == 0) strncpy(cfg->channelTab.COLOR_START, lpString, sizeof(cfg->channelTab.COLOR_START));
        else if (strcmp(lpKeyName, "COLOR_STOP") == 0) strncpy(cfg->channelTab.COLOR_STOP, lpString, sizeof(cfg->channelTab.COLOR_STOP));
        else if (strcmp(lpKeyName, "PREFIX_START") == 0) strncpy(cfg->channelTab.PREFIX_START, lpString, sizeof(cfg->channelTab.PREFIX_START));
        else if (strcmp(lpKeyName, "PREFIX_STOP") == 0) strncpy(cfg->channelTab.PREFIX_STOP, lpString, sizeof(cfg->channelTab.PREFIX_STOP));
        else return 0;
    } else if (strcmp(lpAppName, "LOGGING") == 0) {
        if (strcmp(lpKeyName, "LOG_MQTT_MSG") == 0) strncpy(cfg->logging.LOG_MQTT_MSG, lpString, sizeof(cfg->logging.LOG_MQTT_MSG));
        else return 0;
    } else if (strcmp(lpAppName, "GENERAL") == 0) {
        if (strcmp(lpKeyName, "LANGUAGE") == 0) strncpy(cfg->general.LANGUAGE, lpString, sizeof(cfg->general.LANGUAGE));
        else return 0;
    } else {
        return 0;
    }

    // Nullterminierung
    cfg->mqtt.PATH[sizeof(cfg->mqtt.PATH)-1] = '\0';
    cfg->mqtt.HOST[sizeof(cfg->mqtt.HOST)-1] = '\0';
    cfg->mqtt.PORT[sizeof(cfg->mqtt.PORT)-1] = '\0';
    cfg->mqtt.USER[sizeof(cfg->mqtt.USER)-1] = '\0';
    cfg->mqtt.PASSWORD[sizeof(cfg->mqtt.PASSWORD)-1] = '\0';
    cfg->mqtt.QOS[sizeof(cfg->mqtt.QOS)-1] = '\0';
    cfg->mqtt.CAFILE[sizeof(cfg->mqtt.CAFILE)-1] = '\0';
    cfg->mqtt.SEND_START[sizeof(cfg->mqtt.SEND_START)-1] = '\0';
    cfg->mqtt.SEND_STOP[sizeof(cfg->mqtt.SEND_STOP)-1] = '\0';
    cfg->mqtt.TOPIC_START[sizeof(cfg->mqtt.TOPIC_START)-1] = '\0';
    cfg->mqtt.TOPIC_STOP[sizeof(cfg->mqtt.TOPIC_STOP)-1] = '\0';

    cfg->channelTab.SHOW_START[sizeof(cfg->channelTab.SHOW_START)-1] = '\0';
    cfg->channelTab.SHOW_STOP[sizeof(cfg->channelTab.SHOW_STOP)-1] = '\0';
    cfg->channelTab.COLOR_START[sizeof(cfg->channelTab.COLOR_START)-1] = '\0';
    cfg->channelTab.COLOR_STOP[sizeof(cfg->channelTab.COLOR_STOP)-1] = '\0';
    cfg->channelTab.PREFIX_START[sizeof(cfg->channelTab.PREFIX_START)-1] = '\0';
    cfg->channelTab.PREFIX_STOP[sizeof(cfg->channelTab.PREFIX_STOP)-1] = '\0';

    cfg->logging.LOG_MQTT_MSG[sizeof(cfg->logging.LOG_MQTT_MSG)-1] = '\0';

    cfg->general.LANGUAGE[sizeof(cfg->general.LANGUAGE)-1] = '\0';

    cfg->needWritingIni=TRUE;

    return 1;
}


// Hilfsfunktion: schreibt eine Section + Key=Value in die INI-Datei
void WriteIniValueHelper(FILE* file, const char* key, const char* value)
{
    fprintf(file, "%s=%s\n", key, value ? value : "");
}

// Die Hauptfunktion: schreibt die komplette Struktur in die INI
void WriteCompleteIniFile(const LH2MQTT_INI* cfg)
{
    FILE* f = fopen(currentIniFile, "w");
    if (!f)
    {
        perror("Failed to open INI file for writing");
        return;
    }

    #ifdef _WIN32
        fprintf(f, "; lh2mqtt - LastHeard To Mqtt - TeamSpeak 3 Plugin (Windows)\n");
    #else
        fprintf(f, "; lh2mqtt - LastHeard To Mqtt - TeamSpeak 3 Plugin (Linux)\n");
    #endif
    fprintf(f, ";-------------------------------------------------------------------------------\n");
    fprintf(f, "; Dieses Plugin ermoeglicht die Anzeige des zuletzt aktiven Sprechers \n");
    fprintf(f, ";   1) via MQTT (INI-Bereich: MQTT)\n");
    fprintf(f, ";   2) im Channel-Tab in TeamSpeak 3 (INI-Bereich: CHANNELTAB)\n");
    fprintf(f, ";\n");
    fprintf(f, ";-------------------------------------------------------------------------------\n");
    fprintf(f, "; MQTT:\n");
    fprintf(f, ";-------------------------------------------------------------------------------\n");
    fprintf(f, "; Fuer MQTT wird eine Installation von Mosquitto benoetigt, \n");
    fprintf(f, "; siehe: https://mosquitto.org/download/\n");
    fprintf(f, ";\n");
    #ifdef _WIN32
        fprintf(f, "; PATH beinhaltet den kompletten Pfad zur mosquitto_pub.exe (inkl. EXE)\n");
    #else
        fprintf(f, "; PATH beinhaltet den kompletten Pfad zur mosquitto_pub Binary\n");
        fprintf(f, ";   siehe: whereis mosquitto_pub\n");
    #endif
    fprintf(f, "; HOST kann eine IP-Adresse oder ein Hostname (FQDN) sein\n");
    fprintf(f, ";   also 127.0.0.1 oder meine-broker-fqdn\n");
    fprintf(f, "; PORT des Brokers (anzugeben, falls abweichend von 1883)\n");
    fprintf(f, "; USER/PASSWORD/QOS sind optional\n");
    fprintf(f, "; CAFILE: kompletter Pfad und Dateiname des Server-CA-Bundles (SSL)\n");
    fprintf(f, "; SEND_START/SEND_STOP: 1 gibt an, dass die Info via MQTT gesendet wird\n");
    fprintf(f, "; TOPIC_START/TOPIC_STOP: Topic auf dem die Info veroeffentlicht wird\n");
    fprintf(f, ";\n");
    fprintf(f, ";-------------------------------------------------------------------------------\n");
    fprintf(f, "; CHANNELTAB:\n");
    fprintf(f, ";-------------------------------------------------------------------------------\n");
    fprintf(f, "; Farbwerte sind RGB-HEX-Codes (z.B.: #FF00FF) \n");
    fprintf(f, "; oder manche, einfache Farbnamen auch direkt (z.B.: green, red, yellow)\n");
    fprintf(f, "; siehe: https://www.farb-tabelle.de/de/farbtabelle.htm\n");
    fprintf(f, ";\n");
    fprintf(f, "; SHOW_START/SHOW_STOP: 1 gibt an, dass Info im Channel-Tab ausgegeben wird\n");
    fprintf(f, "; COLOR_START/COLOR_STOP: RGB-Farbwert (bei HEX-Code mit # angeben!)\n");
    fprintf(f, ";\n");
    fprintf(f, ";-------------------------------------------------------------------------------\n");
    fprintf(f, "; LOGGING:\n");
    fprintf(f, ";-------------------------------------------------------------------------------\n");
    fprintf(f, "; LOG_MQTT_MSG: 1 gibt an, dass die gesendete MQTT-Message mitgeloggt wird\n");
    fprintf(f, ";\n");
    fprintf(f, ";-------------------------------------------------------------------------------\n");
    fprintf(f, "; Diese Config wird automatisch beim Programmstart von TeamSpeak 3\n");
    fprintf(f, "; oder nach 'Plugins|lh2mqtt|Konfiguration editieren' neu eingelesen!\n");
    fprintf(f, "; Sollte keine Config existieren, wird ein Standardinhalt als Vorlage erzeugt.\n");
    fprintf(f, ";-------------------------------------------------------------------------------\n");
    fprintf(f, "; Autor: Little.Ben, DH6BS\n");
    fprintf(f, "; Quellcode siehe: https://github.com/Little-Ben/ts3client-pluginsdk-lh2mqtt\n");
    fprintf(f, "; Lizenz: LGPL\n");
    fprintf(f, ";-------------------------------------------------------------------------------\n");
    fprintf(f, "\n");

    // --------- MQTT Section ----------
    fprintf(f, "[MQTT]\n");    
    WriteIniValueHelper(f, "PATH",        cfg->mqtt.PATH);
    WriteIniValueHelper(f, "HOST",        cfg->mqtt.HOST);
    WriteIniValueHelper(f, "PORT",        cfg->mqtt.PORT);
    WriteIniValueHelper(f, "USER",        cfg->mqtt.USER);
    WriteIniValueHelper(f, "PASSWORD",    cfg->mqtt.PASSWORD);
    WriteIniValueHelper(f, "QOS",         cfg->mqtt.QOS);
    WriteIniValueHelper(f, "CAFILE",      cfg->mqtt.CAFILE);
    WriteIniValueHelper(f, "SEND_START",  cfg->mqtt.SEND_START);
    WriteIniValueHelper(f, "SEND_STOP",   cfg->mqtt.SEND_STOP);
    WriteIniValueHelper(f, "TOPIC_START", cfg->mqtt.TOPIC_START);
    WriteIniValueHelper(f, "TOPIC_STOP",  cfg->mqtt.TOPIC_STOP);
    fprintf(f, "\n");

    // --------- CHANNELTAB Section ----------
    fprintf(f, "[CHANNELTAB]\n");
    WriteIniValueHelper(f, "SHOW_START",   cfg->channelTab.SHOW_START);
    WriteIniValueHelper(f, "SHOW_STOP",    cfg->channelTab.SHOW_STOP);
    WriteIniValueHelper(f, "COLOR_START",  cfg->channelTab.COLOR_START);
    WriteIniValueHelper(f, "COLOR_STOP",   cfg->channelTab.COLOR_STOP);
    WriteIniValueHelper(f, "PREFIX_START", cfg->channelTab.PREFIX_START);
    WriteIniValueHelper(f, "PREFIX_STOP",  cfg->channelTab.PREFIX_STOP);
    fprintf(f, "\n");

    // --------- LOGGING Section ----------
    fprintf(f, "[LOGGING]\n");
    WriteIniValueHelper(f, "LOG_MQTT_MSG", cfg->logging.LOG_MQTT_MSG);
    fprintf(f, "\n");

    // --------- GENERAL Section ----------
    fprintf(f, "[GENERAL]\n");
    WriteIniValueHelper(f, "LANGUAGE", cfg->general.LANGUAGE);
    fprintf(f, "\n");

    fclose(f);
}

void FlushIniFile()
{
#ifndef _WIN32
    LH2MQTT_INI* cfg = &iniData;
        
    if (cfg->needWritingIni == TRUE)
    {
        printf("PLUGIN::INI_WRAPPER: FlushIniFile: Writing new configuration to ini file: %s\n", currentIniFile);
        WriteCompleteIniFile(cfg);
    }
    cfg->needWritingIni = FALSE;
#endif
}

#endif // !_WIN32

