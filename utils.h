#ifndef UTILS
#define UTILS
#include <stdio.h>
#include <ctype.h>

#ifndef USE_COLORS
#define USE_COLORS 1
#endif

void red();
void blue();
void green();
void reset();

int remove_spaces(char* string);
void to_upper(char* string);
void print_date(short date);
void print_time(short time);
short create_date(unsigned char day, unsigned char month, unsigned char year);
short create_time(unsigned char seconds, unsigned char minutes, unsigned char hours);

#endif