#ifndef INI_STRUCTS_H
#define INI_STRUCTS_H

#define PATH_LEN 256
#define HOST_LEN 64
#define PORT_LEN 8
#define USER_LEN 64
#define PASSWORD_LEN 64
#define QOS_LEN 8
#define CAFILE_LEN 256
#define TOPIC_LEN 128
#define COLOR_LEN 16
#define PREFIX_LEN 64
#define LOG_LEN 8
#define LANG_LEN 8

#ifndef BOOL
    typedef int BOOL;
    #define TRUE  1
    #define FALSE 0
#endif // BOOL

typedef struct {
    char PATH[PATH_LEN];
    char HOST[HOST_LEN];
    char PORT[PORT_LEN];
    char USER[USER_LEN];
    char PASSWORD[PASSWORD_LEN];
    char QOS[QOS_LEN];
    char CAFILE[CAFILE_LEN];
    char SEND_START[LOG_LEN];
    char SEND_STOP[LOG_LEN];
    char TOPIC_START[TOPIC_LEN];
    char TOPIC_STOP[TOPIC_LEN];
} MQTT_SECTION;

typedef struct {
    char SHOW_START[LOG_LEN];
    char SHOW_STOP[LOG_LEN];
    char COLOR_START[COLOR_LEN];
    char COLOR_STOP[COLOR_LEN];
    char PREFIX_START[PREFIX_LEN];
    char PREFIX_STOP[PREFIX_LEN];
} CHANNELTAB_SECTION;

typedef struct {
    char LOG_MQTT_MSG[LOG_LEN];
} LOGGING_SECTION;

typedef struct {
    char LANGUAGE[LANG_LEN];
} GENERAL_SECTION;

typedef struct {
    MQTT_SECTION mqtt;
    CHANNELTAB_SECTION channelTab;
    LOGGING_SECTION logging;
    GENERAL_SECTION general;
    BOOL needWritingIni;
} LH2MQTT_INI;


#endif // INI_STRUCTS_H
