#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>      // Para open()
#include <sys/mman.h>   // Para mmap() e madvise()
#include <sys/stat.h>   // Para fstat() e stat()
#include <unistd.h>     // Para close()
#ifdef __linux__
    #include <sys/mman.h>
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
            if (flag) word_count++;
        }
        
    }
    fclose(file);
    return word_count;
}
/*função que conta a ocorrência de uma palavra em um arquivo usando RAM, ou seja, 
lendo o arquivo inteiro para a memória e depois fazendo a contagem, se o arquivo for muito grande pode causar
lentidão no SO*/
int count_words_RAM(const char *filename, char *word)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        perror("Erro ao abrir o arquivo");
        return -1;
    }
    
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file); 

    char *buffer = (char *)malloc(length);
    if (buffer == NULL)
    {
        perror("Erro ao alocar memória para o arquivo");
        printf("Será necessario executar a versão de contagem sem RAM\n");
        return count_words(filename, word);
        fclose(file);
    }

    fread(buffer, 1, length, file);
    fclose(file); 

    int word_count = 0;
    int len = strlen(word);

    for (long i = 0; i < length; i++)
    {
        if (buffer[i] == word[0])
        {
            int flag = 1;

            for (int j = 1; j < len; j++)
            {
                if (i + j >= length || buffer[i + j] != word[j])
                {
                    flag = 0;
                    break;
                }
            }
    
            if (flag == 1)
            {
                word_count++;
            }
        }
    }

    free(buffer); 
    return word_count;
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
    // lembrar de mudar o nome desses arquivos para a apresentação
    const char *filename = "arquivo10GB.txt";
    length = len_file(filename);
   
    clock_t start = clock();
    word_count = count_words_mmap(filename, word);
    clock_t end = clock();
    double time = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("\n*************************************\n");
    printf("Tamanho do arquivo: %ld bytes\n", length);
    printf("Palavra a ser contada: %s\n", word);
    printf("Tempo de execução: %f segundos\n", time);
    printf("Número de ocorrências: %lld\n", word_count);
    printf("*************************************\n");

    free(word);
    return 0;
}