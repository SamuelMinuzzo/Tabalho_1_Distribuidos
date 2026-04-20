// gcc -O3 -march=native -flto -fopenmp main.c -o main

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <omp.h>

/** Tamanho da tabela de deslocamento do algoritmo BMH. */
#define TAMANHO_ALFABETO 256
/** Tamanho padrao do bloco de leitura do arquivo. */
#define CHUNK_SIZE (512LL * 1024LL * 1024LL)
/** Tamanho do buffer usado nas leituras de IO. */
#define IO_BUFFER_SIZE (24 * 1024 * 1024)
/** Distancia de prefetch usada na busca no buffer. */
#define PREFETCH_DISTANCE 512
/** Menor quantidade de bytes para habilitar o processamento paralelo por buffer. */
#define MIN_PARALLEL_BYTES (4LL * 1024LL * 1024LL)

#ifdef _WIN32
    #define FSEEK64 _fseeki64
    #define FTELL64 _ftelli64
#else
    #define FSEEK64 fseeko
    #define FTELL64 ftello
#endif
/**
 * Calcula o salto seguro apos um match usando a KMP failure function.
 *
 * Para palavras sem sobreposicao (ex: "abc"), retorna m, permitindo o
 * maximo salto possivel. Para palavras com sobreposicao (ex: "aaaaaa"),
 * retorna o menor salto que nao perde matches sobrepostos.
 *
 * @param palavra Palavra buscada em bytes.
 * @param m Tamanho da palavra em bytes.
 * @return Quantidade de posicoes a avancar apos um match confirmado.
 */

static size_t calcula_salto_pos_match(const unsigned char *palavra, size_t m) {
    if (m == 0) return 1;
    if (m == 1) return 1;

    size_t *falha = calloc(m, sizeof(size_t));
    if (!falha) return 1; 

    falha[0] = 0;
    size_t k = 0;
    for (size_t i = 1; i < m; i++) {
        while (k > 0 && palavra[k] != palavra[i])
            k = falha[k - 1];
        if (palavra[k] == palavra[i])
            k++;
        falha[i] = k;
    }

   
    size_t salto = m - falha[m - 1];
    free(falha);
    return salto;
}

/**
 * Monta a tabela de deslocamentos do algoritmo Boyer-Moore-Horspool.
 *
 * @param palavra Palavra buscada em bytes.
 * @param tamanho_palavra Quantidade de bytes da palavra.
 * @param tabela Tabela de deslocamento a ser preenchida.
 */
static inline void monta_tabela_deslocamento(
    const unsigned char * __restrict__ palavra,
    size_t tamanho_palavra,
    size_t * __restrict__ tabela
) {
    for (size_t i = 0; i < TAMANHO_ALFABETO; ++i) {
        tabela[i] = tamanho_palavra;
    }

    if (tamanho_palavra >= 2) {
        for (size_t i = 0; i < tamanho_palavra - 1; ++i) {
            tabela[palavra[i]] = tamanho_palavra - 1 - i;
        }
    }
}

/**
 * Conta ocorrencias de um unico caractere em um intervalo.
 *
 * @param texto Buffer com o texto a ser analisado.
 * @param inicio Indice inicial do intervalo.
 * @param fim Indice final exclusivo do intervalo.
 * @param c Caractere a ser contado.
 * @return Quantidade de ocorrencias encontradas.
 */
static inline long long conta_um_caractere_intervalo(
    const unsigned char * __restrict__ texto,
    long long inicio,
    long long fim,
    unsigned char c
) {
    long long count = 0;
    const unsigned char * __restrict__ p = texto + inicio;
    const unsigned char * __restrict__ end = texto + fim;

    while (p + 16 <= end) {
        count += (p[0]  == c) + (p[1]  == c) + (p[2]  == c) + (p[3]  == c) +
                 (p[4]  == c) + (p[5]  == c) + (p[6]  == c) + (p[7]  == c) +
                 (p[8]  == c) + (p[9]  == c) + (p[10] == c) + (p[11] == c) +
                 (p[12] == c) + (p[13] == c) + (p[14] == c) + (p[15] == c);
        p += 16;
    }

    while (p < end) {
        count += (*p == c);
        ++p;
    }

    return count;
}

