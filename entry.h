#ifndef ENTRY
#define ENTRY
#include <stdio.h>
#include "fat12.h"

short get_entry(FILE* fp, int position);
void write_entry(FILE* fp, int position, short entry);
int get_first_free_fat_entry(FILE* fp);
int get_last_entry(FILE* fp, int dirSector);
void print_fat_table(FILE* fp);
int get_free_clusters(FILE *fp);

#endif