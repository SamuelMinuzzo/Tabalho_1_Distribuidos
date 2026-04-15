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


/*função que retorna o tamanho do arquivo em bytes */
long int len_file(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        perror("Erro ao abrir o arquivo");
        return -1;
    }
    fseek(file, 0, SEEK_END);
    long int length = ftell(file);
    fclose(file);
    return length;
}
/*função que conta a ocorrência de uma palavra em um arquivo */
int count_words(const char *filename, char *word)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        perror("Erro ao abrir o arquivo");
        return -1;
    }
    int flag = 0;
    int c;
    int word_count = 0;
    int len = strlen(word);

    while ((c = fgetc(file))!= EOF)
    {
        if (c == (unsigned char)word[0]) {  
            flag = 1;
            for (int i = 1; i < len; i++) {
                c = fgetc(file);
                if (c == EOF || c != (unsigned char)word[i]) {
                    flag = 0;
                    fseek(file, -(i), SEEK_CUR);  
                    break;
                }
            }
            if (flag)
            {
                word_count++;
            }
        }
        
    }
    fclose(file);
    return word_count;
}

long long count_words_openMP(const char *filename, const char *word)
{
    long length = len_file(filename);
    size_t word_len = strlen(word);

    if (word_len == 0 || length < (long)word_len)
    {
        return 0;
    }

    int num_threads = omp_get_max_threads();
    long chunk_size = length / num_threads;
    long long total_count = 0;

    #pragma omp parallel for reduction(+:total_count) schedule(static)
    for (int t = 0; t < num_threads; t++)
    {
        FILE *file = fopen(filename, "rb");
        if(file == NULL)
        {
            perror("Erro ao abrir o arquivo");
            return -1;
        }

        long start = t * chunk_size;
        long end = (t == num_threads - 1) ? length : (t + 1) * chunk_size;
        long search_end = end + word_len - 1;
        if (search_end > length)
            search_end = length;

        fseek(file, start, SEEK_SET);

        long pos = start;
        int c;

        while (pos <= search_end - word_len &&
               (c = fgetc(file)) != EOF)
        {
            if (c == (unsigned char)word[0])
            {
                int flag = 1;
                for (size_t i = 1; i < word_len; i++)
                {
                    c = fgetc(file);
                    if (c == EOF || c != (unsigned char)word[i])
                    {
                        flag = 0;
                        fseek(file, -(long)i, SEEK_CUR);
                        break;
                    }
                }

                if (flag)
                {
                    total_count++;
                }
            }
            pos++;
        }
        fclose(file);
    }
    return total_count;
}

/* * FUNÇÃO: count_words_mmap
 * ------------------------
 * Realiza a contagem de ocorrências de uma palavra utilizando Mapeamento de Memória (mmap).
 * Esta abordagem é significativamente mais eficiente que a leitura tradicional (fread/RAM)
 * por utilizar as seguintes estratégias de baixo nível:
 *
 * 1. MAPEAMENTO DE MEMÓRIA (mmap):
 * Em vez de alocar um buffer manual e copiar os dados do disco, o mmap 
 * projeta o arquivo diretamente no espaço de endereçamento virtual do processo. 
 * O Sistema Operacional carrega os dados sob demanda, evitando 
 * o uso excessivo de RAM física e permitindo processar arquivos maiores que a própria RAM.
 *
 * 2. OTIMIZAÇÃO DE KERNEL (madvise):
 * Informa ao Kernel que o acesso aos dados será sequencial (MADV_SEQUENTIAL). 
 * Isso ativa o 'Read-Ahead' no hardware, fazendo com que o SO pré-carregue os 
 * próximos blocos do disco para o cache antes mesmo do programa solicitá-los.
 * Infelizmente, o madvise não é suportado no Windows, mas o sistema de arquivos do Windows
 * é otimizado para acesso sequencial por padrão.
 *
 * 3. BUSCA VETORIAL (memchr + SIMD):
 * Utiliza a função memchr para localizar o primeiro caractere da palavra. 
 * Internamente, memchr utiliza instruções SIMD (Single Instruction, Multiple Data), 
 * como SSE (Streaming SIMD Extensions) ou AVX (Advanced Vector Extensions), que permitem ao processador
 * comparar múltiplos bytes (16, 32 ou 64) em um único ciclo de clock, "saltando" pelo texto em alta velocidade.
 *
 * 4. COMPARAÇÃO OTIMIZADA (memcmp):
 * Uma vez encontrado o primeiro caractere, memcmp valida o restante da palavra. 
 * Diferente de um loop manual, memcmp é otimizado em nível de Assembly para 
 * comparar blocos de memória usando a largura total do barramento de dados.
 *
 * 5. TRATAMENTO DE SOBREPOSIÇÃO:
 * O ponteiro avança de 1 em 1 após cada encontro (p++), permitindo a contagem 
 * correta de padrões sobrepostos (ex: encontrar "aaa" 2 vezes dentro de "aaaa").
 *
 * RETORNO: 
 * Número total de ocorrências (long long) ou chama count_words (fallback) caso 
 * o mapeamento falhe por limitações de arquitetura (ex: 32 bits).
 */
