// javac Main.java
// java Main arquivoXGB.txt "palavra" n_threadsimport java.io.File;

import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.util.Locale;
import java.util.concurrent.CyclicBarrier;
import java.util.concurrent.Semaphore;

public class Main {

    private static long totalOcorrencias = 0;
    private static Semaphore semaforoDisco;
    // Definindo 16MB para os buffers
    private static final int TAMANHO_BUFFER = 16 * 1024 * 1024; 
    public static void main(String[] args) {
        Locale.setDefault(Locale.US);
        if (args.length < 3) {
            System.err.printf("Uso: java Main <arquivo> <palavra> <num_threads>\n");
            System.exit(-1);
        }

        String filename = args[0];
        String word = args[1];
        int threads_para_teste = Integer.parseInt(args[2]);

        File file = new File(filename);
        if (!file.exists()) {
            System.err.println("Erro ao abrir arquivo.");
            System.exit(-1);
        }
        long length = file.length();

        try {
            long s_start_nano = System.nanoTime();
            long s_count = countWordsSequential(filename, word);
            long s_end_nano = System.nanoTime();
            double s_time = (s_end_nano - s_start_nano) / 1e9; // Converte para segundos (double)

            
            long p_start_nano = System.nanoTime();
            long p_count = countWordsParallel(filename, word, threads_para_teste);
            long p_end_nano = System.nanoTime();
            double p_time = (p_end_nano - p_start_nano) / 1e9;

            if (s_count != p_count) {
                System.err.printf("Contagem sequencial (%d) e paralela (%d) não coincidem!\n", s_count, p_count);
            }

            System.out.printf("%s,%s,%d,%d,%d,%.6f,%.6f,%d\n", 
                    filename, word, word.length(), threads_para_teste, 
                    length, s_time, p_time, p_count);

        } catch (Exception e) {
            e.printStackTrace();
            System.exit(-1);
        }
    }

    // medoto para incrementar o total de ocorrencias de forma thread-safe(ou seja, sem risco de condições de corrida)
    private static synchronized void incrementarTotal(long parcial) {
        totalOcorrencias += parcial;
    }

    public static long countWordsSequential(String arquivo, String palavra) throws IOException {
        long contador = 0;
        int index = 0;
        ByteBuffer buffer = ByteBuffer.allocateDirect(TAMANHO_BUFFER);

        try (RandomAccessFile raf = new RandomAccessFile(arquivo, "r");
             FileChannel canal = raf.getChannel()) {

            while (canal.read(buffer) != -1) {
                buffer.flip();
                while (buffer.hasRemaining()) {
                    byte b = buffer.get();
                    if (b == (palavra.charAt(index) & 0xFF)) {
                        index++;
                        if (index == palavra.length()) {
                            contador++;
                            index = 0;
                        }
                    } else {
                        index = 0;
                        if (b == (palavra.charAt(0) & 0xFF)) index = 1;
                    }
                }
                buffer.clear();
            }
        }
        return contador;
    }

    public static long countWordsParallel(String arquivo, String palavra, int numThreads) throws IOException, InterruptedException {
        totalOcorrencias = 0;
        semaforoDisco = new Semaphore(numThreads);
        Thread[] threads = new Thread[numThreads];

        CyclicBarrier barreira = new CyclicBarrier(numThreads, () -> {
            //System.out.println("Fase de processamento concluída por todas as threads.");
        });

        long tamanhoArquivo;
        try (RandomAccessFile raf = new RandomAccessFile(arquivo, "r")) {
            tamanhoArquivo = raf.length();
        }

        long tamanhoParte = tamanhoArquivo / numThreads;

        for (int i = 0; i < numThreads; i++) {
            long inicio = i * tamanhoParte;
            long fim;

            if (i == numThreads - 1) {
                fim = tamanhoArquivo;
            } else {
                // Sobreposição para não cortar palavras entre as threads
                fim = (i + 1) * tamanhoParte + (palavra.length() - 1);
                if (fim > tamanhoArquivo) fim = tamanhoArquivo;
            }

            threads[i] = new Thread(new WordCountTask(arquivo, palavra, inicio, fim, barreira));
            threads[i].start();
        }

        for (Thread t : threads) {
            t.join();
        }

        return totalOcorrencias;
    }

    static class WordCountTask implements Runnable {
        private String arquivo, palavra;
        private long inicio, fim;
        private CyclicBarrier barreira;

        public WordCountTask(String arquivo, String palavra, long inicio, long fim, CyclicBarrier barreira) {
            this.arquivo = arquivo;
            this.palavra = palavra;
            this.inicio = inicio;
            this.fim = fim;
            this.barreira = barreira;
        }

        @Override
        public void run() {
            long contadorLocal = 0;
            int index = 0;
            // Cada thread terá seu próprio buffer de 16MB 
            ByteBuffer buffer = ByteBuffer.allocateDirect(TAMANHO_BUFFER);

            try {
                semaforoDisco.acquire();
                try (RandomAccessFile raf = new RandomAccessFile(arquivo, "r");
                     FileChannel canal = raf.getChannel()) {

                    canal.position(inicio);
                    long limiteParaLer = fim - inicio;
                    long totalLidoDestaThread = 0;

                    while (totalLidoDestaThread < limiteParaLer) {
                        buffer.clear();
                        
                        // Garante que não leremos além do fim do bloco desta thread
                        if (limiteParaLer - totalLidoDestaThread < buffer.capacity()) {
                            buffer.limit((int) (limiteParaLer - totalLidoDestaThread));
                        }

                        int lidos = canal.read(buffer);
                        if (lidos == -1) break;

                        buffer.flip();
                        while (buffer.hasRemaining()) {
                            byte b = buffer.get();
                            if (b == (palavra.charAt(index) & 0xFF)) {
                                index++;
                                if (index == palavra.length()) {
                                    contadorLocal++;
                                    index = 0;
                                }
                            } else {
                                index = 0;
                                if (b == (palavra.charAt(0) & 0xFF)) index = 1;
                            }
                        }
                        totalLidoDestaThread += lidos;
                    }
                } finally {
                    semaforoDisco.release();
                }

                barreira.await();
                incrementarTotal(contadorLocal);

            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }
}