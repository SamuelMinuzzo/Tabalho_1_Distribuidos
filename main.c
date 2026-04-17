/*
 * Uso: ./main.exe <arquivo> <palavra> <num_threads>

 * gcc -O3 main.c -o main -fopenmp
 */

 //  

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <fcntl.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif 

/* Número de threads configurado a partir do parâmetro de entrada. */
int num_threads = 0;

/**
 * len_file
 * --------
 * Retorna o tamanho do arquivo em bytes.
 * Em Windows a função usa API de arquivos nativa,
 * em Unix usa fopen/fseek/ftell para obter o tamanho.
 */
long long len_file(const char *filename)
{
#ifdef _WIN32

    HANDLE hFile = CreateFileA(
        filename,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        perror("Erro ao abrir arquivo");
        return -1;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        perror("Erro ao obter tamanho");
        return -1;
    }

    CloseHandle(hFile);
    return fileSize.QuadPart;

#else

    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Erro ao abrir o arquivo");
        return -1;
    }

    fseek(file, 0, SEEK_END);
    long long length = ftell(file);
    fclose(file);

    return length;

#endif
}
/**
 * count_words_mmap
 * ----------------
 * Realiza a contagem de ocorrências usando mmap (ou MapViewOfFile no Windows).
 * O arquivo é mapeado na memória e percorrido byte a byte para buscar a palavra.
 * A função retorna o número total de ocorrências encontradas.
 *
 * Em Windows, usa MapViewOfFile; em Unix, usa mmap + madvise.
 */
long long count_words_mmap(const char *filename, char *word)
{
#ifdef _WIN32
    HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, 
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        perror("Erro ao abrir arquivo");
        return -1;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return -1;
    }

    long long length = fileSize.QuadPart;
    size_t word_len = strlen(word);

    if (word_len == 0 || length < (long long)word_len) {
        CloseHandle(hFile);
        return 0;
    }

    HANDLE hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (hMap == NULL) {
        CloseHandle(hFile);
        return -1;
    }

    char *data = (char *)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (data == NULL) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return -1;
    }

    long long word_count = 0;
    int index = 0;
    int flag = 0;

    for (long long i = 0; i < length; i++) {
        if (data[i] == word[index]) {
            if (!flag) {
                flag = 1;
            }
            index++;

            if (index == word_len) {
                word_count++;
                flag = 0;
                index = 0;
            }
        } else {
            flag = 0;
            index = 0;
        }
    }

    UnmapViewOfFile(data);
    CloseHandle(hMap);
    CloseHandle(hFile);

    return word_count;

#else
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Erro ao abrir o arquivo");
        return -1;
    }

    struct stat st;
    fstat(fd, &st);
    long length = st.st_size;
    size_t word_len = strlen(word);

    if (word_len == 0) {
        close(fd);
        return 0;
    }

    char *data = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("Erro no mmap");
        close(fd);
        return -1;
    }

#ifdef __unix__
    madvise(data, length, MADV_SEQUENTIAL);
#endif

    long long word_count = 0;
    int index = 0;
    int flag = 0;

    for (long long i = 0; i < length; i++) {
        if (data[i] == word[index]) {
            if (!flag) {
                flag = 1;
            }
            index++;

            if (index == word_len) {
                word_count++;
                flag = 0;
                index = 0;
            }
        } else {
            flag = 0;
            index = 0;
        }
    }

    munmap(data, length);
    close(fd);
    return word_count;
#endif
}
 /**
 * count_words_mmap_openMP
 * -----------------------
 * Realiza a contagem paralela de ocorrências usando mmap + OpenMP.
 * O arquivo é dividido em chunks por thread e cada chunk busca a palavra
 * dentro de sua região. A sobreposição mínima (word_len-1) garante que
 * ocorrências que atravessam fronteiras não sejam perdidas.
 *
 * Em Windows, usa MapViewOfFile; em Unix, usa mmap + madvise.
 */
