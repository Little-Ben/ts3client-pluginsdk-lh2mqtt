#ifndef INI_WRAPPER_H
#define INI_WRAPPER_H

#include <stdio.h>
#include "ini_structs.h"

#ifdef __cplusplus
extern "C" {
#endif

// Windows API Wrapper
unsigned int GetPrivateProfileStringAWrapper(
    const char* lpAppName,
    const char* lpKeyName,
    const char* lpDefault,
    char* lpReturnedString,
    unsigned int nSize,
    const char* lpFileName
);

// Windows API Wrapper
unsigned int WritePrivateProfileStringAWrapper(
    const char* lpAppName,
    const char* lpKeyName,
    const char* lpString,
    const char* lpFileName
);

void ReadCompleteIniFile(const char configIniFileName[512]);
LH2MQTT_INI* GetIniStruct(void);

void WriteIniValueHelper(FILE* file, const char* key, const char* value);
void WriteCompleteIniFile(const LH2MQTT_INI* cfg);
void FlushIniFile();

#ifdef __cplusplus
}
#endif

#endif
