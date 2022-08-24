#ifndef COM_UTILS_H__
#define COM_UTILS_H__

#include <stdint.h>
#include <string>
#define MAX_REQ_SIZE 20

void printTimeStamp(const char* premsg);
void printTimeStampWithName(const char* name, const char* premsg);
void printTimeStampWithName(const char* name, const char* premsg, FILE* fp);
uint64_t getCurNs();
char *timeStamp();

#else
#endif 
