#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

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

/*função que decompõe a palavra em caracteres individuais e retorna um ponteiro para a string decomposta */
char *decompose_word(const char *word)
{
    if (word == NULL)
    {
        return NULL;
    }
    char *decomposed = malloc(strlen(word) + 1);
    if (decomposed == NULL)
    {
        perror("Erro ao alocar memória");
        return NULL;
    }
    for (int i = 0; i < strlen(word); i++)
    {
        decomposed[i] = word[i];
    }
    decomposed[strlen(word)] = '\0';
    return decomposed;
}

/*função que conta a ocorrência de uma palavra em um arquivo */
int count_words(const char *filename, int *count, char *word)
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


int main()
{
    int word_count = 0;
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
    const char *filename = "arquivo_texto_grande.txt";
    length = len_file(filename);
    printf("\n*************************************\n");
    printf("Tamanho do arquivo: %ld bytes\n", length);
    printf("Palavra a ser contada: %s\n", word);
    clock_t start = clock();
    word_count = count_words(filename, NULL, word);
    clock_t end = clock();

    double time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Tempo de execução: %f segundos\n", time);
    printf("Número de ocorrências: %d\n", word_count);
    printf("*************************************\n");
    return 0;
}