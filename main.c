/**
 * @file main.c
 * @brief Contador de ocorrências em arquivo gigante usando BMH e otimizações de baixo nível.
 *
 * O programa foi escrito para maximizar throughput sem paralelismo. Ele usa leitura em chunks,
 * tabela de deslocamento do Boyer-Moore-Horspool, alinhamento de cache line e prefetching.
 * 
 * gcc -O3 -march=native -flto main.c -o main.exe
 */

#include <stdio.h>   /**< Entrada e saída padrão. */
#include <stdlib.h>  /**< Alocação de memória e gerenciamento de recursos. */
#include <time.h>    /**< Medição de tempo com clock(). */
#include <string.h>  /**< strlen(), memcpy() e operações de memória. */
#include <stdint.h>  /**< uintptr_t para alinhamento de ponteiros. */

/** Tamanho do alfabeto em bytes. */
#define TAMANHO_ALFABETO 256
/** Tamanho do bloco de leitura do arquivo. */
#define CHUNK_SIZE (512 * 1024 * 1024)
/** Tamanho do buffer interno do FILE*. */
#define IO_BUFFER_SIZE (24 * 1024 * 1024)
/** Distância do prefetch em bytes. */
#define PREFETCH_DISTANCE 512

#ifdef _WIN32
/** Leitura e posicionamento de arquivo em 64 bits no Windows. */
#define FSEEK64 _fseeki64
#define FTELL64 _ftelli64
#else
/** Leitura e posicionamento de arquivo em 64 bits em sistemas Unix. */
#define FSEEK64 fseeko
#define FTELL64 ftello
#endif

/**
 * @brief Monta a tabela de deslocamento do BMH.
 *
 * Cada posição da tabela indica quantos bytes podem ser saltados quando um caractere
 * não casa com o último caractere da palavra procurada.
 *
 * @param palavra Palavra/padrão procurado.
 * @param tamanho_palavra Tamanho da palavra.
 * @param tabela Vetor de saída com 256 posições.
 */
