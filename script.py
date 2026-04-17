import os
import sys

def criar_arquivo_escalonado(origem, prefixo_saida, tamanhos_gb):
    """
    origem: arquivo pequeno base (ex: texto de 1MB)
    prefixo_saida: pasta ou prefixo para os arquivos
    tamanhos_gb: lista de tamanhos em GB
    """
    if not os.path.exists(origem):
        print(f"Erro: Arquivo base '{origem}' não encontrado.")
        return

    # Lê o conteúdo base uma vez para a memória para ser ultra rápido
    with open(origem, 'rb') as f:
        conteudo_base = f.read()
    
    if not conteudo_base:
        print("Erro: Arquivo base está vazio.")
        return

    for gb in tamanhos_gb:
        nome_arquivo = f"arquivo{gb}GB.txt"
        tamanho_alvo = int(gb * 1024 * 1024 * 1024)
        
        print(f"Gerando {nome_arquivo} ({gb} GB)...", end="\r")
        
        bytes_escritos = 0
        with open(nome_arquivo, 'wb') as f_out:
            # Escreve em blocos grandes para performance de escrita
            while bytes_escritos < tamanho_alvo:
                restante = tamanho_alvo - bytes_escritos
                chunk = conteudo_base[:restante] if len(conteudo_base) > restante else conteudo_base
                f_out.write(chunk)
                bytes_escritos += len(chunk)
        
        print(f"Gerado: {nome_arquivo} [OK]          ")

if __name__ == "__main__":
    # Lista sugerida para uma apresentação impactante:
    # 0.1 (100MB) e 0.5 (500MB) servem para mostrar o overhead das threads
    # 1 a 15 mostram a escalabilidade e o limite do SSD
    lista_tamanhos = [0.1, 0.5, 1, 2, 4, 6, 8, 10, 15]
    
    arquivo_base = "arquivo_texto_medio.txt" # Crie um arquivo txt pequeno com algumas frases
    if not os.path.exists(arquivo_base):
        with open(arquivo_base, "w") as f:
            f.write("exemplo de conteudo para busca paralela " * 100)

    criar_arquivo_escalonado(arquivo_base, "arquivo", lista_tamanhos)
    print("\nTodos os arquivos de teste foram criados!")