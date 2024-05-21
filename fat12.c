#include <stdio.h>
#include <stdlib.h>

#define CLUSTER_SIZE 512
#define ROOT_DIR CLUSTER_SIZE*19

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

void read_directory(FILE* fp, int dirNum, fat12_dir* directory){
    fseek(fp, ROOT_DIR + 32*dirNum, SEEK_SET);
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

    // unsigned char *buffer = malloc(512);

    // size_t bytesLidos;
    // bytesLidos = fread(bootsector, 1, 512, arqFat);

    // if(bytesLidos < 512)
    //     return 1;

    // fat12_bs boot = *(fat12_bs*)&bootsector;

    // short tamSetor = *(short*)&bootsector[11];

    // printf("Tamanho do setor: %d\n", boot.tamanhoSetor);
    // printf("Tamanho do setor: %d\n", tamSetor);

    fat12_bs bootsector;
    fread(&bootsector.ignorar, 3, 1, arqFat); 
    fread(&bootsector.oem, 8, 1, arqFat); 
    fread(&bootsector.tamanhoSetor, 2, 1, arqFat); 
    fread(&bootsector.setorPorCluster, 1, 1, arqFat); 
    fread(&bootsector.NumSetoresRes, 2, 1, arqFat); 
    fread(&bootsector.NumFats, 1, 1, arqFat); 
    fread(&bootsector.ignorar2, 2, 1, arqFat); 
    fread(&bootsector.quantSetoresDisco, 2, 1, arqFat); 
    fread(&bootsector.mediaDescriptor, 1, 1, arqFat); 
    fread(&bootsector.setoresPorFat, 2, 1, arqFat); 
    fread(&bootsector.ignorar4, 30, 1, arqFat); 
    fread(&bootsector.tipoDeSistema, 8, 1, arqFat); 

    unsigned char buffer[8] = {0};
    unsigned char buffer2[8] = {0};
    fseek(arqFat, CLUSTER_SIZE*1, SEEK_SET);
    fread(buffer, 8, 1, arqFat);

    fseek(arqFat, CLUSTER_SIZE*10, SEEK_SET);
    fread(buffer2, 8, 1, arqFat);

    //printf("FAT1: %s\nFAT2: %s\n", buffer, buffer2);

    for(int i = 0; i < 8; i++){
        if(buffer[i] != buffer2[i]){
            printf("Errado\n");
            return 1;
        }
    }
    printf("Certo\n");

    printf("Tabela FAT:\n");
    for(int i = 0; i < 8; i++){
        printf("%d: %X\n", i, get_entry(arqFat, i));
    }
    printf("\n");

    fat12_dir directory;

    int i = 0;
    while(1){
        read_directory(arqFat, i, &directory);
        int filename_int = (directory.filename[0] << 7) + (directory.filename[1] << 6) + (directory.filename[2] << 5) + (directory.filename[3] << 4) + (directory.filename[4] << 3) + (directory.filename[5] << 2) + (directory.filename[6] << 1) + directory.filename[7];
        if(filename_int == 0xE5){
            printf("Vazio\n");
        }
        else if(filename_int == 0x00){
            printf("Fim\n");
            break;
        }
        remove_spaces(directory.filename);
        printf("filename: %s.%s\n", directory.filename, directory.extension);
        // printf("time: %hu date: %hu\n", directory.creationTime, directory.creationDate); //pag26-> https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf
        printf("first cluster: %hd\n\n", directory.firstLogicalCluster);
        i++;
    }

    return 0;
}
