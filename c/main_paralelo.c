//gcc -O3 -march=native -flto -fopenmp main_paralelo.c -o main_paralelo
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <omp.h>

#define TAMANHO_ALFABETO 256
#define CHUNK_SIZE (512 * 1024 * 1024)
#define IO_BUFFER_SIZE (24 * 1024 * 1024)
#define PREFETCH_DISTANCE 512
#define MIN_PARALLEL_BYTES (4 * 1024 * 1024)

#ifdef _WIN32
#define FSEEK64 _fseeki64
#define FTELL64 _ftelli64
#else
#define FSEEK64 fseeko
#define FTELL64 ftello
#endif

static inline void monta_tabela_deslocamento(const unsigned char * __restrict__ palavra, size_t tamanho_palavra, size_t * __restrict__ tabela) {
    size_t valor_padrao = tamanho_palavra;
    
    for (size_t i = 0; i < TAMANHO_ALFABETO; i += 4) {
        tabela[i] = valor_padrao;
        tabela[i+1] = valor_padrao;
        tabela[i+2] = valor_padrao;
        tabela[i+3] = valor_padrao;
    }
    
    for (int i = (int)tamanho_palavra - 2; i >= 0; i--) {
        tabela[palavra[i]] = tamanho_palavra - 1 - i;
    }
}

static inline long long conta_um_caractere(const unsigned char * __restrict__ texto, long long tamanho, unsigned char c) {
    long long count = 0;
    const unsigned char * __restrict__ pos = texto;
    const unsigned char * __restrict__ fim = texto + tamanho;
    
    long long tamanho_blocos = (tamanho / 16) * 16;
    const unsigned char * __restrict__ fim_blocos = texto + tamanho_blocos;
    
    while (pos < fim_blocos) {
        count += (pos[0] == c) + (pos[1] == c) + (pos[2] == c) + (pos[3] == c) +
                 (pos[4] == c) + (pos[5] == c) + (pos[6] == c) + (pos[7] == c) +
                 (pos[8] == c) + (pos[9] == c) + (pos[10] == c) + (pos[11] == c) +
                 (pos[12] == c) + (pos[13] == c) + (pos[14] == c) + (pos[15] == c);
        pos += 16;
    }
    
    while (pos < fim) {
        count += (*pos == c);
        pos++;
    }
    
    return count;
}