long long count_words_mmap(const char *filename, char *word)
{
    #ifdef _WIN32
        printf("[Windows] mmap indisponível -> usando fallback sequencial.\n");
        return count_words(filename, word);

    #else
        /* a função open retorna um indice inteiro na tabela do kernel */
        int fd = open(filename, O_RDONLY);     if (fd == -1) { //*a diretiva O_RDONLY indica que o arquivo será aberto apenas para leitura
            perror("Erro ao abrir o arquivo");
            return -1;
        }

        // Obtendo o tamanho do arquivo usando fstat, que é mais eficiente do que abrir o arquivo e usar fseek/ftell
        struct stat st;
        fstat(fd, &st);
        long  length = st.st_size;
        size_t word_len = strlen(word);

        if (word_len == 0) {
            close(fd);
            return 0;
        }

        /* Mapeando o arquivo na memória, a função mmap retorna um ponteiro para o início do mapeamento,
        ou MAP_FAILED em caso de erro, os paramentros são:
        NULL: o sistema escolhe o endereço de mapeamento
        length: tamanho do mapeamento
        PROT_READ: permissão de leitura
        MAP_PRIVATE: mapeamento privado
        fd: descriptor do arquivo
        0: deslocamento no arquivo */
        char *data = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data == MAP_FAILED) {
            perror("Erro no mmap. Tentando versão clássica...");
            close(fd);
            return count_words(filename, (char *)word); 
        }

        #ifdef __unix__ 
            madvise(data, length, MADV_SEQUENTIAL);
        #elif defined(_WIN32)
        #endif

        long long word_count = 0;
        char *p = data;
        char *end = data + length;

        // Lógica de busca otimizada usando memchr e memcmp
        while (p <= end - word_len) {
            // memchr é extremamente rápido (usa instruções SIMD do processador)
            /* a funcao memchr procura a primeira ocorrência do primeiro caractere da palavra (word[0]) no buffer,
            começando do ponteiro p e limitando a busca até end - word_len + 1 para garantir que haja espaço suficiente 
            para comparar a palavra completa. Se memchr encontrar o caractere, ele retorna um ponteiro para essa posição; caso contrário, retorna NULL. Isso é muito eficiente porque pode usar otimizações de hardware para acelerar a busca.
            */
            p = memchr(p, word[0], end - p - word_len + 1);
            
            if (!p) break;

            // Compara o restante da palavra
            if (memcmp(p + 1, word + 1, word_len - 1) == 0) {
                word_count++;
            }
            p++; // Incrementa para permitir sobreposição
        }

        munmap(data, length);
        close(fd);
        return word_count;
    #endif
}


