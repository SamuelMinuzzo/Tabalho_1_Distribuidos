import java.io.*;
import java.nio.channels.FileChannel;
import java.util.Scanner;

// utilizando RandomAccessFile
public class Main {
    public static void main(String[] args) throws IOException {

        String file = "arquivo10GB.txt";
        String word; // palavra a ser contada
        Scanner scanner = new Scanner(System.in);
        System.out.print("Digite a palavra: ");
        word = scanner.next();
        if (word.length() > 100) {
            word = word.substring(0, 100);
        }
        try (RandomAccessFile raf = new RandomAccessFile(file, "rw")) {
            // o FileChannel é utilizado para obter o tamanho do arquivo 
            FileChannel channel = raf.getChannel();
            long fileSize = channel.size(); 
            System.out.println("Tamanho do arquivo: " + fileSize + " bytes");

            // dividindo o arquivo em partes, como o maximo dos inteiros é 2^31-1, utilizei  1GB
            long partSize = 1024L * 1024L * 1024L;
            long numParts = (fileSize + partSize - 1) / partSize;
            long count = 0;
            boolean flag = false; // flag para indicar se a palavra foi encontrada
            int index = 0;
            //System.out.println("numero de partes: " + numParts);
            long startTime = System.nanoTime();
            // a contagem é feita sequencial, até encontrar o primeiro caracter da palavra, depois disso ele liga a flag e verifica toda a palavra se no final da word a flag for 1 então tem uma palavra , depois continua de onde parou
            // como ja foi lido o arquivo em partes, a contagem é feita em cada parte, e depois somada no final, lembrar de 
            // se a palavra for encontrada no final da parte, a contagem continua na próxima parte, para isso é necessário guardar o estado da flag e o indice da palavra que foi encontrada
            for (int i = 0; i < numParts; i++) {
                //System.out.println("lendo parte " + (i + 1) + " de " + numParts);
                long start = i * partSize;
                // o size da parte é o minimo entre o tamanho da parte e o tamanho do arquivo menos o inicio da parte,
                // para evitar ler além do final do arquivo
                if (partSize > fileSize) {
                    partSize = fileSize;
                }else if (start + partSize > fileSize) {
                    partSize = fileSize - start;
                }
                // posiciona o ponteiro do arquivo no inicio da parte 
                raf.seek(start);
                // copia a parte do arquivo para um buffer, para isso é necessário criar um array de bytes 
                //com o tamanho da parte, e ler o arquivo para esse buffer
                byte[] buffer = new byte[(int) partSize];
                raf.read(buffer);
                for (int j = 0; j < buffer.length; j++) {
                    // word.charAt(index) & 0xFF é utilizado para comparar o byte do buffer com o caracter da palavra,
                    // pois o buffer é um array de bytes e a palavra é uma string, 
                    //então é necessário converter o caracter da palavra para byte,
                    // utilizando a operação AND com 0xFF para garantir que o valor seja positivo e
                    // compatível com o byte do buffer
                    if (buffer[j] == (word.charAt(index) & 0xFF))
                    {
                        if (!flag) {
                            flag = true; 
                        }
                        index++; // incrementa o indice da palavra
                        if (index == word.length()) {
                            count++; // palavra completa encontrada, incrementa a contagem
                            flag = false; // reseta a flag
                            index = 0; // reseta o indice
                        }
                    } else {
                        flag = false; // caracter diferente, reseta a flag
                        index = 0; // reseta o indice
                    }
                }
            }
            long endTime = System.nanoTime();
            double time = (endTime - startTime) / 1000000000.0; 
            System.out.println("\n*************************************");
            System.out.println("Tamanho do arquivo: " + fileSize + " bytes");
            System.out.printf("Palavra a ser contada: '%s'%n", word);
            System.out.printf("Tempo de execução: %.6f s%n", time);
            System.out.println("Número de ocorrências: " + count);
            System.out.println("*************************************");
        }
        Exception e = new Exception("Erro ao ler o arquivo");
        System.out.println(e.getMessage());    
    }
}