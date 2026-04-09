import java.io.*;
import java.util.Scanner;

public class Main {

    public static long lenFile(String filename) throws IOException {
        return new File(filename).length();
    }

    public static int countWords(String filename, String word) throws IOException {
        int wordCount = 0;
        
        byte[] wordBytes = word.getBytes();
        int len = wordBytes.length;

        if (len == 0) return 0;

        try (BufferedInputStream bis = new BufferedInputStream(new FileInputStream(filename))) {
            int c;
            
            while ((c = bis.read()) != -1) {
                
                if (c == (wordBytes[0] & 0xFF)) {  
                    int flag = 1;
                    
                    bis.mark(len);

                    for (int i = 1; i < len; i++) {
                        c = bis.read();
                        if (c == -1 || c != (wordBytes[i] & 0xFF)) {
                            flag = 0;
                            
                            bis.reset();  
                            break;
                        }
                    }
                    
                    if (flag == 1) {
                        wordCount++;
                    }
                }
            }
        }
        return wordCount;
    }

    public static void main(String[] args) {
        Scanner scanner = new Scanner(System.in);
        String filename = "arquivo_texto_grande.txt";
        
        try {
            long lenfile = lenFile(filename);
            System.out.println("Tamanho do arquivo: " + lenfile + " bytes");

            System.out.print("Digite a palavra: ");
            String word = scanner.next();

            if (word.length() > 100) {
                word = word.substring(0, 100);
            }

            long startTime = System.nanoTime();
            int count = countWords(filename, word);
            long endTime = System.nanoTime();

            System.out.printf("Palavra '%s' ocorre %d vezes%n", word, count);
            double time = (endTime - startTime) / 1_000_000_000.0;
            System.out.printf("Tempo de execução: %.6f s%n", time);

        } catch (IOException e) {
            System.err.println("Erro: " + e.getMessage());
        }
    }
}