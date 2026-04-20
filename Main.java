// javac Main.java
// java Main arquivo10GB.txt Bacamarte 8

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.*;
import java.util.Locale;

/**
 * Programa para contar ocorrencias de uma palavra em um arquivo de texto usando
 * uma estrategia Boyer-Moore-Horspool (BMH) em modo sequencial e paralelo.
 *
 * A aplicacao le o arquivo em blocos, trata sobreposicoes entre chunks para nao
 * perder matches nas fronteiras e imprime um resumo com os tempos de execucao.
 */
public class Main {

    /** Tamanho da tabela de deslocamento baseada no conjunto de bytes. */
    private static final int TAMANHO_ALFABETO = 256;
    /** Tamanho padrao do chunk de leitura do arquivo. */
    private static final int CHUNK_SIZE = 512 * 1024 * 1024;
    /** Tamanho do buffer de entrada usado pelo stream de leitura. */
    private static final int IO_BUFFER_SIZE = 24 * 1024 * 1024;
    /** Menor chunk que a alocacao em fallback tenta manter. */
    private static final int MIN_CHUNK_SIZE = 1 * 1024 * 1024;
    /** Limite minimo para ativar o processamento paralelo por buffer. */
    private static final int MIN_PARALLEL_BYTES = 4 * 1024 * 1024;

    /**
     * Monta a tabela de deslocamento usada pelo algoritmo Boyer-Moore-Horspool.
     *
     * @param palavra palavra buscada, convertida em bytes.
     * @param tabela tabela de deslocamento a ser preenchida.
     */
    private static void montaTabelaDeslocamento(byte[] palavra, int[] tabela) {
        int m = palavra.length;

        for (int i = 0; i < TAMANHO_ALFABETO; i++) {
            tabela[i] = m;
        }

        if (m >= 2) {
            for (int i = 0; i < m - 1; i++) {
                tabela[palavra[i] & 0xFF] = m - 1 - i;
            }
        }
    }

    /**
     * Conta quantas vezes um byte aparece em um intervalo do vetor.
     *
     * @param texto buffer com o texto a analisar.
     * @param inicio indice inicial do intervalo.
     * @param fim indice final exclusivo do intervalo.
     * @param c byte a ser contado.
     * @return quantidade de ocorrencias encontradas.
     */
    private static long contaUmCaractereIntervalo(byte[] texto, int inicio, int fim, byte c) {
        long count = 0;
        for (int i = inicio; i < fim; i++) {
            if (texto[i] == c) {
                count++;
            }
        }
        return count;
    }

    /**
        * Executa BMH em um intervalo "de propriedade".
     *
     * A thread é dona dos matches cujo índice inicial está em:
     * [inicioPropriedade, fimPropriedade)
     *
     * Para não perder matches na fronteira, a thread pode inspecionar
     * até fimPropriedade + (m - 1), mas só contabiliza matches cujo
     * início pertence ao seu intervalo de propriedade.
         *
         * @param texto buffer com o texto a analisar.
         * @param tamanhoTexto quantidade de bytes validos no buffer.
         * @param palavra palavra buscada, convertida em bytes.
         * @param tabela tabela de deslocamento da BMH.
         * @param inicioPropriedade inicio do intervalo de responsabilidade da thread.
         * @param fimPropriedade fim exclusivo do intervalo de responsabilidade da thread.
         * @return quantidade de ocorrencias encontradas no intervalo.
     */
    private static long bmhIntervaloPropriedade(
            byte[] texto,
            int tamanhoTexto,
            byte[] palavra,
            int[] tabela,
            int inicioPropriedade,
            int fimPropriedade) {
        int m = palavra.length;

        if (inicioPropriedade >= fimPropriedade || tamanhoTexto <= 0) {
            return 0;
        }

        if (m == 1) {
            return contaUmCaractereIntervalo(texto, inicioPropriedade, fimPropriedade, palavra[0]);
        }

        if (tamanhoTexto < m) {
            return 0;
        }

        int ultimoInicioValido = tamanhoTexto - m;
        if (inicioPropriedade > ultimoInicioValido) {
            return 0;
        }

        if (fimPropriedade - 1 > ultimoInicioValido) {
            fimPropriedade = ultimoInicioValido + 1;
        }

        if (inicioPropriedade >= fimPropriedade) {
            return 0;
        }

        long count = 0;
        int m1 = m - 1;
        int i = inicioPropriedade + m1;
        byte ultimo = palavra[m1];

        int limiteInspecao = fimPropriedade + m1;
        if (limiteInspecao > tamanhoTexto) {
            limiteInspecao = tamanhoTexto;
        }

        while (i < limiteInspecao) {
            byte c = texto[i];

            if (c == ultimo) {
                int j = m1;
                int k = i;

                while (j > 0 && texto[k - 1] == palavra[j - 1]) {
                    j--;
                    k--;
                }

                if (j == 0) {
                    int inicioMatch = i - m1;

                    if (inicioMatch >= inicioPropriedade && inicioMatch < fimPropriedade) {
                        count++;
                    }

                    // permite sobreposição
                    i += 1;
                    continue;
                }
            }

            i += tabela[c & 0xFF];
        }

        return count;
    }