long long count_words_mmap_openMP(const char *filename, char *word)
{
#ifdef _WIN32
    HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, 
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        perror("Erro ao abrir arquivo");
        return -1;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return -1;
    }

    long long length = fileSize.QuadPart;
    size_t word_len = strlen(word);

    if (word_len == 0 || length < (long long)word_len) {
        CloseHandle(hFile);
        return 0;
    }

    HANDLE hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (hMap == NULL) {
        CloseHandle(hFile);
        return -1;
    }

    char *data = (char *)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (data == NULL) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return -1;
    }

    long long chunk_size = length / num_threads;
    long long total_word_count = 0;

#pragma omp parallel for reduction(+:total_word_count) schedule(static)
    for (int i = 0; i < num_threads; i++) {
        long long start = i * chunk_size;
        long long end = (i == num_threads - 1) ? length : (i + 1) * chunk_size;
        long long search_limit = (i == num_threads - 1) ? end : end + (word_len - 1);
        if (search_limit > length) search_limit = length;

        long long local_count = 0;
        int index = 0;
        int flag = 0;

        for (long long j = start; j < search_limit; j++) {
            if (data[j] == word[index]) {
                if (!flag) {
                    flag = 1;
                }
                index++;

                if (index == word_len) {
                    // Conta somente matches cujo início pertence ao chunk original
                    if (j - word_len + 1 < end) {
                        local_count++;
                    }
                    flag = 0;
                    index = 0;
                }
            } else {
                flag = 0;
                index = 0;
            }
        }

        total_word_count += local_count;
    }

    UnmapViewOfFile(data);
    CloseHandle(hMap);
    CloseHandle(hFile);

    return total_word_count;

#else
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Erro ao abrir o arquivo");
        return -1;
    }

    struct stat st;
    fstat(fd, &st);
    long length = st.st_size;
    size_t word_len = strlen(word);

    if (word_len == 0 || length < (long)word_len) {
        close(fd);
        return 0;
    }

    char *data = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("Erro no mmap");
        close(fd);
        return -1;
    }

#ifdef __unix__
    madvise(data, length, MADV_SEQUENTIAL);
    madvise(data, length, MADV_WILLNEED);
#endif

    long long chunk_size = length / num_threads;
    long long total_word_count = 0;

#pragma omp parallel for reduction(+:total_word_count) schedule(static)
    for (int i = 0; i < num_threads; i++) {
        long long start = i * chunk_size;
        long long end = (i == num_threads - 1) ? length : (i + 1) * chunk_size;
        long long search_limit = (i == num_threads - 1) ? end : end + (word_len - 1);
        if (search_limit > length) search_limit = length;

        long long local_count = 0;
        int index = 0;
        int flag = 0;

        for (long long j = start; j < search_limit; j++) {
            if (data[j] == word[index]) {
                if (!flag) {
                    flag = 1;
                }
                index++;

                if (index == word_len) {
                    if (j - word_len + 1 < end) {
                        local_count++;
                    }
                    flag = 0;
                    index = 0;
                }
            } else {
                flag = 0;
                index = 0;
            }
        }

        total_word_count += local_count;
    }

    munmap(data, length);
    close(fd);
    return total_word_count;
#endif
}
/**
 * main
 * ----
 * Captura os parâmetros de linha de comando, inicializa o número de threads
 * e mede o tempo de execução das versões sequencial e paralela da busca.
 * A saída é impressa em formato CSV para facilitar análise de desempenho.
 */
int main(int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <arquivo> <palavra> <num_threads>\n", argv[0]);
        return -1;
    }

    const char *filename = argv[1];
    char *word = argv[2];
    int threads_para_teste = atoi(argv[3]);
    
    omp_set_num_threads(threads_para_teste);
    num_threads = threads_para_teste; 

    long long length = len_file(filename);
    if (length < 0) return -1;


    double s_start = omp_get_wtime();
    long long s_count = count_words_mmap(filename, word);
    double s_end = omp_get_wtime();
    double s_time = s_end - s_start;

    double p_start = omp_get_wtime();
    long long p_count = count_words_mmap_openMP(filename, word);
    double p_end = omp_get_wtime();
    double p_time = p_end - p_start;

    printf("%s,%s,%zu,%d,%lld,%.6f,%.6f,%lld\n", 
            filename, word, strlen(word), threads_para_teste, 
            length, s_time, p_time, p_count);

    return 0;
}