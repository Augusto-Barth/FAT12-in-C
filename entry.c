#include "entry.h"

int fatTableSector[2] = {FAT_TABLE1_SECTOR, FAT_TABLE2_SECTOR};

short get_entry(FILE* fp, int position){
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

void write_entry(FILE* fp, int position, short entry){
    //printf("Writing entry: %d (%X)\n", position, entry);
    unsigned char full, half, buffer;
    for(int curTable = 0; curTable < 2; curTable++){
        if(position%2 == 0){
            fseek(fp, CLUSTER_SIZE*fatTableSector[curTable] + (3*position)/2+1, SEEK_SET);
            fread(&half, 1, 1, fp);
            buffer = half&0b11110000;
            buffer |= (entry&0b111100000000) >> 8;
            fseek(fp, CLUSTER_SIZE*fatTableSector[curTable] + (3*position)/2+1, SEEK_SET);
            fwrite(&buffer, 1, 1, fp);

            fseek(fp, CLUSTER_SIZE*fatTableSector[curTable] + (3*position)/2, SEEK_SET);
            fread(&full, 1, 1, fp);
            buffer = full&0b00000000;
            buffer |= (entry&0b000011111111);
            fseek(fp, CLUSTER_SIZE*fatTableSector[curTable] + (3*position)/2, SEEK_SET);
            fwrite(&buffer, 1, 1, fp);
        }
        else{
            fseek(fp, CLUSTER_SIZE*fatTableSector[curTable] + (3*position)/2, SEEK_SET);
            fread(&half, 1, 1, fp);
            buffer = half&0b00001111;
            buffer |= (entry&0b000000001111) << 4;
            fseek(fp, CLUSTER_SIZE*fatTableSector[curTable] + (3*position)/2, SEEK_SET);
            fwrite(&buffer, 1, 1, fp);

            fseek(fp, CLUSTER_SIZE*fatTableSector[curTable] + (3*position)/2+1, SEEK_SET);
            fread(&full, 1, 1, fp);
            buffer = full&0b00000000;
            buffer |= (entry&0b111111110000) >> 4;
            fseek(fp, CLUSTER_SIZE*fatTableSector[curTable] + (3*position)/2+1, SEEK_SET);
            fwrite(&buffer, 1, 1, fp);
        }
    }
}

int get_first_free_fat_entry(FILE* fp){
    int entry;
    for(int i = 0; i < CLUSTER_SIZE; i++){
        entry = get_entry(fp, i);
        if(entry == 0x000)
            return i;
    }
    return -1;
}

void print_fat_table(FILE* fp){
    for(int i = 0; i < CLUSTER_SIZE; i++){
        if(i % 8 == 0 && i != 0)
            printf("\n");
        printf("%03X ", get_entry(fp, i));
    }
    printf("\n");
}