#include "fileio.h"

void read_bootsector(FILE* fp, fat12_bs* bootsector){
    fread(&bootsector->ignorar, 3, 1, fp); 
    fread(&bootsector->oem, 8, 1, fp); 
    fread(&bootsector->tamanhoSetor, 2, 1, fp); 
    fread(&bootsector->setorPorCluster, 1, 1, fp); 
    fread(&bootsector->NumSetoresRes, 2, 1, fp); 
    fread(&bootsector->NumFats, 1, 1, fp); 
    fread(&bootsector->tamanhoRoot, 2, 1, fp); 
    fread(&bootsector->quantSetoresDisco, 2, 1, fp); 
    fread(&bootsector->mediaDescriptor, 1, 1, fp); 
    fread(&bootsector->setoresPorFat, 2, 1, fp); 
    fread(&bootsector->ignorar4, 30, 1, fp); 
    fread(&bootsector->tipoDeSistema, 8, 1, fp); 
}

unsigned char read_directory(FILE* fp, int dirSector, int dirNum, fat12_dir* directory){
    fseek(fp, CLUSTER_SIZE*dirSector + 32*dirNum, SEEK_SET);
    fread(directory->filename, 8, 1, fp);
    directory->filename[8] = 0;
    fread(&directory->extension, 3, 1, fp);
    directory->extension[3] = 0;
    fread(&directory->attributes, 1, 1, fp);
    fread(&directory->reserved, 2, 1, fp);
    fread(&directory->creationTime, 2, 1, fp);
    fread(&directory->creationDate, 2, 1, fp);
    fread(&directory->lastAccessDate, 2, 1, fp);
    fread(&directory->ignore, 2, 1, fp);
    fread(&directory->lastWriteTime, 2, 1, fp);
    fread(&directory->lastWriteDate, 2, 1, fp);
    fread(&directory->firstLogicalCluster, 2, 1, fp);
    fread(&directory->fileSize, 4, 1, fp);
    return directory->filename[0];
}

void read_attributes(unsigned char attributes_byte, fat12_dir_attr* attributes){
    attributes->readOnly = attributes_byte & 0x01;
    attributes->hidden = attributes_byte & 0x02;
    attributes->system = attributes_byte & 0x04;
    attributes->volumeLabel = attributes_byte & 0x08;
    attributes->subdirectory = attributes_byte & 0x10;
    attributes->archive = attributes_byte & 0x20;
}

void read_cluster(FILE* fp, int logicalCluster, unsigned char* cluster){
    memset(cluster, 0, CLUSTER_SIZE);
    fseek(fp, (33+logicalCluster-2)*CLUSTER_SIZE, SEEK_SET);
    fread(cluster, CLUSTER_SIZE, 1, fp);
}

void remove_cluster(FILE* fp, int logicalCluster){
    unsigned char buffer[CLUSTER_SIZE] = {0};
    fseek(fp, (33+logicalCluster-2)*CLUSTER_SIZE, SEEK_SET);
    fwrite(&buffer, CLUSTER_SIZE, 1, fp);
}

void write_cluster(FILE* fp, int logicalCluster, unsigned char* cluster){
    printf("Writing cluster at logicalCluster %d or physical %x(%d)\n", logicalCluster, (33+logicalCluster-2)*CLUSTER_SIZE, (33+logicalCluster-2)*CLUSTER_SIZE);
    fseek(fp, (33+logicalCluster-2)*CLUSTER_SIZE, SEEK_SET);
    fwrite(cluster, CLUSTER_SIZE, 1, fp);
}

int read_ext_file(FILE* srcFp, unsigned char* cluster, int* remainingFileSize){
    int size = 0, a;
    memset(cluster, 0, CLUSTER_SIZE);
    while(size < CLUSTER_SIZE && ((*remainingFileSize)-->0)){
        fread(&(cluster[size++]), 1, 1, srcFp);
    }
    if(*remainingFileSize<=0)
        return 1;
    return 0;
}

int get_ext_file_size(FILE* srcFp){
    int size = 0;
    unsigned char buffer;
    while(fread(&buffer, 1, 1, srcFp) == 1)
        size++;
    return size;
}

void write_directory(FILE* fp, int dirSector, int dirNum, fat12_dir directory){
    fseek(fp, CLUSTER_SIZE*dirSector + 32*dirNum, SEEK_SET);
    fwrite(&directory.filename, 8, 1 , fp);
    fwrite(&directory.extension, 3, 1, fp);
    fwrite(&directory.attributes, 1, 1, fp);
    fwrite(&directory.reserved, 2, 1, fp);
    fwrite(&directory.creationTime, 2, 1, fp);
    fwrite(&directory.creationDate, 2, 1, fp);
    fwrite(&directory.lastAccessDate, 2, 1, fp);
    fwrite(&directory.ignore, 2, 1, fp);
    fwrite(&directory.lastWriteTime, 2, 1, fp);
    fwrite(&directory.lastWriteDate, 2, 1, fp);
    fwrite(&directory.firstLogicalCluster, 2, 1, fp);
    fwrite(&directory.fileSize, 4, 1, fp);
}