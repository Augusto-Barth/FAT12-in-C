#ifndef FILEIO
#define FILEIO
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fat12.h"
#include "entry.h"

void read_bootsector(FILE *fp, fat12_bs *bootsector);
unsigned char read_directory(FILE *fp, int dirSector, int dirNum, fat12_dir *directory);
void read_attributes(unsigned char attributes_byte, fat12_dir_attr *attributes);

void read_cluster(FILE *fp, int logicalCluster, unsigned char *cluster);
void remove_cluster(FILE *fp, int logicalCluster);
void write_cluster(FILE *fp, int logicalCluster, unsigned char *cluster);

int read_ext_file(FILE *srcFp, unsigned char *cluster, int *remainingFileSize);
int get_ext_file_size(FILE *srcFp);

void write_directory(FILE *fp, int dirSector, int dirNum, fat12_dir directory);

#endif