/**
 * Busca BMH em um intervalo de propriedade.
 *
 * A thread e dona dos matches cujo indice inicial esta em:
 * [inicio_propriedade, fim_propriedade)
 *
 * Para nao perder matches na fronteira, a thread pode inspecionar
 * ate fim_propriedade + (m - 1), mas so contabiliza matches cujo
 * inicio pertence ao seu intervalo de propriedade.
 *
 * @param texto Buffer com o texto a ser analisado.
 * @param tamanho_texto Quantidade de bytes validos no buffer.
 * @param palavra Palavra buscada em bytes.
 * @param tamanho_palavra Tamanho da palavra buscada.
 * @param tabela Tabela de deslocamento da BMH.
 * @param inicio_propriedade Inicio do intervalo de responsabilidade da thread.
 * @param fim_propriedade Fim exclusivo do intervalo de responsabilidade da thread.
 * @return Quantidade de ocorrencias encontradas no intervalo.
 */
static inline long long bmh_intervalo_propriedade(
    const unsigned char * __restrict__ texto,
    long long tamanho_texto,
    const unsigned char * __restrict__ palavra,
    size_t tamanho_palavra,
    const size_t * __restrict__ tabela,
    long long inicio_propriedade,
    long long fim_propriedade,
    size_t salto_match
) {
    if (inicio_propriedade >= fim_propriedade || tamanho_texto <= 0) {
        return 0;
    }

    if (tamanho_palavra == 1) {
        return conta_um_caractere_intervalo(
            texto,
            inicio_propriedade,
            fim_propriedade,
            palavra[0]
        );
    }

    if (tamanho_texto < (long long)tamanho_palavra) {
        return 0;
    }

    long long ultimo_inicio_valido = tamanho_texto - (long long)tamanho_palavra;
    if (inicio_propriedade > ultimo_inicio_valido) {
        return 0;
    }

    if (fim_propriedade - 1 > ultimo_inicio_valido) {
        fim_propriedade = ultimo_inicio_valido + 1;
    }

    if (inicio_propriedade >= fim_propriedade) {
        return 0;
    }

    const unsigned char ultimo = palavra[tamanho_palavra - 1];
    const size_t m1 = tamanho_palavra - 1;

    long long count = 0;
    long long i = inicio_propriedade + (long long)m1;

    long long limite_inspecao = fim_propriedade + (long long)m1;
    if (limite_inspecao > tamanho_texto) {
        limite_inspecao = tamanho_texto;
    }

    while (i < limite_inspecao) {
#if defined(__GNUC__) || defined(__clang__)
        if (i + PREFETCH_DISTANCE < tamanho_texto) {
            __builtin_prefetch(&texto[i + PREFETCH_DISTANCE], 0, 1);
        }
#endif

        unsigned char c = texto[i];

        if (c == ultimo) {
            size_t j = m1;
            long long k = i;

            while (j > 0 && texto[k - 1] == palavra[j - 1]) {
                --j;
                --k;
            }

            if (j == 0) {
                long long inicio_match = i - (long long)m1;

                if (inicio_match >= inicio_propriedade &&
                    inicio_match < fim_propriedade) {
                    ++count;
                }

                /* permite ocorrências sobrepostas */
                i += (long long)salto_match;
                continue;
            }
        }

        i += (long long)tabela[c];
    }

    return count;
}

/**
 * Versao sequencial no buffer inteiro.
 *
 * @param texto Buffer com o texto a ser analisado.
 * @param tamanho Buffer com a quantidade de bytes validos.
 * @param palavra Palavra buscada em bytes.
 * @param tamanho_palavra Tamanho da palavra buscada.
 * @param tabela Tabela de deslocamento da BMH.
 * @return Quantidade de ocorrencias encontradas.
 */
static inline long long bmh_sequencial_buffer(
    const unsigned char * __restrict__ texto,
    long long tamanho,
    const unsigned char * __restrict__ palavra,
    size_t tamanho_palavra,
    const size_t * __restrict__ tabela,
    size_t salto_match
) {
    return bmh_intervalo_propriedade(
        texto,
        tamanho,
        palavra,
        tamanho_palavra,
        tabela,
        0,
        tamanho,
        salto_match
    );
}

/**
 * Versao paralela no buffer inteiro.
 *
 * Divide o espaco de possiveis indices iniciais entre as threads.
 * Isso evita perda e dupla contagem nas fronteiras.
 *
 * @param texto Buffer com o texto a ser analisado.
 * @param tamanho Buffer com a quantidade de bytes validos.
 * @param palavra Palavra buscada em bytes.
 * @param tamanho_palavra Tamanho da palavra buscada.
 * @param tabela Tabela de deslocamento da BMH.
 * @param num_threads Quantidade de threads a utilizar.
 * @return Quantidade total de ocorrencias encontradas.
 */