static inline void monta_tabela_deslocamento(const unsigned char * __restrict__ palavra,
                                             size_t tamanho_palavra,
                                             size_t * __restrict__ tabela) {
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

/**
 * @brief Conta ocorrências de um único caractere em um buffer.
 *
 * Este caminho é usado quando a palavra tem tamanho 1. Ele evita o custo do BMH e utiliza
 * loop unrolling para reduzir branches e melhorar o paralelismo interno da CPU.
 *
 * @param texto Buffer de entrada.
 * @param tamanho Tamanho do buffer.
 * @param c Caractere procurado.
 * @return Quantidade de ocorrências encontradas.
 */
static inline long long conta_um_caractere(const unsigned char * __restrict__ texto,
                                           long long tamanho,
                                           unsigned char c) {
    long long count = 0;
    const unsigned char * __restrict__ pos = texto;
    const unsigned char * __restrict__ fim = texto + tamanho;
    
    // Loop principal com unrolling 16x
    long long tamanho_blocos = (tamanho / 16) * 16;
    const unsigned char * __restrict__ fim_blocos = texto + tamanho_blocos;
    
    while (pos < fim_blocos) {
        count += (pos[0] == c) + (pos[1] == c) + (pos[2] == c) + (pos[3] == c) +
                 (pos[4] == c) + (pos[5] == c) + (pos[6] == c) + (pos[7] == c) +
                 (pos[8] == c) + (pos[9] == c) + (pos[10] == c) + (pos[11] == c) +
                 (pos[12] == c) + (pos[13] == c) + (pos[14] == c) + (pos[15] == c);
        pos += 16;
    }
    
    // Restante
    while (pos < fim) {
        count += (*pos == c);
        pos++;
    }
    
    return count;
}

/**
 * @brief Executa a busca Boyer-Moore-Horspool com prefetching.
 *
 * A função compara primeiro o último caractere da palavra e, quando há chance de match,
 * valida o restante em ordem reversa. Quando não há match, aplica o salto da tabela.
 *
 * @param texto Buffer de texto a ser processado.
 * @param tamanho Tamanho do buffer.
 * @param palavra Palavra procurada.
 * @param tamanho_palavra Tamanho da palavra.
 * @param tabela Tabela de deslocamento já construída.
 * @return Número total de ocorrências encontradas.
 */
static inline long long bmh_com_prefetch(const unsigned char * __restrict__ texto,
                                         long long tamanho,
                                         const unsigned char * __restrict__ palavra,
                                         size_t tamanho_palavra,
                                         const size_t * __restrict__ tabela) {
    if (tamanho_palavra == 1) {
        return conta_um_caractere(texto, tamanho, palavra[0]);
    }

    long long count = 0;
    long long i = tamanho_palavra - 1;
    const unsigned char ultimo = palavra[tamanho_palavra - 1];
    const size_t tamanho_palavra_m1 = tamanho_palavra - 1;

    while (i < tamanho) {
        /* Prefetching para reduzir cache miss em leitura sequencial. */
        if (i + PREFETCH_DISTANCE < tamanho) {
            __builtin_prefetch(&texto[i + PREFETCH_DISTANCE], 0, 1);
        }
        
        unsigned char c = texto[i];

        if (c == ultimo) {
            size_t j = tamanho_palavra_m1;
            long long k = i;

            /* Comparação backward do padrão quando o último caractere bate. */
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
    printf("Tamanho do arquivo: %lld bytes\n", tamanho);

    return tamanho;
}

/**
 * @brief Conta as ocorrências da palavra no arquivo usando leitura em chunks.
 *
 * O arquivo é lido em blocos grandes para reduzir chamadas ao sistema. Cada bloco preserva uma
 * sobreposição de tamanho_palavra - 1 bytes para não perder ocorrências que cruzam a borda entre
 * dois chunks.
 *
 * @param arq Caminho do arquivo.
 * @param palavra Palavra procurada.
 * @param tamanho_palavra Tamanho da palavra.
 * @return Número total de ocorrências, ou -1 em caso de erro.
 */
long long conta_palavras_bmh(const char *arq, const unsigned char *palavra, int tamanho_palavra) {
    FILE *arquivo = fopen(arq, "rb");

    if (arquivo == NULL) {
        printf("Erro ao abrir arquivo\n");
        return -1;
    }

    /* Buffer I/O grande para favorecer leitura sequencial. */
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
    
    /* Alocação com espaço extra para alinhamento em cache line. */
    unsigned char *buffer_base = (unsigned char *)malloc(tamanho_buffer);
    if (!buffer_base) {
        printf("Erro ao alocar memoria\n");
        fclose(arquivo);
        return -1;
    }
    
    /* Alinhamento de 64 bytes para reduzir custo de acesso desalinhado. */
    size_t alinhamento_offset = (64 - ((uintptr_t)buffer_base % 64)) % 64;
    unsigned char * __restrict__ buffer = buffer_base + alinhamento_offset;

    long long count = 0;
    size_t bytes_lidos;
    size_t bytes_sobrepostos = 0;

    printf("Arquivo: %s\n", arq);
    printf("Palavra que sera procurada: %s\n", (const char *)palavra);

    while (1) {
        bytes_lidos = fread(buffer + bytes_sobrepostos, 1, (size_t)chunk_real, arquivo);

        if (bytes_lidos == 0) break;

        long long tamanho_valido = bytes_sobrepostos + bytes_lidos;

        count += bmh_com_prefetch(buffer, tamanho_valido, palavra, m, tabela);

        if (tamanho_valido >= sobreposicao) {
            /* Copia somente a sobreposição para o início do buffer. */
            if (sobreposicao > 0) {
                memcpy(buffer, buffer + tamanho_valido - sobreposicao, sobreposicao);
            }
            bytes_sobrepostos = sobreposicao;
        } else {
            bytes_sobrepostos = tamanho_valido;
        }

        if (bytes_lidos < chunk_real) break;
    }

    printf("Total de vezes encontrada: %lld\n", count);

    free(buffer_base);
    fclose(arquivo);
    return count;
}

/**
 * @brief Ponto de entrada do programa.
 *
 * Lê a palavra informada pelo usuário, mede o tempo de execução e chama a rotina principal de busca.
 *
 * @return 0 em caso de sucesso; valor negativo em caso de erro de entrada.
 */
int main() {
    char palavra[100];
    const char *filename = "arquivo_10_G.txt";

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