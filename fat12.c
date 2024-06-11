#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "fat12.h"
#include "entry.h"
#include "utils.h"
#include "fileio.h"

fat12_bs bootsector;

int get_full_filename(fat12_dir directory, char* fullFilename){
    memset(fullFilename, 0, 12);

    strcpy(fullFilename, directory.filename);
    int stop = remove_spaces(fullFilename);

    if(!(directory.extension[0] == ' ')){
        fullFilename[stop++] = '.';
        for(int i = 0; i < 3; i++){
            fullFilename[stop+i] = directory.extension[i];
        }
        remove_spaces(fullFilename);
    }
}

int get_max_directory_size(int dirSector){
    return (dirSector == ROOT_DIR_SECTOR) ? bootsector.tamanhoRoot : CLUSTER_SIZE/32;
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

int get_free_root_dir(FILE* fp){
    int freeDirs = 0;
    unsigned char dir[32];
    fseek(fp, CLUSTER_SIZE*ROOT_DIR_SECTOR, SEEK_SET);
    for(int i = 0; i < bootsector.tamanhoRoot; i++){
        fread(dir, 32, 1, fp);
        if(dir[0] == 0xE5 || dir[0] == 0x00)
            freeDirs++;
    }
    return freeDirs;
}

void print_cluster_sequence(FILE* fp, int firstLogicalCluster, unsigned char* cluster){
    int logicalCluster = firstLogicalCluster;
    int entry = get_entry(fp, logicalCluster);
    read_cluster(fp, logicalCluster, cluster);
    printf("%s", cluster);

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
    int dirNum = 0, maxDirNum, entry;

    if(!subLevel){
        green();
        printf("%s\n", MOUNT_POINT);
        reset();
    }
    while(1){
        maxDirNum = get_max_directory_size(dirSector);
        while(dirNum < maxDirNum){
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
        if(dirSector == ROOT_DIR_SECTOR)
            break;

        entry = get_entry(fp, dirSector-33+2);
        if(entry >= 0xFF8)
            break;
        dirNum = 0;
        dirSector = entry+33-2;
    }
}

void print_directory_name(FILE* fp, int dirSector, int targetSector){
    fat12_dir directory;
    fat12_dir_attr attributes;
    int dirNum = 0, maxDirNum, entry;

    if(targetSector == ROOT_DIR_SECTOR){
        green();
        printf("%s\n", MOUNT_POINT);
        reset();
        return;
    }

    while(1){
        maxDirNum = get_max_directory_size(dirSector);
        while(dirNum < maxDirNum){
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
                    print_directory_name(fp, directory.firstLogicalCluster+33-2, targetSector);
                }
            }
            dirNum++;
        }
        if(dirSector == ROOT_DIR_SECTOR)
            break;

        entry = get_entry(fp, dirSector-33+2);
        if(entry >= 0xFF8)
            break;
        dirNum = 0;
        dirSector = entry+33-2;
    }
}