static inline long long bmh_paralelo_buffer(
    const unsigned char * __restrict__ texto,
    long long tamanho,
    const unsigned char * __restrict__ palavra,
    size_t tamanho_palavra,
    const size_t * __restrict__ tabela,
    int num_threads,
    size_t salto_match
) {
    if (tamanho_palavra == 0 || tamanho <= 0) {
        return 0;
    }
    if (tamanho_palavra == 1) {
        long long total = 0;
    
#pragma omp parallel for reduction(+:total) schedule(static) num_threads(num_threads)
        for (int t = 0; t < num_threads; ++t) {
            long long inicio = (tamanho * t) / num_threads;
            long long fim    = (tamanho * (t + 1)) / num_threads;
            total += conta_um_caractere_intervalo(texto, inicio, fim, palavra[0]);
        }

        return total;
    }

    if (tamanho < (long long)tamanho_palavra) {
        return 0;
    }

    if (tamanho < MIN_PARALLEL_BYTES || num_threads <= 1) {
        return bmh_sequencial_buffer(texto, tamanho, palavra, tamanho_palavra, tabela, salto_match);
    }

    long long total = 0;
    long long total_inicios_validos = tamanho - (long long)tamanho_palavra + 1;

#pragma omp parallel for reduction(+:total) schedule(static) num_threads(num_threads)
    for (int t = 0; t < num_threads; ++t) {
        long long inicio_propriedade = (total_inicios_validos * t) / num_threads;
        long long fim_propriedade    = (total_inicios_validos * (t + 1)) / num_threads;

        total += bmh_intervalo_propriedade(
            texto,
            tamanho,
            palavra,
            tamanho_palavra,
            tabela,
            inicio_propriedade,
            fim_propriedade,
            salto_match
        );
    }

    return total;
}

/**
 * Retorna o tamanho do arquivo em bytes.
 *
 * @param arquivo Ponteiro para o arquivo aberto.
 * @return Tamanho do arquivo em bytes, ou -1 em caso de falha.
 */
static long long pega_tamanho_arquivo(FILE *arquivo) {
    long long tamanho = -1;

    if (FSEEK64(arquivo, 0, SEEK_END) == 0) {
        tamanho = (long long)FTELL64(arquivo);
        FSEEK64(arquivo, 0, SEEK_SET);
    }

    return tamanho;
}

/**
 * Aloca buffer com alinhamento manual de 64 bytes.
 *
 * @param tamanho_total Quantidade total de bytes desejada.
 * @param base_ptr Ponteiro de saida para liberar a base original do bloco.
 * @return Ponteiro alinhado para uso, ou NULL em caso de falha.
 */
static unsigned char *aloca_buffer_alinhado(size_t tamanho_total, unsigned char **base_ptr) {
    unsigned char *base = (unsigned char *)malloc(tamanho_total + 64);
    if (!base) {
        *base_ptr = NULL;
        return NULL;
    }

    uintptr_t addr = (uintptr_t)base;
    uintptr_t aligned = (addr + 63u) & ~(uintptr_t)63u;

    *base_ptr = base;
    return (unsigned char *)aligned;
}

/**
 * Conta substrings no arquivo usando leitura em chunks e BMH sequencial.
 *
 * @param arq Caminho do arquivo de entrada.
 * @param palavra Palavra procurada em bytes.
 * @param tamanho_palavra Tamanho da palavra procurada.
 * @return Quantidade de ocorrencias encontradas, ou -1 em caso de erro.
 */
