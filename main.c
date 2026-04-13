#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define TAMANHO_ALFABETO 256
#define CHUNK_SIZE (256 * 1024 * 1024)
#define IO_BUFFER_SIZE (8 * 1024 * 1024)

#ifdef _WIN32
#define FSEEK64 _fseeki64
#define FTELL64 _ftelli64
#else
#define FSEEK64 fseeko
#define FTELL64 ftello
#endif

static inline void monta_tabela_deslocamento(const unsigned char *palavra, size_t tamanho_palavra, size_t *tabela) {
    for (size_t i = 0; i < TAMANHO_ALFABETO; i++) {
        tabela[i] = tamanho_palavra;
    }
    for (size_t i = 0; i < tamanho_palavra - 1; i++) {
        tabela[(unsigned char)palavra[i]] = tamanho_palavra - 1 - i;
    }
}

static inline long long conta_um_caractere(const unsigned char *texto, size_t tamanho, unsigned char c) {
    long long count = 0;
    for (size_t i = 0; i < tamanho; i++) {
        count += (texto[i] == c);
    }
    return count;
}

static inline long long bmh_no_chunk(const unsigned char *texto, size_t tamanho, const unsigned char *palavra, size_t tamanho_palavra, const size_t *tabela) {
    if (tamanho_palavra == 1) {
        return conta_um_caractere(texto, tamanho, palavra[0]);
    }

    long long count = 0;
    size_t i = tamanho_palavra - 1;
    const unsigned char ultimo = palavra[tamanho_palavra - 1];

    while (i < tamanho) {
        const unsigned char c = texto[i];

        if (c == ultimo) {
            size_t j = tamanho_palavra - 1;
            size_t k = i;

            while (j > 0 && texto[k - 1] == palavra[j - 1]) {
                j--;
                k--;
            }

            if (j == 0) {
                count++;
                i += 1;
                continue;
            }
        }

        i += tabela[c];
    }

    return count;
}

long long pega_tamanho_arquivo(FILE *arquivo) {
    long long tamanho = -1;

    if (FSEEK64(arquivo, 0, SEEK_END) == 0) {
        tamanho = FTELL64(arquivo);
        FSEEK64(arquivo, 0, SEEK_SET);
    }
    printf("Tamanho do arquivo: %lld\n", tamanho);

    return tamanho;
}

void conta_palavras_bmh(char *palavra, char *arq, int tamanho_palavra) {
    FILE *arquivo = fopen(arq, "rb");

    if (arquivo != NULL) {
        printf("Arquivo aberto com sucesso\n");
    } else {
        printf("Deu ruim edilson\n");
        return;
    }

    if (setvbuf(arquivo, NULL, _IOFBF, IO_BUFFER_SIZE) != 0) {
        printf("Aviso: nao foi possivel ajustar o buffer de IO\n");
    }

    const unsigned char *palavra_u = (const unsigned char *)palavra;
    size_t m = (size_t)tamanho_palavra;

    size_t tabela[TAMANHO_ALFABETO];
    monta_tabela_deslocamento(palavra_u, m, tabela);

    size_t sobreposicao = (m > 0) ? (m - 1) : 0;


    long long tamanho_arquivo = pega_tamanho_arquivo(arquivo);
    long long chunk_real = CHUNK_SIZE;
    if (tamanho_arquivo > 0 && tamanho_arquivo < CHUNK_SIZE) {
        chunk_real = tamanho_arquivo; 
    }

    size_t tamanho_buffer = (size_t)chunk_real + sobreposicao;
    unsigned char *buffer = (unsigned char *)malloc(tamanho_buffer + 1);
    if (!buffer) {
        printf("Erro ao alocar memoria\n");
        fclose(arquivo);
        return;
    }

    long long count = 0;
    size_t bytes_lidos;
    size_t bytes_sobrepostos = 0;

    printf("Palavra que sera procurada: %s\n", palavra);

    while (1) {
        bytes_lidos = fread(buffer + bytes_sobrepostos, 1, (size_t)chunk_real, arquivo);

        if (bytes_lidos == 0) break;

        size_t tamanho_valido = bytes_sobrepostos + bytes_lidos;
        buffer[tamanho_valido] = '\0';

        count += bmh_no_chunk(buffer, tamanho_valido, palavra_u, m, tabela);

        if (tamanho_valido >= sobreposicao) {
            memmove(buffer, buffer + tamanho_valido - sobreposicao, sobreposicao);
            bytes_sobrepostos = sobreposicao;
        } else {
            bytes_sobrepostos = tamanho_valido;
        }

        if (bytes_lidos < chunk_real) break;
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