    /**
     * Executa BMH em um buffer completo, cobrindo todo o intervalo valido.
     *
     * @param texto buffer com o texto a analisar.
     * @param tamanhoTexto quantidade de bytes validos no buffer.
     * @param palavra palavra buscada, convertida em bytes.
     * @param tabela tabela de deslocamento da BMH.
     * @return quantidade de ocorrencias encontradas no buffer.
     */
    private static long bmhSequencialBuffer(byte[] texto, int tamanhoTexto, byte[] palavra, int[] tabela) {
        return bmhIntervaloPropriedade(texto, tamanhoTexto, palavra, tabela, 0, tamanhoTexto);
    }

    /**
     * Executa a busca BMH em paralelo sobre um buffer já carregado em memoria.
     *
     * @param texto buffer com o texto a analisar.
     * @param tamanhoTexto quantidade de bytes validos no buffer.
     * @param palavra palavra buscada, convertida em bytes.
     * @param tabela tabela de deslocamento da BMH.
     * @param numThreads numero de threads a usar.
     * @param executor executor responsavel por despachar as tarefas.
     * @return quantidade total de ocorrencias encontradas.
     * @throws InterruptedException se a execucao for interrompida ao aguardar resultados.
     * @throws ExecutionException se alguma tarefa paralela falhar.
     */
    private static long bmhParaleloBuffer(
            byte[] texto,
            int tamanhoTexto,
            byte[] palavra,
            int[] tabela,
            int numThreads,
            ExecutorService executor) throws InterruptedException, ExecutionException {

        int m = palavra.length;

        if (m == 0 || tamanhoTexto <= 0) {
            return 0;
        }

        if (m == 1) {
            List<Future<Long>> futures = new ArrayList<>(numThreads);

            for (int t = 0; t < numThreads; t++) {
                final int inicio = (int) (((long) tamanhoTexto * t) / numThreads);
                final int fim = (int) (((long) tamanhoTexto * (t + 1)) / numThreads);

                futures.add(executor.submit(() -> contaUmCaractereIntervalo(texto, inicio, fim, palavra[0])));
            }

            long total = 0;
            for (Future<Long> f : futures) {
                total += f.get();
            }
            return total;
        }

        if (tamanhoTexto < m || tamanhoTexto < MIN_PARALLEL_BYTES || numThreads <= 1) {
            return bmhSequencialBuffer(texto, tamanhoTexto, palavra, tabela);
        }

        int totalIniciosValidos = tamanhoTexto - m + 1;
        List<Future<Long>> futures = new ArrayList<>(numThreads);

        for (int t = 0; t < numThreads; t++) {
            final int inicioPropriedade = (int) (((long) totalIniciosValidos * t) / numThreads);
            final int fimPropriedade = (int) (((long) totalIniciosValidos * (t + 1)) / numThreads);

            futures.add(executor.submit(() -> bmhIntervaloPropriedade(
                    texto,
                    tamanhoTexto,
                    palavra,
                    tabela,
                    inicioPropriedade,
                    fimPropriedade)));
        }

        long total = 0;
        for (Future<Long> f : futures) {
            total += f.get();
        }

        return total;
    }

