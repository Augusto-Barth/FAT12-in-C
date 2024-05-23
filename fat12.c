#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "fat12.h"

#define CLUSTER_SIZE 512
#define ROOT_DIR_SECTOR 19
fat12_bs bootsector;

int get_entry(FILE* fp, int position){
    // Entrar com a posicao absoluta no arquivo e verificar se esta dentro de uma
    // tabela FAT (de preferencia a primeira) (NAO)
    // Tambem fazer a verificacao se a entrada eh a mesma nas duas tabelas (nao sei
    // se eh assim que funciona na FAT12)
    // Talvez preservar a posicao do ponteiro de arquivo no fim da funcao (ftell e fseek) (NAO)
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

int remove_spaces(char* string){
    int i = 0;
    while(string[i] != ' ' && string[i] != 0){
        i++;
    }
    string[i] = 0;
    return i;
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

void print_date(short date){
    unsigned char day, month, year;
    day = date&0b11111;
    month = (date&(0b1111<<5))>>5;
    year = (date&(0b1111111<<9))>>9;
    printf("%02d/%02d/%d\n", day, month, year+1980);
}

void print_time(short time){
    unsigned char seconds, minutes, hours;
    seconds = (time&0b11111)*2;
    minutes = (time&(0b111111<<5))>>5;
    hours = (time&(0b11111<<11))>>11;
    printf("%02d:%02d:%02d\n", hours, minutes, seconds);
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
        int filename_int = read_directory(fp, dirSector, dirNum, &directory);
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
        if(attributes.subdirectory){
            printf("subdirectory: %s\n", directory.filename);
            printf("----\n");
        }
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
        dirNum++;
    }
}

void print_directory_sequence_short(FILE* fp, int dirSector, int subLevel){
    fat12_dir directory;
    fat12_dir_attr attributes;
    unsigned char* cluster = (unsigned char*)malloc(CLUSTER_SIZE+1);
    int dirNum = 0;
    while(1){
        memset(cluster, 0, CLUSTER_SIZE+1);
        int filename_int = read_directory(fp, dirSector, dirNum, &directory);
        if(filename_int == 0xE5)
            continue;
        else if(filename_int == 0x00)
            break;
        
        for(int i = 0; i < subLevel; i++)
            printf("|"); // printar o caminho completo Ex.:"/SUBDIR/TESTE.C"


        remove_spaces(directory.filename);
        read_attributes(directory.attributes, &attributes);

        if(subLevel)
            printf("-");

        if(attributes.subdirectory){
            printf("subdirectory: %s\n", directory.filename);
        }
        else
            printf("filename: %s.%s\n", directory.filename, directory.extension);

        if(attributes.subdirectory && strcmp(directory.filename, ".") && strcmp(directory.filename, "..")){ //Verificar com strcmp . e .. (preguica)
            print_directory_sequence_short(fp, directory.firstLogicalCluster+33-2, subLevel+1);
        }
        dirNum++;
    }
}

void print_directory_short(FILE* fp, int dirSector, int subLevel){
    fat12_dir directory;
    fat12_dir_attr attributes;
    unsigned char* cluster = (unsigned char*)malloc(CLUSTER_SIZE+1);
    int dirNum = 0;

    while(1){
        memset(cluster, 0, CLUSTER_SIZE+1);
        int filename_int = read_directory(fp, dirSector, dirNum, &directory);
        
        if(filename_int == 0xE5){
            dirNum++;
            continue;
        }
        else if(filename_int == 0x00)
            break;
        
        for(int i = 0; i < subLevel; i++)
            printf("|");


        remove_spaces(directory.filename);
        read_attributes(directory.attributes, &attributes);

        if(subLevel)
            printf("-");

        if(attributes.subdirectory){
            printf("subdirectory: %s\n", directory.filename);
        }
        else
            printf("filename: %s.%s\n", directory.filename, directory.extension);
        dirNum++;
    }
}

void read_file(FILE* fp, int dirSector, unsigned char* filename){
    fat12_dir directory;
    fat12_dir_attr attributes;
    unsigned char* cluster = (unsigned char*)malloc(CLUSTER_SIZE+1);
    unsigned char* fullFilename = (unsigned char*)malloc(12);
    int dirNum = 0, pos = 0;

    while(1){
        if(filename[pos] == 0)
            break;
        filename[pos] = toupper(filename[pos]);
        pos++;
    }

    while(1){
        memset(cluster, 0, CLUSTER_SIZE+1);
        memset(fullFilename, 0, 12);
        int filename_int = read_directory(fp, dirSector, dirNum, &directory);
        if(filename_int == 0xE5){
            continue;
        }
        else if(filename_int == 0x00){
            printf("%s nao encontrado\n", filename);
            break;
        }

        int stop = remove_spaces(directory.filename);

        strcpy(fullFilename, directory.filename);
        read_attributes(directory.attributes, &attributes);

        if(!(directory.extension[0] == ' ')){
            fullFilename[stop++] = '.';
            for(int i = 0; i < 3; i++){
                fullFilename[stop+i] = directory.extension[i];
            }
            remove_spaces(fullFilename);
        }

        if(strcmp(filename, fullFilename)){
            dirNum++;
            continue;
        }
        
        if(attributes.subdirectory){
            printf("%s eh um diretorio\n", directory.filename);
        }
        else{
            print_cluster_sequence(fp, directory.firstLogicalCluster, cluster);
            print_time(directory.lastWriteTime);
            print_date(directory.lastWriteDate);    
        }
        
        return;
    }
}

void change_directory(FILE* fp, int* dirSector, unsigned char* directoryName){
    fat12_dir directory;
    fat12_dir_attr attributes;
    unsigned char* cluster = (unsigned char*)malloc(CLUSTER_SIZE+1);
    unsigned char* fullFilename = (unsigned char*)malloc(12);
    int dirNum = 0, pos = 0;

    while(1){
        if(directoryName[pos] == 0)
            break;
        directoryName[pos] = toupper(directoryName[pos]);
        pos++;
    }

    while(1){
        memset(cluster, 0, CLUSTER_SIZE+1);
        memset(fullFilename, 0, 12);
        read_directory(fp, *dirSector, dirNum, &directory);
        int filename_int = (directory.filename[0] << 7) + (directory.filename[1] << 6) + (directory.filename[2] << 5) + (directory.filename[3] << 4) + (directory.filename[4] << 3) + (directory.filename[5] << 2) + (directory.filename[6] << 1) + directory.filename[7];
        if(filename_int == 0xE5){
            continue;
        }
        else if(filename_int == 0x00){
            printf("%s nao encontrado\n", directoryName);
            break;
        }

        int stop = remove_spaces(directory.filename);
        strcpy(fullFilename, directory.filename);
        read_attributes(directory.attributes, &attributes);
        
        if(!(directory.extension[0] == ' ')){
            fullFilename[stop++] = '.';
            for(int i = 0; i < 3; i++){
                fullFilename[stop+i] = directory.extension[i];
            }
            remove_spaces(fullFilename);
        }

        if(strcmp(directoryName, fullFilename)){
            dirNum++;
            continue;
        }
        
        if(attributes.subdirectory){
            *dirSector = (directory.firstLogicalCluster == 0) ? ROOT_DIR_SECTOR : (directory.firstLogicalCluster + 33 - 2);
        }
        else
            printf("%s nao eh um diretorio\n", directory.filename);
        
        return;
    }
}

int get_first_free_directory_entry(FILE* fp, int dirSector, int* freeDirNum){
    fat12_dir directory;
    fat12_dir_attr attributes;
    int dirNum = 0, maxDirNum = CLUSTER_SIZE/32;
    if(dirSector == ROOT_DIR_SECTOR)
        maxDirNum = bootsector.tamanhoRoot;

    while(maxDirNum--){
        int filename_int = read_directory(fp, dirSector, dirNum, &directory);
        if(filename_int == 0xE5 || filename_int == 0x00){
            *freeDirNum = dirNum;
            return 1;
        }
        dirNum++;
    }
    *freeDirNum = -1;
    return 0;
}

void print_fat_table(FILE* fp){
    for(int i = 0; i < CLUSTER_SIZE; i++){
        if(i % 8 == 0 && i != 0)
            printf("\n");
        printf("%03X ", get_entry(fp, i));
    }
    printf("\n");
}

void remove_entry(FILE* fp, int position){
    unsigned char full, half, buffer;
    if(position%2 == 0){
        fseek(fp, CLUSTER_SIZE*1 + (3*position)/2+1, SEEK_SET);
        fread(&half, 1, 1, fp);
        buffer = half&0b11110000;
        fwrite(&buffer, 1, 1, fp);

        fseek(fp, CLUSTER_SIZE*1 + (3*position)/2, SEEK_SET);
        fread(&full, 1, 1, fp);
        buffer = full&0b00000000;
        fwrite(&buffer, 1, 1, fp);
    }
    else{
        fseek(fp, CLUSTER_SIZE*1 + (3*position)/2, SEEK_SET);
        fread(&half, 1, 1, fp);
        buffer = half&0b00001111;
        fwrite(&buffer, 1, 1, fp);

        fseek(fp, CLUSTER_SIZE*1 + (3*position)/2+1, SEEK_SET);
        fread(&full, 1, 1, fp);
        buffer = full&0b00000000;
        fwrite(&buffer, 1, 1, fp);
    }
}

void remove_cluster(FILE* fp, int logicalCluster){
    unsigned char buffer[512] = {0};
    fseek(fp, (33+logicalCluster-2)*CLUSTER_SIZE, SEEK_SET);
    fwrite(&buffer, 512, 1, fp);
}


void remove_cluster_sequence(FILE* fp, int firstLogicalCluster){
    int logicalCluster = firstLogicalCluster;
    int entry = get_entry(fp, logicalCluster);
    remove_cluster(fp, logicalCluster);

    if(entry == 0x00){
        // printf("Unused\n");
        return;
    }
    else if(entry >= 0xFF0 && entry <= 0xFF6){
        // printf("Reserved Cluster\n");
        return;
    }
    else if(entry == 0xFF7){
        // printf("Bad Cluster\n");
        return;
    }
    else if(entry >= 0xFF8 && entry <= 0xFFF){
        //printf("Last Cluster in a file\n");
        remove_entry(fp, logicalCluster);
        return;
    }
    else{
        remove_cluster_sequence(fp, entry);
    }
    remove_entry(fp, logicalCluster);
}

void remove_file(FILE* fp, int dirSector, unsigned char* filename){
    fat12_dir directory;
    fat12_dir_attr attributes;
    unsigned char* fullFilename = (unsigned char*)malloc(12);
    int dirNum = 0, pos = 0;
    unsigned char buffer[32] = {0};

    fat12_dir directory_next;
    while(1){
        if(filename[pos] == 0)
            break;
        filename[pos] = toupper(filename[pos]);
        pos++;
    }

    while(1){
        memset(fullFilename, 0, 12);
        int filename_int = read_directory(fp, dirSector, dirNum, &directory);
        if(filename_int == 0xE5){
            continue;
        }
        else if(filename_int == 0x00){
            printf("%s nao encontrado\n", filename);
            break;
        }

        int stop = remove_spaces(directory.filename);

        strcpy(fullFilename, directory.filename);
        read_attributes(directory.attributes, &attributes);

        if(!(directory.extension[0] == ' ')){
            fullFilename[stop++] = '.';
            for(int i = 0; i < 3; i++){
                fullFilename[stop+i] = directory.extension[i];
            }
            remove_spaces(fullFilename);
        }
        // TODO separar tudo aqui em cima em uma funcao find_file que retorna diretorio, fullFilename e tal
        if(strcmp(filename, fullFilename)){
            dirNum++;
            continue;
        }
        
        if(attributes.subdirectory){
            printf("%s eh um diretorio\n", directory.filename);
        }
        else{
            remove_cluster_sequence(fp, directory.firstLogicalCluster);
            fseek(fp, CLUSTER_SIZE*dirSector + 32*dirNum, SEEK_SET);
            fwrite(&buffer, 32, 1, fp);
            int filename_int_next = read_directory(fp, dirSector, dirNum+1, &directory_next);
            printf("NEXT: %X\n", filename_int_next);
            if(filename_int_next != 0x00){
                int bufferInt = 0xE5;
                fseek(fp, CLUSTER_SIZE*dirSector + 32*dirNum, SEEK_SET);
                fwrite(&bufferInt, sizeof(bufferInt), 1, fp);
            }
            
        }
        
        return;
    }
}

int main(int argc, char* argv[]){

    if(argc != 2){
        printf("Usage: %s path-to-fat12.img\n", argv[0]);
        exit(1);
    }

    FILE *arqFat = fopen(argv[1], "rb+");

    if(arqFat == NULL){
        printf("Unable to open %s\n", argv[1]);
        exit(1);
    }

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
    // printf("Tabela FAT:\n");
    // for(int i = 0; i < 8; i++){
    //     printf("%d: %X\n", i, get_entry(arqFat, i));
    // }
    // printf("\n");

    //print_directory_sequence_short(arqFat, ROOT_DIR_SECTOR, 0);
    //printf("--------------------\n");

    int currentWorkingDirSector = ROOT_DIR_SECTOR; // Mudar com cd

    print_fat_table(arqFat);
    unsigned char comando[32] = {0};
    unsigned char argumento[32] = {0};
    int retorno = 0;
    while(1){
        memset(comando, 0, 32);
        memset(argumento, 0, 32);
        printf(">");
        scanf("%s", comando);
        if(!strcmp(comando, "cat")){
            scanf("%s", argumento);
            read_file(arqFat, currentWorkingDirSector, argumento);
            //printf("\n");
        }
        else if(!strcmp(comando, "ls")){
            print_directory_short(arqFat, currentWorkingDirSector, 0);
        }
        else if(!strcmp(comando, "ls-1")){
            print_directory_sequence_short(arqFat, ROOT_DIR_SECTOR, 0);
        }
        else if(!strcmp(comando, "cd")){
            scanf("%s", argumento);
            change_directory(arqFat, &currentWorkingDirSector, argumento);
        }
        else if(!strcmp(comando, "free")){
            if(get_first_free_directory_entry(arqFat, currentWorkingDirSector, &retorno))
                printf("Primeiro endereco livre: %d\n", retorno);
            else
                printf("Nao ha espacos livres no diretorio atual\n");
        }
        else if(!strcmp(comando, "rm")){
            scanf("%s", argumento);
            remove_file(arqFat, currentWorkingDirSector, argumento);
        }
        else if(comando[0] == 'q'){
            fclose(arqFat);
            break;
        }
        else
            printf("Comando desconhecido\n");
    }

    return 0;
}