long long conta_substrings_arquivo_bmh_sequencial(
    const char *arq,
    const unsigned char *palavra,
    int tamanho_palavra
) {
    if (!arq || !palavra || tamanho_palavra <= 0) {
        return -1;
    }

    FILE *arquivo = fopen(arq, "rb");
    if (arquivo == NULL) {
        fprintf(stderr, "Erro ao abrir arquivo: %s\n", arq);
        return -1;
    }

    if (setvbuf(arquivo, NULL, _IOFBF, IO_BUFFER_SIZE) != 0) {
        fprintf(stderr, "Aviso: nao foi possivel ajustar o buffer de IO\n");
    }

    size_t m = (size_t)tamanho_palavra;
    size_t sobreposicao = (m > 0) ? (m - 1) : 0;

    size_t tabela[TAMANHO_ALFABETO];
    monta_tabela_deslocamento(palavra, m, tabela);
    size_t salto_match = calcula_salto_pos_match(palavra, m);

    long long tamanho_arquivo = pega_tamanho_arquivo(arquivo);
    if (tamanho_arquivo < 0) {
        fprintf(stderr, "Erro ao obter tamanho do arquivo\n");
        fclose(arquivo);
        return -1;
    }

    long long chunk_real = CHUNK_SIZE;
    if (tamanho_arquivo > 0 && tamanho_arquivo < chunk_real) {
        chunk_real = tamanho_arquivo;
    }

    size_t capacidade_util = (size_t)chunk_real + sobreposicao;

    unsigned char *buffer_base = NULL;
    unsigned char *buffer = aloca_buffer_alinhado(capacidade_util, &buffer_base);
    if (!buffer) {
        fprintf(stderr, "Erro ao alocar memoria\n");
        fclose(arquivo);
        return -1;
    }

    long long total = 0;
    size_t bytes_sobrepostos = 0;

    while (1) {
        size_t bytes_para_ler = (size_t)chunk_real;
        size_t bytes_lidos = fread(buffer + bytes_sobrepostos, 1, bytes_para_ler, arquivo);

        if (bytes_lidos == 0) {
            if (ferror(arquivo)) {
                fprintf(stderr, "Erro durante a leitura do arquivo\n");
                free(buffer_base);
                fclose(arquivo);
                return -1;
            }
            break;
        }

        long long tamanho_valido = (long long)bytes_sobrepostos + (long long)bytes_lidos;

        total += bmh_sequencial_buffer(
            buffer,
            tamanho_valido,
            palavra,
            m,
            tabela,
            salto_match
        );

        if (sobreposicao > 0) {
            if ((size_t)tamanho_valido >= sobreposicao) {
                memmove(buffer, buffer + tamanho_valido - (long long)sobreposicao, sobreposicao);
                bytes_sobrepostos = sobreposicao;
            } else {
                memmove(buffer, buffer, (size_t)tamanho_valido);
                bytes_sobrepostos = (size_t)tamanho_valido;
            }
        } else {
            bytes_sobrepostos = 0;
        }
    }

    free(buffer_base);
    fclose(arquivo);
    return total;
}

/**
 * Conta substrings no arquivo usando leitura em chunks e BMH paralelo.
 *
 * @param arq Caminho do arquivo de entrada.
 * @param palavra Palavra procurada em bytes.
 * @param tamanho_palavra Tamanho da palavra procurada.
 * @param num_threads Quantidade de threads a utilizar.
 * @return Quantidade de ocorrencias encontradas, ou -1 em caso de erro.
 */