    /**
     * Tenta alocar um buffer e reduz o tamanho em caso de falta de memoria.
     *
     * @param chunkReal tamanho inicial desejado para o chunk.
     * @param sobreposicao quantidade de bytes reservada para a sobreposicao entre leituras.
     * @return buffer alocado com espaco para o chunk e a sobreposicao.
     */
    private static byte[] alocaBufferComFallback(int chunkReal, int sobreposicao) {
        int chunkTentativa = chunkReal;
        int limiteMinimo = Math.min(MIN_CHUNK_SIZE, chunkReal);

        while (chunkTentativa >= limiteMinimo) {
            try {
                return new byte[chunkTentativa + sobreposicao];
            } catch (OutOfMemoryError e) {
                chunkTentativa /= 2;
            }
        }

        if (chunkTentativa > 0) {
            return new byte[chunkTentativa + sobreposicao];
        }

        throw new OutOfMemoryError("Nao foi possivel alocar buffer minimo para leitura");
    }

    /**
     * Conta ocorrencias de uma palavra em um arquivo usando BMH sequencial.
     *
     * @param caminhoArquivo caminho do arquivo de entrada.
     * @param palavra palavra procurada.
     * @return quantidade de ocorrencias ou -1 em caso de erro de leitura.
     */
    private static long contaSubstringsArquivoBmhSequencial(String caminhoArquivo, String palavra) {
        byte[] palavraBytes = palavra.getBytes(StandardCharsets.UTF_8);
        int m = palavraBytes.length;

        if (m == 0) {
            return 0;
        }

        File file = new File(caminhoArquivo);
        long tamanhoArquivo = file.length();

        int[] tabela = new int[TAMANHO_ALFABETO];
        montaTabelaDeslocamento(palavraBytes, tabela);

        int sobreposicao = m - 1;
        int chunkReal = (tamanhoArquivo > 0 && tamanhoArquivo < CHUNK_SIZE)
                ? (int) tamanhoArquivo
                : CHUNK_SIZE;

        byte[] buffer = alocaBufferComFallback(chunkReal, sobreposicao);
        int capacidadeLeitura = buffer.length - sobreposicao;

        long total = 0;
        int bytesSobrepostos = 0;

        try (BufferedInputStream in = new BufferedInputStream(new FileInputStream(file), IO_BUFFER_SIZE)) {
            while (true) {
                int bytesLidos = in.read(buffer, bytesSobrepostos, capacidadeLeitura);

                if (bytesLidos <= 0) {
                    break;
                }

                int tamanhoValido = bytesSobrepostos + bytesLidos;
                total += bmhSequencialBuffer(buffer, tamanhoValido, palavraBytes, tabela);

                if (sobreposicao > 0) {
                    if (tamanhoValido >= sobreposicao) {
                        System.arraycopy(buffer, tamanhoValido - sobreposicao, buffer, 0, sobreposicao);
                        bytesSobrepostos = sobreposicao;
                    } else {
                        System.arraycopy(buffer, 0, buffer, 0, tamanhoValido);
                        bytesSobrepostos = tamanhoValido;
                    }
                } else {
                    bytesSobrepostos = 0;
                }
            }
        } catch (IOException e) {
            System.err.println("Erro ao ler arquivo: " + e.getMessage());
            return -1;
        }

        return total;
    }

