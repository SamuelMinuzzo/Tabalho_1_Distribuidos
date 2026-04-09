#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

void buffer_circular(int n, char *b){
    for (int j = 0; j < n - 1; j++) {
        b[j] = b[j + 1];
    }
}

void ini_buffer(int n, char *b){
    for(int i = 0; i < n; i++){
        b[i] = '\0';
    }
}

int compara_palavra(char *a, char *b, int n){
    for(int i = 0; i < n; i++){
        if(a[i] != b[i]){
            return 0;
        }
    }

    return 1;
}

void conta_palavras_sequencial(char *palavra, char *arq, int tamanho_palavra){
    FILE *arquivo = fopen(arq, "r");

    char buffer[tamanho_palavra + 1];
    int count = 0,c = 0;

    ini_buffer(tamanho_palavra, buffer);

    if (arquivo != NULL){
        printf("Arquivo aberto com sucesso\n");
    }
    else{
        printf("Deu ruim edilson\n");
        return;
    }

    printf("Palavra que sera procurada %s\n", palavra);

    while((c = fgetc(arquivo)) != EOF){
        //printf("%c", c); não descomente favor
        buffer_circular(tamanho_palavra,buffer);
        buffer[tamanho_palavra - 1] = c;

        if(compara_palavra(buffer,palavra, tamanho_palavra)){
            count++;
        }

    }

    printf("Total de vezes encontrada: %d\n", count);

    fclose(arquivo);

}

int main(){

    char palavra[100]; //seguinte não pode passar disso, se não nem ideia de como resolve

    printf("Digite a palavra que quer procurar: \n");
    scanf("%s", palavra);

    printf("palavra: %s\n", palavra);

    clock_t ini = clock();
    conta_palavras_sequencial(palavra, "arquivo_teste.txt", strlen(palavra));
    clock_t fim = clock();
    double tempo = (double)(fim - ini) / CLOCKS_PER_SEC;

    printf("Tempo total de execucao: %.12f\n", tempo);

    return 0;
}