static inline long long bmh_com_prefetch(const unsigned char * __restrict__ texto, long long tamanho, const unsigned char * __restrict__ palavra, size_t tamanho_palavra, const size_t * __restrict__ tabela) {
    if (tamanho_palavra == 1) {
        return conta_um_caractere(texto, tamanho, palavra[0]);
    }

    long long count = 0;
    long long i = tamanho_palavra - 1;
    const unsigned char ultimo = palavra[tamanho_palavra - 1];
    const size_t tamanho_palavra_m1 = tamanho_palavra - 1;

    while (i < tamanho) {
        if (i + PREFETCH_DISTANCE < tamanho) {
            __builtin_prefetch(&texto[i + PREFETCH_DISTANCE], 0, 1);
        }
        
        unsigned char c = texto[i];

        if (c == ultimo) {
            size_t j = tamanho_palavra_m1;
            long long k = i;

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

static inline long long bmh_com_prefetch_paralelo(const unsigned char * __restrict__ texto, long long tamanho, const unsigned char * __restrict__ palavra, size_t tamanho_palavra, const size_t * __restrict__ tabela) {
    if (tamanho_palavra <= 1 || tamanho < (long long)tamanho_palavra || tamanho < MIN_PARALLEL_BYTES) {
        return bmh_com_prefetch(texto, tamanho, palavra, tamanho_palavra, tabela);
    }

    int num_threads = omp_get_max_threads();
    if (num_threads < 1) {
        num_threads = 1;
    }

    long long *divisoes = (long long *)malloc((size_t)(num_threads + 1) * sizeof(long long));
    if (divisoes == NULL) {
        return bmh_com_prefetch(texto, tamanho, palavra, tamanho_palavra, tabela);
    }

    divisoes[0] = 0;
    divisoes[num_threads] = tamanho;

    for (int t = 1; t < num_threads; t++) {
        long long corte = (tamanho * t) / num_threads;

        if (corte < tamanho && isalpha((unsigned char)texto[corte])) {
            while (corte < tamanho && texto[corte] != ' ') {
                corte++;
            }
            if (corte < tamanho) {
                corte++;
            }
        }

        if (corte < divisoes[t - 1]) {
            corte = divisoes[t - 1];
        }

        divisoes[t] = corte;
    }

    long long count = 0;

#pragma omp parallel for reduction(+:count) schedule(static)
    for (int t = 0; t < num_threads; t++) {
        long long ini = divisoes[t];
        long long fim = divisoes[t + 1];
        long long tam_local = fim - ini;

        if (tam_local > 0) {
            count += bmh_com_prefetch(texto + ini, tam_local, palavra, tamanho_palavra, tabela);
        }
    }

    free(divisoes);
    return count;
}

long long pega_tamanho_arquivo(FILE *arquivo) {
    long long tamanho = -1;

    if (FSEEK64(arquivo, 0, SEEK_END) == 0) {
        tamanho = FTELL64(arquivo);
        FSEEK64(arquivo, 0, SEEK_SET);
    }
    printf("Tamanho do arquivo: %lld bytes\n", tamanho);

    return tamanho;
}

long long conta_palavras_bmh(const char *arq, const unsigned char *palavra, int tamanho_palavra) {
    FILE *arquivo = fopen(arq, "rb");

    if (arquivo == NULL) {
        printf("Erro ao abrir arquivo\n");
        return -1;
    }

    if (setvbuf(arquivo, NULL, _IOFBF, IO_BUFFER_SIZE) != 0) {
        printf("Aviso: nao foi possivel ajustar o buffer de IO\n");
    }

    size_t m = (size_t)tamanho_palavra;

    size_t tabela[TAMANHO_ALFABETO];
    monta_tabela_deslocamento(palavra, m, tabela);

    size_t sobreposicao = (m > 0) ? (m - 1) : 0;

    long long tamanho_arquivo = pega_tamanho_arquivo(arquivo);
    long long chunk_real = CHUNK_SIZE;
    if (tamanho_arquivo > 0 && tamanho_arquivo < CHUNK_SIZE) {
        chunk_real = tamanho_arquivo; 
    }

    size_t tamanho_buffer = (size_t)chunk_real + sobreposicao + 64;
    
    unsigned char *buffer_base = (unsigned char *)malloc(tamanho_buffer);
    if (!buffer_base) {
        printf("Erro ao alocar memoria\n");
        fclose(arquivo);
        return -1;
    }
    
    size_t alinhamento_offset = (64 - ((uintptr_t)buffer_base % 64)) % 64;
    unsigned char * __restrict__ buffer = buffer_base + alinhamento_offset;

    long long count = 0;
    size_t bytes_lidos;
    size_t bytes_sobrepostos = 0;

    printf("Arquivo: %s\n", arq);
    printf("Palavra que sera procurada: %s\n", (const char *)palavra);

    while (1) {
        bytes_lidos = fread(buffer + bytes_sobrepostos, 1, (size_t)chunk_real, arquivo);

        if (bytes_lidos == 0) {
            if (ferror(arquivo)) {
                fprintf(stderr, "Erro durante a leitura do arquivo\n");
                free(buffer_base);
                fclose(arquivo);
                return -1;
            }
            break;
        }

        long long tamanho_valido = bytes_sobrepostos + bytes_lidos;

        count += bmh_com_prefetch_paralelo(buffer, tamanho_valido, palavra, m, tabela);

        if (tamanho_valido >= sobreposicao) {
            if (sobreposicao > 0) {
                memcpy(buffer, buffer + tamanho_valido - sobreposicao, sobreposicao);
            }
            bytes_sobrepostos = sobreposicao;
        } else {
            bytes_sobrepostos = tamanho_valido;
        }

    }

    printf("Total de vezes encontrada: %lld\n", count);

    free(buffer_base);
    fclose(arquivo);
    return count;
}

int main() {
    char palavra[100];
    const char *filename = "arquivo_pequeno.txt";

    printf("Digite a palavra que quer procurar: ");
    if (scanf("%99s", palavra) != 1) {
        fprintf(stderr, "Erro ao ler a palavra\n");
        return -1;
    }

    printf("palavra: %s\n", palavra);

    clock_t ini = clock();
    long long count = conta_palavras_bmh(filename, (const unsigned char *)palavra, strlen(palavra));
    clock_t fim = clock();
    double tempo = (double)(fim - ini) / CLOCKS_PER_SEC;

    if (count >= 0) {
        printf("Tempo total de execucao: %.12f segundos\n", tempo);
    }

    return 0;
}