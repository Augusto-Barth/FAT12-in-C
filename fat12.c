#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "fat12.h"
#include "entry.h"
#include "utils.h"

fat12_bs bootsector;

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
    //limpar com memset?
    fseek(fp, (33+logicalCluster-2)*CLUSTER_SIZE, SEEK_SET);
    fread(cluster, CLUSTER_SIZE, 1, fp);
}

void print_cluster_sequence(FILE* fp, int firstLogicalCluster, unsigned char* cluster){
    int logicalCluster = firstLogicalCluster;
    int entry = get_entry(fp, logicalCluster);
    read_cluster(fp, logicalCluster, cluster);
    printf("%s", cluster);
    memset(cluster, 0, CLUSTER_SIZE+1);

    if(entry == 0x000){
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

void print_directory_sequence_short(FILE* fp, int dirSector, int subLevel){
    fat12_dir directory;
    fat12_dir_attr attributes;
    int dirNum = 0;

    if(!subLevel){
        green();
        printf("%s\n", MOUNT_POINT);
        reset();
    }
    while(1){
        int filename_int = read_directory(fp, dirSector, dirNum, &directory);
        if(filename_int == 0xE5){
            dirNum++;
            continue;
        }
        else if(filename_int == 0x00)
            break;
        
        printf("|");
        for(int i = 0; i < subLevel; i++)
            printf("  |");
        printf("-");

        remove_spaces(directory.filename);
        read_attributes(directory.attributes, &attributes);


        if(attributes.subdirectory){
            blue();    
            printf("%s\n", directory.filename);
            reset();
        }
        else
            printf("%s.%s\n", directory.filename, directory.extension);

        if(attributes.subdirectory && strcmp(directory.filename, ".") && strcmp(directory.filename, ".."))
            print_directory_sequence_short(fp, directory.firstLogicalCluster+33-2, subLevel+1);

        dirNum++;
    }
}

void find_directory(FILE* fp, int dirSector, int targetSector){
    fat12_dir directory;
    fat12_dir_attr attributes;
    int dirNum = 0;

    if(targetSector == ROOT_DIR_SECTOR){
        green();
        printf("%s\n", MOUNT_POINT);
        reset();
        return;
    }

    while(1){
        int filename_int = read_directory(fp, dirSector, dirNum, &directory);
        if(filename_int == 0xE5){
            dirNum++;
            continue;
        }
        else if(filename_int == 0x00)
            break;

        remove_spaces(directory.filename);
        read_attributes(directory.attributes, &attributes);


        if(attributes.subdirectory && strcmp(directory.filename, ".") && strcmp(directory.filename, "..")){
            if(directory.firstLogicalCluster+33-2 == targetSector){
                green();
                printf("%s\n", directory.filename);
                reset();
                return;
            }
            else{
                find_directory(fp, directory.firstLogicalCluster+33-2, targetSector);
            }
        }
        dirNum++;
    }
}

void print_directory_short(FILE* fp, int dirSector){
    fat12_dir directory;
    fat12_dir_attr attributes;
    int dirNum = 0;

    find_directory(fp, ROOT_DIR_SECTOR, dirSector);

    while(1){
        int filename_int = read_directory(fp, dirSector, dirNum, &directory);
        
        if(filename_int == 0xE5){
            dirNum++;
            continue;
        }
        else if(filename_int == 0x00)
            break;

        printf("|-");

        remove_spaces(directory.filename);
        read_attributes(directory.attributes, &attributes);

        if(attributes.subdirectory){
            blue();
            printf("%s\n", directory.filename);
            reset();
        }
        else
            printf("%s.%s\n", directory.filename, directory.extension);
        dirNum++;
    }
}

void read_file(FILE* fp, int dirSector, unsigned char* filename){
    fat12_dir directory;
    fat12_dir_attr attributes;
    unsigned char* cluster = (unsigned char*)malloc(CLUSTER_SIZE+1);
    unsigned char* fullFilename = (unsigned char*)malloc(12);
    int dirNum = 0, pos = 0;

    to_upper(filename);

    while(1){
        memset(cluster, 0, CLUSTER_SIZE+1);
        memset(fullFilename, 0, 12);
        int filename_int = read_directory(fp, dirSector, dirNum, &directory);
        if(filename_int == 0xE5){
            dirNum++;
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
            // print_time(directory.creationTime);
            // printf(" ");
            // print_date(directory.creationDate);
            // printf("\n");
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

    to_upper(directoryName);

    while(1){
        memset(cluster, 0, CLUSTER_SIZE+1);
        memset(fullFilename, 0, 12);
        int filename_int = read_directory(fp, *dirSector, dirNum, &directory);
        if(filename_int == 0xE5){
            dirNum++;
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
        
        if(attributes.subdirectory)
            *dirSector = (directory.firstLogicalCluster == 0) ? ROOT_DIR_SECTOR : (directory.firstLogicalCluster + 33 - 2);
        else
            printf("%s nao eh um diretorio\n", directory.filename);
        
        break;
    }
    free(cluster);
    free(fullFilename);
    return;
}

int get_first_free_directory_entry(FILE* fp, int dirSector){
    fat12_dir directory;
    int dirNum = 0, maxDirNum = CLUSTER_SIZE/32;
    if(dirSector == ROOT_DIR_SECTOR)
        maxDirNum = bootsector.tamanhoRoot;

    while(maxDirNum--){
        int filename_int = read_directory(fp, dirSector, dirNum, &directory);
        if(filename_int == 0xE5 || filename_int == 0x00){
            return dirNum;
        }
        dirNum++;
    }
    return -1;
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
        return;
    }
    else if(entry >= 0xFF0 && entry <= 0xFF6){
        return;
    }
    else if(entry == 0xFF7){
        return;
    }
    else if(entry >= 0xFF8 && entry <= 0xFFF){
        write_entry(fp, logicalCluster, 0x000);
        return;
    }
    else{
        remove_cluster_sequence(fp, entry);
    }
    write_entry(fp, logicalCluster, 0x000);
}

void remove_file(FILE* fp, int dirSector, unsigned char* filename){
    fat12_dir directory;
    fat12_dir_attr attributes;
    unsigned char* fullFilename = (unsigned char*)malloc(12);
    int dirNum = 0, pos = 0;
    unsigned char buffer[32] = {0};

    fat12_dir directory_next;
    to_upper(filename);

    while(1){
        memset(fullFilename, 0, 12);
        int filename_int = read_directory(fp, dirSector, dirNum, &directory);
        if(filename_int == 0xE5){
            dirNum++;
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
            //printf("NEXT: %X\n", filename_int_next);
            if(filename_int_next != 0x00){
                short bufferInt = 0xE5;
                fseek(fp, CLUSTER_SIZE*dirSector + 32*dirNum, SEEK_SET);
                //printf("At: %X\n", CLUSTER_SIZE*dirSector + 32*dirNum);
                fwrite(&bufferInt, sizeof(bufferInt), 1, fp);
            }
            
        }
        free(fullFilename);
        return;
    }
}

void export_cluster_sequence(FILE* fp, FILE* destFp, int firstLogicalCluster, unsigned char* cluster, int fileSize){
    int logicalCluster = firstLogicalCluster;
    int entry = get_entry(fp, logicalCluster);
    read_cluster(fp, logicalCluster, cluster);
    // printf("%d -> %d\n", firstLogicalCluster, MIN(fileSize, CLUSTER_SIZE));
    fwrite(cluster, MIN(fileSize, CLUSTER_SIZE), 1, destFp);
    fileSize -= CLUSTER_SIZE;
    // fprintf(destFp, "%s", cluster);
    memset(cluster, 0, CLUSTER_SIZE);

    if(entry == 0x00){
        //printf("Unused\n");
        return;
    }
    else if(entry >= 0xFF0 && entry <= 0xFF6){
        //printf("Reserved Cluster\n");
        return;
    }
    else if(entry == 0xFF7){
        //printf("Bad Cluster\n");
        return;
    }
    else if(entry >= 0xFF8 && entry <= 0xFFF){
        //printf("Last Cluster in a file\n");
        return;
    }
    else{
        export_cluster_sequence(fp, destFp, entry, cluster, fileSize);
    }
}

void export_file(FILE* fp, int dirSector, unsigned char* filename, unsigned char* destinationFilename){
    fat12_dir directory;
    fat12_dir_attr attributes;
    unsigned char* cluster = (unsigned char*)malloc(CLUSTER_SIZE);
    unsigned char* fullFilename = (unsigned char*)malloc(12);
    int dirNum = 0, pos = 0;

    FILE* destFp = fopen(destinationFilename, "wb");
    if(destFp == NULL){
        printf("Nao foi possivel abrir %s\n", destinationFilename);
        return;
    }

    to_upper(filename);

    while(1){
        memset(cluster, 0, CLUSTER_SIZE);
        memset(fullFilename, 0, 12);
        int filename_int = read_directory(fp, dirSector, dirNum, &directory);
        if(filename_int == 0xE5){
            dirNum++;
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
            export_cluster_sequence(fp, destFp, directory.firstLogicalCluster, cluster, directory.fileSize);
            print_time(directory.lastWriteTime);
            print_date(directory.lastWriteDate);    
        }
        free(fullFilename);
        free(cluster);
        fclose(destFp);
        return;
    }
}

void import_cluster_sequence(FILE* fp, FILE* destFp, int firstLogicalCluster, unsigned char* cluster, int fileSize){
    int logicalCluster = firstLogicalCluster;
    int entry = get_entry(fp, logicalCluster);
    read_cluster(fp, logicalCluster, cluster);
    // printf("%d -> %d\n", firstLogicalCluster, MIN(fileSize, CLUSTER_SIZE));
    fwrite(cluster, MIN(fileSize, CLUSTER_SIZE), 1, destFp);
    fileSize -= CLUSTER_SIZE;
    // fprintf(destFp, "%s", cluster);
    memset(cluster, 0, CLUSTER_SIZE);

    if(entry == 0x00){
        //printf("Unused\n");
        return;
    }
    else if(entry >= 0xFF0 && entry <= 0xFF6){
        //printf("Reserved Cluster\n");
        return;
    }
    else if(entry == 0xFF7){
        //printf("Bad Cluster\n");
        return;
    }
    else if(entry >= 0xFF8 && entry <= 0xFFF){
        //printf("Last Cluster in a file\n");
        return;
    }
    else{
        import_cluster_sequence(fp, destFp, entry, cluster, fileSize);
    }
}

// int read_ext_file(FILE* srcFp, unsigned char* cluster){
//     int size = 0;
//     memset(cluster, 0, CLUSTER_SIZE+1);
//     while(!feof(srcFp) && size < CLUSTER_SIZE){
//         fread(&(cluster[size++]), 1, 1, srcFp);
//         // printf("%X ", cluster[size-1]);
//     }
//     printf("Read %d bytes\n", size);  
//     if(feof(srcFp))
//         return 1;
//     return 0;
// }

int read_ext_file(FILE* srcFp, unsigned char* cluster, int* remainingFileSize){
    int size = 0, a;
    memset(cluster, 0, CLUSTER_SIZE+1);
    while(size < CLUSTER_SIZE && ((*remainingFileSize)-->0)){
        fread(&(cluster[size++]), 1, 1, srcFp);
        // printf("%X ", cluster[size-1]);
    }
    printf("Read %d bytes\n", size);  
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

int get_free_clusters(FILE* fp){
    int freeClusters = 0, i = 0;
    while(i < CLUSTER_SIZE){
        if(get_entry(fp, i++) == 0x00)
            freeClusters++;
    }
    return freeClusters;
}

void write_cluster(FILE* fp, int logicalCluster, unsigned char* cluster){
    fseek(fp, (33+logicalCluster-2)*CLUSTER_SIZE, SEEK_SET);
    fwrite(cluster, CLUSTER_SIZE, 1, fp);
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

void import_file(FILE* fp, int dirSector, char* fullFilename, char* sourceFilename){
    fat12_dir directory;
    memset(&directory, 0, sizeof(fat12_dir));
    unsigned char* cluster = (unsigned char*)malloc(CLUSTER_SIZE);
    char* filename = (char*)malloc(32);
    char* extension = (char*)malloc(32);
    int dirNum = 0, pos = 0;

    FILE* srcFp = fopen(sourceFilename, "rb");
    if(srcFp == NULL){
        printf("Nao foi possivel abrir %s\n", sourceFilename);
        return;
    }

    int extFilesize = get_ext_file_size(srcFp);
    int freeSpace = get_free_clusters(fp)*CLUSTER_SIZE;
    if(extFilesize > freeSpace){
        printf("Nao ha espaco suficiente na imagem\n");
        return;
    }

    strcpy(filename, strtok(fullFilename, "."));
    strcpy(extension, strtok(NULL, "."));

    size_t tamanhoFilename = strlen(filename);
    if(tamanhoFilename > 8){
        printf("Nome (%s) deve ter ate 8 caracteres (%ld)\n", filename, tamanhoFilename);
        return;
    }
    strncpy(directory.filename, filename, 8);
    for(int i = 0; i < 8 - tamanhoFilename; i++){
        directory.filename[i+tamanhoFilename] = ' ';
    }
    to_upper(directory.filename);

    size_t tamanhoExtensao = strlen(extension);
    if(tamanhoExtensao > 3){
        printf("Extensao (%s) deve ter ate 3 caracteres\n", extension);
        return;
    }
    strncpy(directory.extension, extension, 3);
    for(int i = 0; i < 3 - tamanhoExtensao; i++){
        directory.extension[i+tamanhoExtensao] = ' ';
    }
    to_upper(directory.extension);

    int firstFreeDirectoryNum = get_first_free_directory_entry(fp, dirSector);
    int firstFreeFatPosition = get_first_free_fat_entry(fp);
    directory.firstLogicalCluster = firstFreeFatPosition;
    directory.fileSize = extFilesize;

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    directory.lastWriteDate = directory.creationDate = create_date(tm.tm_mday, tm.tm_mon+1, tm.tm_year-80);
    directory.lastWriteTime = directory.creationTime = create_time(tm.tm_sec, tm.tm_min, tm.tm_hour);

    write_directory(fp, dirSector, firstFreeDirectoryNum, directory);

    int numberOfClustersFile = (extFilesize+CLUSTER_SIZE-1)/CLUSTER_SIZE;
    printf("Number of clusters: %d\n", numberOfClustersFile);

    // while(1){
    //     numberOfClustersFile--;
    //     if(numberOfClustersFile == 0)
    //         break;
    // }

    // IMPORTANTE PERGUNTAR PARA O PROF:
    // incluir \0 ao importar arquivos txt?

    int end = 0;
    int previousFreeFatPosition = firstFreeFatPosition;
    fseek(srcFp, 0, SEEK_SET);
    while(!end){
        end = read_ext_file(srcFp, cluster, &extFilesize);
        // printf("[");
        // printf("%s", cluster);
        // printf("]\n");
        write_cluster(fp, firstFreeFatPosition, cluster);
        write_entry(fp, firstFreeFatPosition, 0xFFF);
        firstFreeFatPosition = get_first_free_fat_entry(fp);
        if(!end){
            write_entry(fp, previousFreeFatPosition, firstFreeFatPosition);
        }
        previousFreeFatPosition = firstFreeFatPosition;
    }

    free(filename);
    free(extension);
    free(cluster);
    fclose(srcFp);
    return;
}

void print_details(FILE* fp, int dirSector, unsigned char* filename){
    fat12_dir directory;
    fat12_dir_attr attributes;
    unsigned char* cluster = (unsigned char*)malloc(CLUSTER_SIZE);
    unsigned char* fullFilename = (unsigned char*)malloc(12);
    int dirNum = 0, pos = 0;

    to_upper(filename);

    while(1){
        memset(cluster, 0, CLUSTER_SIZE);
        memset(fullFilename, 0, 12);
        int filename_int = read_directory(fp, dirSector, dirNum, &directory);
        if(filename_int == 0xE5){
            dirNum++;
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
        
        printf("Filename: %s\n", directory.filename);
        printf("Extension: %s\n", directory.extension);
        printf("Size: %d\n", directory.fileSize);
        printf("First Logical Cluster: %d\n", directory.firstLogicalCluster);
        printf("Archive: %s\n", attributes.archive ? "Yes" : "No");
        printf("Hidden: %s\n", attributes.hidden ? "Yes" : "No");
        printf("Read Only: %s\n", attributes.readOnly ? "Yes" : "No");
        printf("Subdirectory: %s\n", attributes.subdirectory ? "Yes" : "No");
        printf("System: %s\n", attributes.system ? "Yes" : "No");
        printf("Volume Label: %s\n", attributes.volumeLabel ? "Yes" : "No");
        printf("Creation: ");
        print_date(directory.creationDate);
        printf(" ");
        print_time(directory.creationTime);
        printf("\n");

        free(cluster);
        free(fullFilename);
        return;
    }
}

int main(int argc, char* argv[]){

    if(argc != 2){
        printf("Uso: %s path-to-fat12.img\n", argv[0]);
        exit(1);
    }

    FILE *arqFat = fopen(argv[1], "rb+");

    if(arqFat == NULL){
        printf("Nao foi possivel abrir %s\n", argv[1]);
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

    char comando[32] = {0};
    char argumento[32] = {0};
    char argumento2[32] = {0};
    int retorno = 0;
    while(1){
        memset(comando, 0, 32);
        memset(argumento, 0, 32);
        memset(argumento2, 0, 32);
        printf(">");
        scanf("%s", comando);
        if(!strcmp(comando, "cat")){
            scanf("%s", argumento);
            read_file(arqFat, currentWorkingDirSector, argumento);
        }
        else if(!strcmp(comando, "ls")){
            print_directory_short(arqFat, currentWorkingDirSector);
        }
        else if(!strcmp(comando, "ls-1")){
            print_directory_sequence_short(arqFat, ROOT_DIR_SECTOR, 0);
        }
        else if(!strcmp(comando, "cd")){
            scanf("%s", argumento);
            change_directory(arqFat, &currentWorkingDirSector, argumento);
        }
        else if(!strcmp(comando, "free")){
            retorno = get_first_free_directory_entry(arqFat, currentWorkingDirSector);
            if(retorno != -1)
                printf("Primeiro endereco livre: %d\n", retorno);
            else
                printf("Nao ha espacos livres no diretorio atual\n");
        }
        else if(!strcmp(comando, "rm")){
            scanf("%s", argumento);
            remove_file(arqFat, currentWorkingDirSector, argumento);
        }
        else if(!strcmp(comando, "fat")){
            print_fat_table(arqFat);
            printf("Free: %d\n", get_free_clusters(arqFat));
        }
        else if(!strcmp(comando, "exp")){
            scanf("%s", argumento);
            scanf("%s", argumento2);
            export_file(arqFat, currentWorkingDirSector, argumento, argumento2);
        }
        else if(comando[0] == 'q'){
            fclose(arqFat);
            break;
        }
        else if(!strcmp(comando, "det")){
            scanf("%s", argumento);
            print_details(arqFat, currentWorkingDirSector, argumento);
        }
        else if(!strcmp(comando, "imp")){
            // scanf("%s.%s", argumento, argumento2);
            scanf("%s", argumento);
            scanf("%s", argumento2);
            import_file(arqFat, currentWorkingDirSector, argumento, argumento2);
        }
        else
            printf("Comando desconhecido\n");
    }

    return 0;
}
