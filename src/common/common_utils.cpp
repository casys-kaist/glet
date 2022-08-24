#include "common_utils.h"
#include <sys/time.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <stdint.h>
#include <ctime>
#include <sstream>


char *timeStamp(){
      char *pTimeStamp = (char *)malloc(sizeof(char) * 16); 
      time_t ltime;
      ltime=time(NULL);
      struct tm *tm;
      struct timeval tv;
      int millis;
      tm=localtime(&ltime); 
      gettimeofday(&tv,NULL);
      millis =(tv.tv_usec) / 1000 ; 
      sprintf(pTimeStamp,"%02d:%02d:%02d:%03d", tm->tm_hour, tm->tm_min, tm->tm_sec,millis);
      return pTimeStamp;
}                             


void printTimeStampWithName(const char* name, const char* premsg, FILE* fp){
    char buffer[30];
    struct timeval tv; 
    time_t curtime;
    gettimeofday(&tv, NULL); 
    curtime=tv.tv_sec;
    strftime(buffer,30,"%m-%d-%Y  %T.",localtime(&curtime));
    fprintf(fp,"[%s] %s%06ld : %s\n",name, buffer,tv.tv_usec,premsg);
}

void printTimeStamp(const char* premsg){
    char buffer[30];
    struct timeval tv; 
    time_t curtime;
    gettimeofday(&tv, NULL); 
    curtime=tv.tv_sec;
    strftime(buffer,30,"%m-%d-%Y  %T.",localtime(&curtime));
    printf("%s%06ld : %s\n",buffer,tv.tv_usec,premsg);
}

void printTimeStampWithName(const char* name, const char* premsg){
    char buffer[30];
    struct timeval tv; 
    time_t curtime;
    gettimeofday(&tv, NULL); 
    curtime=tv.tv_sec;
    strftime(buffer,30,"%m-%d-%Y  %T.",localtime(&curtime));
    printf("[%s] %s%06ld : %s\n",name, buffer,tv.tv_usec,premsg);
}

uint64_t getCurNs() {
   struct timespec ts; 
   clock_gettime(CLOCK_REALTIME, &ts);
   uint64_t t = ts.tv_sec*1000*1000*1000 + ts.tv_nsec;
   return t;
}