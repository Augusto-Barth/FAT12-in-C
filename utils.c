#include <stdio.h>
#include <ctype.h>

#include "utils.h"

#if USE_COLORS == 1
void red () { printf("\033[1;31m"); }
void blue() { printf("\033[0;34m"); }
void green() { printf("\033[0;32m"); }
void reset () { printf("\033[0m"); }
#else
void red () { printf(""); }
void blue() { printf(""); }
void green() { printf(""); }
void reset () { printf(""); }
#endif


int remove_spaces(char* string){
    int i = 0;
    while(string[i] != ' ' && string[i] != 0){
        i++;
    }
    string[i] = 0;
    return i;
}

void to_upper(char* string){
    int pos = 0;
    while(1){
        if(string[pos] == 0)
            break;
        string[pos] = toupper(string[pos]);
        pos++;
    }
}

void print_date(short date){
    unsigned char day, month, year;
    day = date&0b11111;
    month = (date&(0b1111<<5))>>5;
    year = (date&(0b1111111<<9))>>9;
    printf("%02d/%02d/%d", day, month, year+1980);
}

void print_time(short time){
    unsigned char seconds, minutes, hours;
    seconds = (time&0b11111)*2;
    minutes = (time&(0b111111<<5))>>5;
    hours = (time&(0b11111<<11))>>11;
    printf("%02d:%02d:%02d", hours, minutes, seconds);
}

short create_date(unsigned char day, unsigned char month, unsigned char year){
    short date = 0;
    date = (date&(~0b11111))|(day&(0b11111));
    date = (date&(~(0b1111<<5)))|((month&0b1111)<<5);
    date = (date&(~(0b1111111<<9)))|((year&0b1111111)<<9);
    return date;
}

short create_time(unsigned char seconds, unsigned char minutes, unsigned char hours){
    short time = 0;
    time = (time&(~0b11111))|((seconds&(0b11111))/2);
    time = (time&(~(0b111111<<5)))|((minutes&0b111111)<<5);
    time = (time&(~(0b11111<<11)))|((hours&0b11111)<<11);
    return time;
}