void print_directory_short(FILE* fp, int dirSector){
    fat12_dir directory;
    fat12_dir_attr attributes;
    int dirNum = 0, maxDirNum, entry;
    print_directory_name(fp, ROOT_DIR_SECTOR, dirSector);

    while(1){
        maxDirNum = get_max_directory_size(dirSector);
        while(dirNum < maxDirNum){
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
        if(dirSector == ROOT_DIR_SECTOR)
            break;

        entry = get_entry(fp, dirSector-33+2);
        if(entry >= 0xFF8)
            break;
        dirNum = 0;
        dirSector = entry+33-2;
    }
}

void read_file(FILE* fp, int dirSector, unsigned char* filename){
    fat12_dir directory;
    fat12_dir_attr attributes;
    unsigned char* cluster = (unsigned char*)malloc(CLUSTER_SIZE+1);
    char* fullFilename = (char*)malloc(12);
    memset(cluster, 0, CLUSTER_SIZE+1);
    int dirNum = 0, maxDirNum, entry;

    to_upper(filename);

    while(1){
        maxDirNum = get_max_directory_size(dirSector);
        while(dirNum < maxDirNum){
            int filename_int = read_directory(fp, dirSector, dirNum, &directory);
            if(filename_int == 0xE5){
                dirNum++;
                continue;
            }
            else if(filename_int == 0x00){
                printf("%s nao encontrado\n", filename);
                break;
            }

            get_full_filename(directory, fullFilename);
            read_attributes(directory.attributes, &attributes);

            if(strcmp(filename, fullFilename)){
                dirNum++;
                continue;
            }
            
            if(attributes.subdirectory){
                printf("%s eh um diretorio\n", fullFilename);
            }
            else{
                print_cluster_sequence(fp, directory.firstLogicalCluster, cluster);
            }
            free(cluster);
            free(fullFilename);
            return;
        }
        if(dirSector == ROOT_DIR_SECTOR)
            break;

        entry = get_entry(fp, dirSector-33+2);
        if(entry >= 0xFF8)
            break;
        dirNum = 0;
        dirSector = entry+33-2;
    }
    free(cluster);
    free(fullFilename);
    return;
}

void change_directory(FILE* fp, int* dirSector, unsigned char* directoryName){
    fat12_dir directory;
    fat12_dir_attr attributes;
    char* fullFilename = (char*)malloc(12);
    int dirNum = 0, maxDirNum, entry;

    to_upper(directoryName);

    while(1){
        maxDirNum = get_max_directory_size(*dirSector);
        while(dirNum < maxDirNum){
            int filename_int = read_directory(fp, *dirSector, dirNum, &directory);
            if(filename_int == 0xE5){
                dirNum++;
                continue;
            }
            else if(filename_int == 0x00){
                printf("%s nao encontrado\n", directoryName);
                break;
            }

            get_full_filename(directory, fullFilename);
            read_attributes(directory.attributes, &attributes);

            if(strcmp(directoryName, fullFilename)){
                dirNum++;
                continue;
            }
            
            if(attributes.subdirectory)
                *dirSector = (directory.firstLogicalCluster == 0) ? ROOT_DIR_SECTOR : (directory.firstLogicalCluster+33-2);
            else
                printf("%s nao eh um diretorio\n", fullFilename);
            
            free(fullFilename);
            return;
        }
        if(*dirSector == ROOT_DIR_SECTOR)
            break;

        entry = get_entry(fp, (*dirSector)-33+2);
        if(entry >= 0xFF8)
            break;
        dirNum = 0;
        *dirSector = entry+33-2;
    }
    free(fullFilename);
    return;
}

void remove_cluster_sequence(FILE* fp, int firstLogicalCluster){
    int logicalCluster = firstLogicalCluster;
    int entry = get_entry(fp, logicalCluster);
    //remove_cluster(fp, logicalCluster);

    if(entry == 0x000){
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

void remove_file(FILE* fp, int dirSector, char* filename){
    fat12_dir directory;
    fat12_dir_attr attributes;
    char* fullFilename = (char*)malloc(12);
    int dirNum = 0, maxDirNum, entry;
    unsigned char buffer[32] = {0};

    fat12_dir directory_next;
    to_upper(filename);

    while(1){
        maxDirNum = get_max_directory_size(dirSector);
        while(dirNum < maxDirNum){
            int filename_int = read_directory(fp, dirSector, dirNum, &directory);
            if(filename_int == 0xE5){
                dirNum++;
                continue;
            }
            else if(filename_int == 0x00){
                printf("%s nao encontrado\n", filename);
                break;
            }

            get_full_filename(directory, fullFilename);
            read_attributes(directory.attributes, &attributes);
            
            if(strcmp(filename, fullFilename)){
                dirNum++;
                continue;
            }
            
            if(attributes.subdirectory){
                printf("%s eh um diretorio\n", fullFilename);
            }
            else{
                remove_cluster_sequence(fp, directory.firstLogicalCluster);
                fseek(fp, CLUSTER_SIZE*dirSector + 32*dirNum, SEEK_SET);
                fwrite(&buffer, 32, 1, fp);
                int filename_int_next = read_directory(fp, dirSector, dirNum+1, &directory_next);

                if(filename_int_next != 0x00){
                    short bufferInt = 0xE5;
                    fseek(fp, CLUSTER_SIZE*dirSector + 32*dirNum, SEEK_SET);
                    fwrite(&bufferInt, sizeof(bufferInt), 1, fp);
                }
            }
            break;
        }
        if(dirSector == ROOT_DIR_SECTOR)
            break;

        entry = get_entry(fp, dirSector-33+2);
        if(entry >= 0xFF8)
            break;
        dirNum = 0;
        dirSector = entry+33-2;
    }
    free(fullFilename);
    return;
}

void export_cluster_sequence(FILE* fp, FILE* destFp, int firstLogicalCluster, unsigned char* cluster, int fileSize){
    int logicalCluster = firstLogicalCluster;
    int entry = get_entry(fp, logicalCluster);

    read_cluster(fp, logicalCluster, cluster);

    fwrite(cluster, MIN(fileSize, CLUSTER_SIZE), 1, destFp);
    fileSize -= CLUSTER_SIZE;

    if(entry == 0x000){
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

void export_file(FILE* fp, int dirSector, char* filename, char* destinationFilename){
    fat12_dir directory;
    fat12_dir_attr attributes;
    unsigned char* cluster = (unsigned char*)malloc(CLUSTER_SIZE);
    char* fullFilename = (char*)malloc(12);
    int dirNum = 0, maxDirNum, entry;

    FILE* destFp = fopen(destinationFilename, "wb");
    if(destFp == NULL){
        printf("Nao foi possivel abrir %s\n", destinationFilename);
        return;
    }

    to_upper(filename);

    while(1){
        maxDirNum = get_max_directory_size(dirSector);
        while(dirNum < maxDirNum){
            int filename_int = read_directory(fp, dirSector, dirNum, &directory);
            if(filename_int == 0xE5){
                dirNum++;
                continue;
            }
            else if(filename_int == 0x00){
                printf("%s nao encontrado\n", filename);
                break;
            }

            get_full_filename(directory, fullFilename);
            read_attributes(directory.attributes, &attributes);

            if(strcmp(filename, fullFilename)){
                dirNum++;
                continue;
            }
            
            if(attributes.subdirectory){
                printf("%s eh um diretorio\n", fullFilename);
            }
            else{
                export_cluster_sequence(fp, destFp, directory.firstLogicalCluster, cluster, directory.fileSize); 
            }
            break;
        }
        if(dirSector == ROOT_DIR_SECTOR)
            break;

        entry = get_entry(fp, dirSector-33+2);
        if(entry >= 0xFF8)
            break;
        dirNum = 0;
        dirSector = entry+33-2;
    }
    free(fullFilename);
    free(cluster);
    fclose(destFp);
    return;
}

void import_cluster_sequence(FILE* fp, FILE* destFp, int firstLogicalCluster, unsigned char* cluster, int fileSize){
    int logicalCluster = firstLogicalCluster;
    int entry = get_entry(fp, logicalCluster);
    
    read_cluster(fp, logicalCluster, cluster);

    fwrite(cluster, MIN(fileSize, CLUSTER_SIZE), 1, destFp);
    fileSize -= CLUSTER_SIZE;

    if(entry == 0x000){
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

void import_file(FILE* fp, int dirSector, char* fullFilename, char* sourceFilename){
    fat12_dir directory;
    memset(&directory, 0, sizeof(fat12_dir));
    unsigned char* cluster = (unsigned char*)malloc(CLUSTER_SIZE);
    char* filename = (char*)malloc(32);
    char* extension = (char*)malloc(32);
    int dirNum = 0;

    FILE* srcFp = fopen(sourceFilename, "rb");
    if(srcFp == NULL){
        printf("Nao foi possivel abrir %s\n", sourceFilename);
        return;
    }

    int firstFreeDirectoryNum = get_first_free_directory_entry(fp, dirSector);
    int extFilesize = get_ext_file_size(srcFp);
    int freeSpace = get_free_clusters(fp);

    if(firstFreeDirectoryNum == -1){
        if(dirSector == ROOT_DIR_SECTOR){
            printf("Erro! Sem espaço suficiente no diretorio atual\n");
            return;
        }
        freeSpace = (get_free_clusters(fp)-1)*CLUSTER_SIZE;
        
    }

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


    int firstFreeFatPosition = get_first_free_fat_entry(fp);
    dirSector = get_last_entry(fp, dirSector);
    firstFreeDirectoryNum = get_first_free_directory_entry(fp, dirSector);

    if(firstFreeDirectoryNum == -1){
        write_entry(fp, dirSector-33+2, firstFreeFatPosition);
        write_entry(fp, firstFreeFatPosition, 0xFFF);
        dirSector = firstFreeFatPosition+33-2;
        remove_cluster(fp, firstFreeFatPosition);
        firstFreeDirectoryNum = get_first_free_directory_entry(fp, dirSector);
        firstFreeFatPosition = get_first_free_fat_entry(fp);
    }

    directory.firstLogicalCluster = firstFreeFatPosition;
    directory.fileSize = extFilesize;
    firstFreeDirectoryNum = get_first_free_directory_entry(fp, dirSector);

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    directory.lastWriteDate = directory.creationDate = create_date(tm.tm_mday, tm.tm_mon+1, tm.tm_year-80);
    directory.lastWriteTime = directory.creationTime = create_time(tm.tm_sec, tm.tm_min, tm.tm_hour);

    write_directory(fp, dirSector, firstFreeDirectoryNum, directory);

    int numberOfClustersFile = (extFilesize+CLUSTER_SIZE-1)/CLUSTER_SIZE;

    int end = 0;
    int previousFreeFatPosition = firstFreeFatPosition;
    fseek(srcFp, 0, SEEK_SET);
    while(!end){
        end = read_ext_file(srcFp, cluster, &extFilesize);
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

void print_details(FILE* fp, int dirSector, char* filename){
    fat12_dir directory;
    fat12_dir_attr attributes;
    char* fullFilename = (char*)malloc(12);
    int dirNum = 0, maxDirNum;

    to_upper(filename);
    
    maxDirNum = get_max_directory_size(dirSector);
    while(dirNum < maxDirNum){
        int filename_int = read_directory(fp, dirSector, dirNum, &directory);
        if(filename_int == 0xE5){
            dirNum++;
            continue;
        }
        else if(filename_int == 0x00){
            printf("%s nao encontrado\n", filename);
            break;
        }

        get_full_filename(directory, fullFilename);
        read_attributes(directory.attributes, &attributes);

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

        free(fullFilename);
        return;
    }
}

void format_image(FILE* fp){
    for(int i = 0; i < CLUSTER_SIZE; i++)
        write_entry(fp, i, 0x000);
    unsigned char buffer[32] = {0};
    fseek(fp, ROOT_DIR_SECTOR*CLUSTER_SIZE, SEEK_SET);
    for(int i = 0; i < bootsector.tamanhoRoot; i++){
        fwrite(buffer, 32, 1, fp);
    }
}

void image_analyzer(FILE* fp){
    int fatLivre = get_free_clusters(fp);
    int rootLivre = get_free_root_dir(fp);
    int clustersDadosLivre = fatLivre-(FAT_TABLE_SIZE-bootsector.quantSetoresDisco)+2;
    printf("Tabela FAT: %d/%d (%0.2f%%)\n", FAT_TABLE_SIZE-fatLivre, FAT_TABLE_SIZE, ((float)(FAT_TABLE_SIZE-fatLivre)/(FAT_TABLE_SIZE)*100));
    printf("Diretorio ROOT (%s): %d/%d (%0.2f%%)\n", MOUNT_POINT, bootsector.tamanhoRoot-rootLivre, bootsector.tamanhoRoot, ((float)(bootsector.tamanhoRoot-rootLivre)/(bootsector.tamanhoRoot)*100));
    printf("Area de Dados: %d/%d (%0.2f%%)\n", bootsector.quantSetoresDisco-clustersDadosLivre, bootsector.quantSetoresDisco-33, ((float)(bootsector.quantSetoresDisco-clustersDadosLivre)/(bootsector.quantSetoresDisco-33)*100));
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

    int currentWorkingDirSector = ROOT_DIR_SECTOR;

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
        else if(!strcmp(comando, "q")){
            fclose(arqFat);
            break;
        }
        else if(!strcmp(comando, "det")){
            scanf("%s", argumento);
            print_details(arqFat, currentWorkingDirSector, argumento);
        }
        else if(!strcmp(comando, "imp")){
            scanf("%s", argumento);
            scanf("%s", argumento2);
            import_file(arqFat, currentWorkingDirSector, argumento, argumento2);
        }
        else if(!strcmp(comando, "dsk")){
            image_analyzer(arqFat);
        }
        else if(!strcmp(comando, "format")){
            red();
            printf("TEM CERTEZA QUE QUER FORMATAR A IMAGEM?(S/n)\n");
            reset();
            scanf("%s", argumento);
            to_upper(argumento);
            if(!strcmp(argumento, "S")){
                format_image(arqFat);
                currentWorkingDirSector = ROOT_DIR_SECTOR;    
            }
        }
        else
            printf("Comando desconhecido\n");
    }

    return 0;
}
