import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import javax.swing.JOptionPane;

public class main {
	private static final int TAMANHO_ALFABETO = 256;
	private static final int CHUNK_SIZE = 256 * 1024 * 1024;
	private static final int IO_BUFFER_SIZE = 8 * 1024 * 1024;
	private static final int MIN_CHUNK_SIZE = 1 * 1024 * 1024;

	private static void montaTabelaDeslocamento(byte[] palavra, int[] tabela) {
		int m = palavra.length;

		for (int i = 0; i < TAMANHO_ALFABETO; i++) {
			tabela[i] = m;
		}

		for (int i = 0; i < m - 1; i++) {
			tabela[palavra[i] & 0xFF] = m - 1 - i;
		}
	}

	private static long contaUmCaractere(byte[] texto, int tamanho, byte c) {
		long count = 0;
		for (int i = 0; i < tamanho; i++) {
			if (texto[i] == c) {
				count++;
			}
		}
		return count;
	}

	private static long bmhNoChunk(byte[] texto, int tamanho, byte[] palavra, int[] tabela) {
		int m = palavra.length;

		if (m == 1) {
			return contaUmCaractere(texto, tamanho, palavra[0]);
		}

		long count = 0;
		int i = m - 1;
		byte ultimo = palavra[m - 1];

		while (i < tamanho) {
			byte c = texto[i];

			if (c == ultimo) {
				int j = m - 1;
				int k = i;

				while (j > 0 && texto[k - 1] == palavra[j - 1]) {
					j--;
					k--;
				}

				if (j == 0) {
					count++;
					i += 1;
					continue;
				}
			}

			i += tabela[c & 0xFF];
		}

		return count;
	}

	private static byte[] alocaBufferComFallback(int chunkReal, int sobreposicao) {
		int chunkTentativa = chunkReal;
		int limiteMinimo = Math.min(MIN_CHUNK_SIZE, chunkReal);

		while (chunkTentativa >= limiteMinimo) {
			try {
				return new byte[chunkTentativa + sobreposicao + 1];
			} catch (OutOfMemoryError e) {
				chunkTentativa /= 2;
			}
		}

		if (chunkTentativa > 0) {
			return new byte[chunkTentativa + sobreposicao + 1];
		}

		throw new OutOfMemoryError("Nao foi possivel alocar buffer minimo para leitura");
	}

	private static long contaPalavrasBmh(String palavra, String caminhoArquivo) {
		byte[] palavraBytes = palavra.getBytes(StandardCharsets.UTF_8);
		int m = palavraBytes.length;

		if (m == 0) {
			System.out.println("Palavra vazia nao eh valida");
			return 0;
		}

		File file = new File(caminhoArquivo);
		long tamanhoArquivo = file.length();
		System.out.println("Tamanho do arquivo: " + tamanhoArquivo);

		int[] tabela = new int[TAMANHO_ALFABETO];
		montaTabelaDeslocamento(palavraBytes, tabela);

		int sobreposicao = m - 1;
		int chunkReal = (tamanhoArquivo > 0 && tamanhoArquivo < CHUNK_SIZE)
				? (int) tamanhoArquivo
				: CHUNK_SIZE;

		byte[] buffer = alocaBufferComFallback(chunkReal, sobreposicao);
		int capacidadeLeitura = buffer.length - sobreposicao - 1;

		long count = 0;
		int bytesSobrepostos = 0;

		try (BufferedInputStream in = new BufferedInputStream(new FileInputStream(file), IO_BUFFER_SIZE)) {
			while (true) {
				int bytesLidos = in.read(buffer, bytesSobrepostos, capacidadeLeitura);
				if (bytesLidos <= 0) {
					break;
				}

				int tamanhoValido = bytesSobrepostos + bytesLidos;
				count += bmhNoChunk(buffer, tamanhoValido, palavraBytes, tabela);

				if (tamanhoValido >= sobreposicao) {
					System.arraycopy(buffer, tamanhoValido - sobreposicao, buffer, 0, sobreposicao);
					bytesSobrepostos = sobreposicao;
				} else {
					bytesSobrepostos = tamanhoValido;
				}

				if (bytesLidos < capacidadeLeitura) {
					break;
				}
			}
		} catch (IOException e) {
			System.out.println("Erro ao ler arquivo: " + e.getMessage());
			return 0;
		}

		return count;
	}

	public static void main(String[] args) {
		String palavra = JOptionPane.showInputDialog(null,
				"Digite a palavra que quer procurar:",
				"Busca Sequencial BMH",
				JOptionPane.QUESTION_MESSAGE);

		if (palavra == null) {
			System.out.println("Operacao cancelada pelo usuario.");
			return;
		}

		palavra = palavra.trim();
		if (palavra.isEmpty()) {
			JOptionPane.showMessageDialog(null, "Palavra vazia nao eh valida.", "Erro", JOptionPane.ERROR_MESSAGE);
			return;
		}

		String caminhoArquivo = JOptionPane.showInputDialog(null,
				"Digite o caminho do arquivo (vazio para usar arquivo_10_G.txt):",
				"arquivo_10_G.txt");

		if (caminhoArquivo == null) {
			System.out.println("Operacao cancelada pelo usuario.");
			return;
		}

		caminhoArquivo = caminhoArquivo.trim();
		if (caminhoArquivo.isEmpty()) {
			caminhoArquivo = "arquivo_10_G.txt";
		}

		File arquivo = new File(caminhoArquivo);
		if (!arquivo.exists() || !arquivo.isFile()) {
			JOptionPane.showMessageDialog(null,
					"Arquivo nao encontrado: " + caminhoArquivo,
					"Erro",
					JOptionPane.ERROR_MESSAGE);
			return;
		}

		System.out.println("palavra: " + palavra);
		System.out.println("Palavra que sera procurada: " + palavra);
		System.out.println("Arquivo de entrada: " + caminhoArquivo);

		long ini = System.nanoTime();
		long total = contaPalavrasBmh(palavra, caminhoArquivo);
		long fim = System.nanoTime();

		double tempo = (fim - ini) / 1_000_000_000.0;
		System.out.printf("Total de vezes encontrada: %d%n", total);
		System.out.printf("Tempo total de execucao: %.12f%n", tempo);

		String mensagemFinal = String.format(
				"Palavra: %s%nArquivo: %s%nTotal de vezes encontrada: %d%nTempo total de execucao: %.12f segundos",
				palavra, caminhoArquivo, total, tempo);
		JOptionPane.showMessageDialog(null, mensagemFinal, "Resultado", JOptionPane.INFORMATION_MESSAGE);
	}
}