    /**
     * Conta ocorrencias de uma palavra em um arquivo usando BMH em paralelo.
     *
     * @param caminhoArquivo caminho do arquivo de entrada.
     * @param palavra palavra procurada.
     * @param numThreads numero de threads a usar.
     * @return quantidade de ocorrencias ou -1 em caso de erro de leitura/processamento.
     */
    private static long contaSubstringsArquivoBmhParalelo(String caminhoArquivo, String palavra, int numThreads) {
        byte[] palavraBytes = palavra.getBytes(StandardCharsets.UTF_8);
        int m = palavraBytes.length;

        if (m == 0) {
            return 0;
        }

        File file = new File(caminhoArquivo);
        long tamanhoArquivo = file.length();

        int[] tabela = new int[TAMANHO_ALFABETO];
        montaTabelaDeslocamento(palavraBytes, tabela);

        int sobreposicao = m - 1;
        int chunkReal = (tamanhoArquivo > 0 && tamanhoArquivo < CHUNK_SIZE)
                ? (int) tamanhoArquivo
                : CHUNK_SIZE;

        byte[] buffer = alocaBufferComFallback(chunkReal, sobreposicao);
        int capacidadeLeitura = buffer.length - sobreposicao;

        long total = 0;
        int bytesSobrepostos = 0;

        ExecutorService executor = Executors.newFixedThreadPool(numThreads);

        try (BufferedInputStream in = new BufferedInputStream(new FileInputStream(file), IO_BUFFER_SIZE)) {
            while (true) {
                int bytesLidos = in.read(buffer, bytesSobrepostos, capacidadeLeitura);

                if (bytesLidos <= 0) {
                    break;
                }

                int tamanhoValido = bytesSobrepostos + bytesLidos;

                total += bmhParaleloBuffer(
                        buffer,
                        tamanhoValido,
                        palavraBytes,
                        tabela,
                        numThreads,
                        executor);

                if (sobreposicao > 0) {
                    if (tamanhoValido >= sobreposicao) {
                        System.arraycopy(buffer, tamanhoValido - sobreposicao, buffer, 0, sobreposicao);
                        bytesSobrepostos = sobreposicao;
                    } else {
                        System.arraycopy(buffer, 0, buffer, 0, tamanhoValido);
                        bytesSobrepostos = tamanhoValido;
                    }
                } else {
                    bytesSobrepostos = 0;
                }
            }
        } catch (IOException e) {
            System.err.println("Erro ao ler arquivo: " + e.getMessage());
            return -1;
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            System.err.println("Execucao interrompida: " + e.getMessage());
            return -1;
        } catch (ExecutionException e) {
            System.err.println("Erro no processamento paralelo: " + e.getMessage());
            return -1;
        } finally {
            executor.shutdown();
        }

        return total;
    }

    /**
     * Ponto de entrada da aplicacao.
     *
     * Espera os argumentos: arquivo, palavra e quantidade de threads. Executa as
     * contagens sequencial e paralela e imprime uma linha CSV com os resultados.
     *
     * @param args argumentos da linha de comando.
     */
    public static void main(String[] args) {
        if (args.length < 3) {
            System.err.println("Uso: java Main <arquivo> <palavra> <num_threads>");
            System.err.println("Exemplo: java Main arquivo10GB.txt Bacamarte 8");
            System.exit(1);
        }

        String arquivo = args[0];
        String palavra = args[1];
        int numThreads = Integer.parseInt(args[2]);

        if (palavra.isEmpty()) {
            System.err.println("Erro: a palavra nao pode ser vazia");
            System.exit(1);
        }

        if (numThreads <= 0) {
            System.err.println("Erro: num_threads deve ser maior que zero");
            System.exit(1);
        }

        File file = new File(arquivo);
        if (!file.exists() || !file.isFile()) {
            System.err.println("Erro ao abrir arquivo: " + arquivo);
            System.exit(1);
        }

        long tamanhoArquivo = file.length();

        long sInicio = System.nanoTime();
        long totalSeq = contaSubstringsArquivoBmhSequencial(arquivo, palavra);
        long sFim = System.nanoTime();

        if (totalSeq < 0) {
            System.exit(1);
        }

        long pInicio = System.nanoTime();
        long totalPar = contaSubstringsArquivoBmhParalelo(arquivo, palavra, numThreads);
        long pFim = System.nanoTime();

        if (totalPar < 0) {
            System.exit(1);
        }

        if (totalSeq != totalPar) {
            System.err.printf(
                    "Aviso: contagem sequencial (%d) e paralela (%d) diferem.%n",
                    totalSeq, totalPar);
        }

        double tempoSeq = (sFim - sInicio) / 1_000_000_000.0;
        double tempoPar = (pFim - pInicio) / 1_000_000_000.0;

        System.out.printf(
                Locale.US,
                "%s,%s,%d,%d,%d,%.9f,%.9f,%d%n",
                arquivo,
                palavra,
                palavra.length(),
                numThreads,
                tamanhoArquivo,
                tempoSeq,
                tempoPar,
                totalPar
            );
    }
}