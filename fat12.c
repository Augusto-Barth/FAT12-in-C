#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUSTER_SIZE 512
#define ROOT_DIR_SECTOR 19

typedef struct fat12_bootsector
{
    unsigned char ignorar[3];
    unsigned char oem[8];
    short tamanhoSetor;
    unsigned char setorPorCluster;
    short NumSetoresRes;
    unsigned char NumFats;
    unsigned char ignorar2[2];
    short quantSetoresDisco;
    unsigned char mediaDescriptor;
    short setoresPorFat;
    unsigned char ignorar4[30];
    unsigned char tipoDeSistema[8];
}fat12_bs;

typedef struct fat12_directory
{
    unsigned char filename[8+1]; // \0
    unsigned char extension[3];
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

int get_entry(FILE* fp, int position){
    // Entrar com a posicao absoluta no arquivo e verificar se esta dentro de uma
    // tabela FAT (de preferencia a primeira)
    // Tambem fazer a verificacao se a entrada eh a mesma nas duas tabelas (nao sei
    // se eh assim que funciona na FAT12)
    // Talvez preservar a posicao do ponteiro de arquivo no fim da funcao (ftell e fseek)
    int entry = 0;
    unsigned char full, half;
    if(position%2 == 0){
        fseek(fp, CLUSTER_SIZE*1 + (3*position)/2+1, SEEK_SET);
        fread(&half, 1, 1, fp);
        fseek(fp, CLUSTER_SIZE*1 + (3*position)/2, SEEK_SET);
        fread(&full, 1, 1, fp);
        entry = ((half&0b00001111) << 8) + full;
    }
    else{
        fseek(fp, CLUSTER_SIZE*1 + (3*position)/2, SEEK_SET);
        fread(&half, 1, 1, fp);
        fseek(fp, CLUSTER_SIZE*1 + (3*position)/2+1, SEEK_SET);
        fread(&full, 1, 1, fp);
        entry = ((half&0b11110000) >> 4) + (full << 4);
    }
    return entry;
}

void read_bootsector(FILE* fp, fat12_bs* bootsector){
    fread(&bootsector->ignorar, 3, 1, fp); 
    fread(&bootsector->oem, 8, 1, fp); 
    fread(&bootsector->tamanhoSetor, 2, 1, fp); 
    fread(&bootsector->setorPorCluster, 1, 1, fp); 
    fread(&bootsector->NumSetoresRes, 2, 1, fp); 
    fread(&bootsector->NumFats, 1, 1, fp); 
    fread(&bootsector->ignorar2, 2, 1, fp); 
    fread(&bootsector->quantSetoresDisco, 2, 1, fp); 
    fread(&bootsector->mediaDescriptor, 1, 1, fp); 
    fread(&bootsector->setoresPorFat, 2, 1, fp); 
    fread(&bootsector->ignorar4, 30, 1, fp); 
    fread(&bootsector->tipoDeSistema, 8, 1, fp); 
}

void read_directory(FILE* fp, int dirSector, int dirNum, fat12_dir* directory){
    fseek(fp, CLUSTER_SIZE*dirSector + 32*dirNum, SEEK_SET);
    fread(directory->filename, 8, 1, fp);
    directory->filename[8] = 0;
    fread(&directory->extension, 3, 1, fp);
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
}

void remove_spaces(char* string){
    int i = 0;
    while(string[i] != ' '){
        i++;
    }
    string[i] = 0;
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
    fseek(fp, (33+logicalCluster-2)*CLUSTER_SIZE, SEEK_SET);
    fread(cluster, CLUSTER_SIZE, 1, fp);
}

void print_cluster_sequence(FILE* fp, int firstLogicalCluster, unsigned char* cluster){
    int logicalCluster = firstLogicalCluster;
    int entry = get_entry(fp, logicalCluster);
    read_cluster(fp, logicalCluster, cluster);
    printf("%s", cluster);
    memset(cluster, 0, CLUSTER_SIZE+1);

    if(entry == 0x00){
        printf("Unused\n");
        return;
    }
    else if(entry >= 0xFF0 && entry <= 0xFF6){
        printf("Reserved Cluster\n");
        return;
    }
    else if(entry == 0xFF7){
        printf("Bad Cluster\n");
        return;
    }
    else if(entry >= 0xFF8 && entry <= 0xFFF){
        //printf("Last Cluster in a file\n");
        printf("\n");
        return;
    }
    else{
        print_cluster_sequence(fp, entry, cluster);
    }
}

void print_directory_sequence(FILE* fp, int dirSector){
    fat12_dir directory;
    fat12_dir_attr attributes;
    unsigned char* cluster = (unsigned char*)malloc(CLUSTER_SIZE+1);
    int dirNum = 0;
    while(1){
        memset(cluster, 0, CLUSTER_SIZE+1);
        read_directory(fp, dirSector, dirNum, &directory);
        int filename_int = (directory.filename[0] << 7) + (directory.filename[1] << 6) + (directory.filename[2] << 5) + (directory.filename[3] << 4) + (directory.filename[4] << 3) + (directory.filename[5] << 2) + (directory.filename[6] << 1) + directory.filename[7];
        //printf("dirSector: %d filename: %s\n", dirSector, directory.filename);
        if(filename_int == 0xE5){
            printf("Vazio\n");
        }
        else if(filename_int == 0x00){
            printf("Fim\n");
            break;
        }
        remove_spaces(directory.filename);
        read_attributes(directory.attributes, &attributes);
        if(attributes.subdirectory)
            printf("subdirectory: %s\n", directory.filename);
        else
            printf("filename: %s.%s\n", directory.filename, directory.extension);
        // printf("time: %hu date: %hu\n", directory.creationTime, directory.creationDate); //pag26-> https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf
        printf("first cluster: %hd\n\n", directory.firstLogicalCluster);
        // printf("Cluster:\n");
        // read_cluster(fp, directory.firstLogicalCluster, cluster);
        // printf("%s\n----FIM----\n", cluster);
        if(attributes.subdirectory && strcmp(directory.filename, ".") && strcmp(directory.filename, "..")){ //Verificar com strcmp . e .. (preguica)
            print_directory_sequence(fp, directory.firstLogicalCluster+33-2);
        }
        else
            print_cluster_sequence(fp, directory.firstLogicalCluster, cluster);
        printf("----\n");
        dirNum++;
    }
}

int main(int argc, char* argv[]){

    if(argc != 2){
        printf("Usage: %s path-to-fat12.img\n", argv[0]);
        exit(1);
    }

    FILE *arqFat = fopen(argv[1], "rb");

    if(arqFat == NULL){
        printf("Unable to open %s\n", argv[1]);
        exit(1);
    }

    fat12_bs bootsector;
    read_bootsector(arqFat, &bootsector);


    /*Verifica se as tabelas FAT sao iguais*/
    // unsigned char buffer[8] = {0};
    // unsigned char buffer2[8] = {0};
    // fseek(arqFat, CLUSTER_SIZE*1, SEEK_SET);
    // fread(buffer, 8, 1, arqFat);

    // fseek(arqFat, CLUSTER_SIZE*10, SEEK_SET);
    // fread(buffer2, 8, 1, arqFat);

    // for(int i = 0; i < 8; i++){
    //     if(buffer[i] != buffer2[i]){
    //         printf("Errado\n");
    //         return 1;
    //     }
    // }
    // printf("Certo\n");
    printf("Tabela FAT:\n");
    for(int i = 0; i < 8; i++){
        printf("%d: %X\n", i, get_entry(arqFat, i));
    }
    printf("\n");

    print_directory_sequence(arqFat, ROOT_DIR_SECTOR);

    return 0;
}
