#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define TAMANHO_ALFABETO 256
#define CHUNK_SIZE (64 * 1024 * 1024) 

void monta_tabela_deslocamento(char *palavra, int tamanho_palavra, int *tabela) {
    for (int i = 0; i < TAMANHO_ALFABETO; i++) {
        tabela[i] = tamanho_palavra;
    }
    for (int i = 0; i < tamanho_palavra - 1; i++) {
        tabela[(unsigned char)palavra[i]] = tamanho_palavra - 1 - i;
    }
}

long long bmh_no_chunk(char *texto, long long tamanho, char *palavra, int tamanho_palavra, int *tabela) {
    long long count = 0;
    long long i = tamanho_palavra - 1;

    while (i < tamanho) {
        int j = tamanho_palavra - 1;
        long long k = i;

        while (j >= 0 && texto[k] == palavra[j]) {
            j--;
            k--;
        }

        if (j == -1) {
            count++;
            i += 1;
        } else {
            i += tabela[(unsigned char)texto[i]];
        }
    }

    return count;
}

void conta_palavras_bmh(char *palavra, char *arq, int tamanho_palavra) {
    FILE *arquivo = fopen(arq, "rb"); 

    if (arquivo != NULL) {
        printf("Arquivo aberto com sucesso\n");
    } else {
        printf("Deu ruim edilson\n");
        return;
    }

    int tabela[TAMANHO_ALFABETO];
    monta_tabela_deslocamento(palavra, tamanho_palavra, tabela);

    int sobreposicao = tamanho_palavra - 1;
    long long tamanho_buffer = CHUNK_SIZE + sobreposicao;

    char *buffer = (char *)malloc(tamanho_buffer + 1);
    if (!buffer) {
        printf("Erro ao alocar memoria\n");
        fclose(arquivo);
        return;
    }

    long long count = 0;
    long long bytes_lidos;
    long long bytes_sobrepostos = 0; 

    printf("Palavra que sera procurada: %s\n", palavra);

    while (1) {
        bytes_lidos = fread(buffer + bytes_sobrepostos, 1, CHUNK_SIZE, arquivo);

        if (bytes_lidos == 0) break; 

        long long tamanho_valido = bytes_sobrepostos + bytes_lidos;
        buffer[tamanho_valido] = '\0';

        count += bmh_no_chunk(buffer, tamanho_valido, palavra, tamanho_palavra, tabela);

        if (tamanho_valido >= sobreposicao) {
            memmove(buffer, buffer + tamanho_valido - sobreposicao, sobreposicao);
            bytes_sobrepostos = sobreposicao;
        } else {
            bytes_sobrepostos = tamanho_valido;
        }

        if (bytes_lidos < CHUNK_SIZE) break; 
    }

    printf("Total de vezes encontrada: %lld\n", count);

    free(buffer);
    fclose(arquivo);
}

int main() {
    char palavra[100]; 

    printf("Digite a palavra que quer procurar: \n");
    scanf("%s", palavra);

    printf("palavra: %s\n", palavra);

    clock_t ini = clock();
    conta_palavras_bmh(palavra, "arquivo_10_G.txt", strlen(palavra));
    clock_t fim = clock();
    double tempo = (double)(fim - ini) / CLOCKS_PER_SEC;

    printf("Tempo total de execucao: %.12f\n", tempo);

    return 0;
}