long long count_words_mmap_openMP(const char *filename, char *word)
{
    #ifdef _WIN32
        printf("[Windows] mmap indisponível -> usando fallback sequencial.\n");
        return count_words_openMP(filename, word);
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
            perror("Erro no mmap. Tentando versão clássica...");
            close(fd);
            return count_words(filename, word); 
        }

        #ifdef __unix__ 
            madvise(data, length, MADV_SEQUENTIAL);
            madvise(data, length, MADV_WILLNEED);
        #endif
        /*Calcula o número de threads e o tamanho de cada chunk*/
        int num_threads = omp_get_max_threads(); // retorna o número máximo de threads disponíveis para o programa, que geralmente é igual ao número de núcleos da CPU
        long long chunk_size = length / num_threads; // divide o arquivo em partes iguais para cada thread processar
        long long total_word_count = 0; 
        /* variável compartilhada para acumular o total de ocorrências, a diretiva reduction(+:total_word_count) 
        garante que cada thread tenha sua própria cópia local da variável e que os resultados sejam somados 
        corretamente no final da execução*/
        

        // PARALELIZAÇÃO DO PROCESSAMENTO
        #pragma omp parallel for reduction(+:total_word_count) schedule(static)
        for (int i = 0; i < num_threads; i++) {
            // int i = omp_get_thread_num(); // retorna o ID da thread atual, que varia de 0 a num_threads-1
            long long start = i * chunk_size; // calcula o início do chunk para a thread atual
            
            // A última thread vai até o fim real do arquivo
            long long end = (i == num_threads - 1) ? length : (i + 1) * chunk_size;
            
            // RESOLUÇÃO DO CORTE: 
            // Cada thread olha um pouco além do seu limite (word_len - 1)
            // para capturar palavras que "atravessam" a fronteira dos blocos.
            long long search_limit = (i == num_threads - 1) ? end : end + (word_len - 1);
            
            // Garante que não ultrapassamos o tamanho total do arquivo
            if (search_limit > length) search_limit = length;

            char *p = data + start;
            char *ptr_end = data + search_limit;

            while (p <= ptr_end - word_len) {
                p = memchr(p, word[0], ptr_end - p - word_len + 1);
                if (!p) break;

                if (memcmp(p + 1, word + 1, word_len - 1) == 0) {
                    total_word_count++;
                }
                p++; 
            }
        }

        munmap(data, length);
        close(fd);
        return total_word_count;
    #endif
}


int main()
{
    long long word_count = 0;
    char *word = malloc(101 * sizeof(char));
    if (word == NULL)
    {
        perror("Erro ao alocar memória");
        return -1;
    }
    /* Primeira versão vai premitir apenas palavras sem espaços, basicamente faz o truncamento no espaço
     e/ou quando passa do limite de 100 caracteres, mas isso tem que ser melhorado, pois ainda não avisa o usuário */

    printf("Digite a palavra a ser contada: ");
    if (scanf("%100s", word) != 1)
    {
        fprintf(stderr, "Erro ao ler a palavra\n");
        free(word);
        return -1;
    }

    long length = 0;
    double time = 0;
    double start, end;
    // lembrar de mudar o nome desses arquivos para a apresentação
    const char *filename = "arquivo10GB.txt";
    length = len_file(filename);
    printf("threads disponíveis: %d\n", omp_get_max_threads());
    for(int i=1; i<=2; i++){
        if(i == 1){
            start = omp_get_wtime();
            word_count = count_words_mmap(filename, word);
            end = omp_get_wtime();
           
        }else{
            start = omp_get_wtime();
            word_count = count_words_mmap_openMP(filename, word);
            end = omp_get_wtime();
        
        }
        time = ((double)(end - start));
        printf("\n*************************************\n");
        printf("Versão: 1-sequencial, 2-paralela\n");
        printf("Versão escolhida: %d\n", i);
        printf("Tamanho do arquivo: %ld bytes\n", length);
        printf("Palavra a ser contada: %s\n", word);
        printf("Tempo de execução: %f segundos\n", time);
        printf("Número de ocorrências: %lld\n", word_count);
        printf("*************************************\n");
       
    }

    free(word);
    return 0;
}