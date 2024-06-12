#ifndef FAT12
#define FAT12

#define MIN(x, y) x < y ? x : y

#define CLUSTER_SIZE 512
#define ROOT_DIR_SECTOR 19
#define FAT_TABLE1_SECTOR 1
#define FAT_TABLE2_SECTOR 10
#define FAT_TABLE_SIZE (CLUSTER_SIZE*8*9)/12
#define MOUNT_POINT "/"

typedef struct fat12_bootsector
{
    unsigned char ignorar[3];
    unsigned char oem[8];
    short tamanhoSetor;
    unsigned char setorPorCluster;
    short NumSetoresRes;
    unsigned char NumFats;
    short tamanhoRoot;
    short quantSetoresDisco;
    unsigned char mediaDescriptor;
    short setoresPorFat;
    unsigned char ignorar4[30];
    unsigned char tipoDeSistema[8];
}fat12_bs;

typedef struct fat12_directory
{
    char filename[8+1]; // \0
    char extension[3+1];
    unsigned char attributes;
    unsigned char reserved[2];
    short creationTime;
    short creationDate;
    short lastAccessDate;
    short ignore;
    short lastWriteTime;
    short lastWriteDate;
    short firstLogicalCluster;
    int fileSize;
}fat12_dir;

typedef struct fat12_directory_attributes
{
    unsigned char readOnly;
    unsigned char hidden;
    unsigned char system;
    unsigned char volumeLabel;
    unsigned char subdirectory;
    unsigned char archive;
}fat12_dir_attr;

#endif