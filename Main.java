import java.io.*;
import java.util.Scanner;

public class Main {

    public static long lenFile(String filename) throws IOException {
        return new File(filename).length();
    }

    public static int countWords(String filename, String word) throws IOException {
    int wordCount = 0;
    int wordLen = word.length();

    // Converte String Unicode para bytes brutos (1 char = 1 byte)
    // No C: char[] já é byte nativo
    // Java: precisa explicitar ISO-8859-1 para igualar fgetc() do arquivo, se não não funciona direito com acentos
    byte[] wordBytes = word.getBytes("ISO-8859-1");

    
    try (FileInputStream fis = new FileInputStream(filename)) {
        int b;
        int flag = 0; 
        
        while ((b = fis.read()) != -1) {
            byte currentByte = (byte) b;
            
            // Verifica PRIMEIRA letra
            if (flag == 0 && currentByte == wordBytes[0]) {
                flag = 1; 
            }
          
            else if (flag > 0) {
                if (flag < wordLen && currentByte == wordBytes[flag]) {
                    flag++;  
                    if (flag == wordLen) {
                        wordCount++;
                        flag = 0;  
                    }
                } else {
                    flag = (currentByte == wordBytes[0]) ? 1 : 0;
                }
            }
        }
    }
    return wordCount;
}

    public static void main(String[] args) {
        Scanner scanner = new Scanner(System.in);
        try {
            long lenfile = lenFile("arquivo_texto_rasoavel.txt");
            System.out.println("Tamanho do arquivo: " + lenfile + " bytes");

            System.out.print("Digite a palavra: ");
            String word = scanner.next();

            if (word.length() > 100) {
                word = word.substring(0, 100);
            }

            long startTime = System.nanoTime();
            int count = countWords("arquivo_texto_rasoavel.txt", word);
            long endTime = System.nanoTime();

            System.out.printf("Palavra '%s' ocorre %d vezes%n", word, count);
            double time = (endTime - startTime) / 1_000_000_000.0;
            System.out.printf("Tempo de execução: %.6f s%n", time);

        } catch (IOException e) {
            System.err.println("Erro: " + e.getMessage());
        }
    }
}