long long conta_substrings_arquivo_bmh_paralelo(
    const char *arq,
    const unsigned char *palavra,
    int tamanho_palavra,
    int num_threads
) {
    if (!arq || !palavra || tamanho_palavra <= 0 || num_threads <= 0) {
        return -1;
    }

    FILE *arquivo = fopen(arq, "rb");
    if (arquivo == NULL) {
        fprintf(stderr, "Erro ao abrir arquivo: %s\n", arq);
        return -1;
    }

    if (setvbuf(arquivo, NULL, _IOFBF, IO_BUFFER_SIZE) != 0) {
        fprintf(stderr, "Aviso: nao foi possivel ajustar o buffer de IO\n");
    }

    size_t m = (size_t)tamanho_palavra;
    size_t sobreposicao = (m > 0) ? (m - 1) : 0;

    size_t tabela[TAMANHO_ALFABETO];
    monta_tabela_deslocamento(palavra, m, tabela);
    size_t salto_match = calcula_salto_pos_match(palavra, m);

    long long tamanho_arquivo = pega_tamanho_arquivo(arquivo);
    if (tamanho_arquivo < 0) {
        fprintf(stderr, "Erro ao obter tamanho do arquivo\n");
        fclose(arquivo);
        return -1;
    }

    long long chunk_real = CHUNK_SIZE;
    if (tamanho_arquivo > 0 && tamanho_arquivo < chunk_real) {
        chunk_real = tamanho_arquivo;
    }

    size_t capacidade_util = (size_t)chunk_real + sobreposicao;

    unsigned char *base[2]   = { NULL, NULL };
    unsigned char *buf[2];

    buf[0] = aloca_buffer_alinhado(capacidade_util, &base[0]);
    buf[1] = aloca_buffer_alinhado(capacidade_util, &base[1]);

    if (!buf[0] || !buf[1]) {
        fprintf(stderr, "Erro ao alocar memoria\n");
        free(base[0]); free(base[1]);
        fclose(arquivo);
        return -1;
    }

    long long total = 0;
    int atual = 0;
    size_t bytes_sobrepostos = 0;

    size_t bytes_lidos = fread(buf[atual] + bytes_sobrepostos, 1, (size_t)chunk_real, arquivo);

    while (bytes_lidos > 0) {
        int proximo = 1 - atual;
        long long tamanho_valido = (long long)bytes_sobrepostos + (long long)bytes_lidos;
        size_t lidos_prox = 0;

        if (sobreposicao > 0 && (size_t)tamanho_valido >= sobreposicao) {
            memcpy(buf[proximo], buf[atual] + tamanho_valido - (long long)sobreposicao, sobreposicao);
        }

        #pragma omp parallel sections num_threads(2)
        {
            #pragma omp section
            {
                lidos_prox = fread(buf[proximo] + sobreposicao, 1, (size_t)chunk_real, arquivo);
            }

            #pragma omp section
            {
                total += bmh_paralelo_buffer(  
                    buf[atual], tamanho_valido, palavra, m, tabela, num_threads,salto_match
                );
            }
        }

        if (ferror(arquivo)) {
            fprintf(stderr, "Erro durante a leitura do arquivo\n");
            free(base[0]); free(base[1]);
            fclose(arquivo);
            return -1;
        }

        atual = proximo;
        bytes_sobrepostos = (lidos_prox > 0) ? sobreposicao : 0;
        bytes_lidos = lidos_prox;
    }

    free(base[0]);
    free(base[1]);
    fclose(arquivo);
    return total;
}

/**
 * Ponto de entrada do programa.
 *
 * Espera os argumentos: arquivo, palavra e numero de threads. Executa as
 * contagens sequencial e paralela, compara os resultados e imprime uma linha CSV.
 *
 * @param argc Quantidade de argumentos recebidos.
 * @param argv Vetor com os argumentos da linha de comando.
 * @return 0 em caso de sucesso, ou 1 em caso de erro.
 */
int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <arquivo> <palavra> <num_threads>\n", argv[0]);
        fprintf(stderr, "Exemplo: %s arquivo10GB.txt teste 8\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    const unsigned char *palavra = (const unsigned char *)argv[2];
    int num_threads = atoi(argv[3]);

    if (num_threads <= 0) {
        fprintf(stderr, "Erro: num_threads deve ser maior que zero\n");
        return 1;
    }

    size_t tamanho_palavra = strlen((const char *)palavra);
    if (tamanho_palavra == 0) {
        fprintf(stderr, "Erro: a palavra nao pode ser vazia\n");
        return 1;
    }

    FILE *arquivo = fopen(filename, "rb");
    if (!arquivo) {
        fprintf(stderr, "Erro ao abrir arquivo: %s\n", filename);
        return 1;
    }

    long long tamanho_arquivo = pega_tamanho_arquivo(arquivo);
    fclose(arquivo);

    if (tamanho_arquivo < 0) {
        fprintf(stderr, "Erro ao obter tamanho do arquivo\n");
        return 1;
    }

    double s_inicio = omp_get_wtime();
    long long total_seq = conta_substrings_arquivo_bmh_sequencial(
        filename,
        palavra,
        (int)tamanho_palavra
    );
    double s_fim = omp_get_wtime();

    if (total_seq < 0) {
        return 1;
    }

    double p_inicio = omp_get_wtime();
    long long total_par = conta_substrings_arquivo_bmh_paralelo(
        filename,
        palavra,
        (int)tamanho_palavra,
        num_threads
    );
    double p_fim = omp_get_wtime();

    if (total_par < 0) {
        return 1;
    }

    if (total_seq != total_par) {
        fprintf(stderr,
                "Aviso: contagem sequencial (%lld) e paralela (%lld) diferem.\n",
                total_seq, total_par);
    }

    printf("%s,%s,%zu,%d,%lld,%.9f,%.9f,%lld\n",
           filename,
           palavra,
           tamanho_palavra,
           num_threads,
           tamanho_arquivo,
           s_fim - s_inicio,
           p_fim - p_inicio,
           total_par);

    return 0;
}