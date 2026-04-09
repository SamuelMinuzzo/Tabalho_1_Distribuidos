import java.io.*;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Scanner;

public class Main {

    public static long lenFile(String filename) throws IOException {
        return new File(filename).length();
    }

    public static int countWords(String filename, String word) throws IOException {
        int wordCount = 0;
        
        //Cria os arrays de bytes baseados nos encodings
        byte[][] allEncodings = {
            word.getBytes("ISO-8859-1"),
            word.getBytes("UTF-8"),
            word.getBytes("Cp1252")
        };
        
        List<byte[]> uniqueEncodings = new ArrayList<>();
        for (byte[] enc : allEncodings) {
            boolean isDuplicate = false;
            for (byte[] unique : uniqueEncodings) {
                if (Arrays.equals(enc, unique)) {
                    isDuplicate = true;
                    break;
                }
            }
            if (!isDuplicate && enc.length > 0) {
                uniqueEncodings.add(enc);
            }
        }

        try (RandomAccessFile file = new RandomAccessFile(filename, "r")) {
        
            for (byte[] wordBytes : uniqueEncodings) {
                file.seek(0); 
                int flag = 0;
                int c;
                int len = wordBytes.length;

                while ((c = file.read()) != -1) {
                    if (c == (wordBytes[0] & 0xFF)) {  
                        flag = 1;
                        for (int i = 1; i < len; i++) {
                            c = file.read();
                            if (c == -1 || c != (wordBytes[i] & 0xFF)) {
                                flag = 0;
                                file.seek(file.getFilePointer() - i);  
                                break;
                            }
                        }
                        
                        if (flag == 1) {
                            wordCount++;
                        }
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