#include <stdio.h>
#include <ctype.h>

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