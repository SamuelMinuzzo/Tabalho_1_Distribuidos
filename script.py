import os

def criar_arquivo_escalonado(origem, tamanhos_gb):
    """
    origem: arquivo base (ex: arquivo_texto_rasoavel.txt)
    tamanhos_gb: lista de tamanhos em GB
    """
    if not os.path.exists(origem):
        print(f"Erro: Arquivo base '{origem}' não encontrado.")
        return

    # Lê o conteúdo base para a memória
    with open(origem, 'rb') as f:
        conteudo_base = f.read()
    
    if not conteudo_base:
        print("Erro: Arquivo base está vazio.")
        return

    len_base = len(conteudo_base)

    for gb in tamanhos_gb:
        nome_arquivo = f"arquivo{gb}GB.txt"
        tamanho_alvo = int(gb * 1024 * 1024 * 1024)
        
        print(f"Gerando {nome_arquivo} ({gb} GB)...", end="\r")
        
        bytes_escritos = 0
        with open(nome_arquivo, 'wb') as f_out:
            # Escreve o bloco base repetidamente
            num_repeticoes = tamanho_alvo // len_base
            for _ in range(num_repeticoes):
                f_out.write(conteudo_base)
            
            # Escreve o restante para completar o tamanho exato
            resto = tamanho_alvo % len_base
            if resto > 0:
                f_out.write(conteudo_base[:resto])
        
        print(f"Gerado: {nome_arquivo} [OK]          ")

if __name__ == "__main__":
    # Alterado para o seu arquivo de origem
    arquivo_base = "arquivo_texto_rasoavel.txt" 
    
    # Lista de tamanhos para os testes de performance
    lista_tamanhos = [0.1, 0.5, 1, 2, 4, 6, 8, 10, 15]
    
    # Verifica se o arquivo existe antes de iniciar, caso contrário cria um dummy
    if not os.path.exists(arquivo_base):
        print(f"Aviso: '{arquivo_base}' não encontrado. Criando um temporário...")
        with open(arquivo_base, "w", encoding="utf-8") as f:
            f.write("Conteudo de teste para busca paralela. " * 1000)

    criar_arquivo_escalonado(arquivo_base, lista_tamanhos)
    print("\nTodos os arquivos de teste foram criados com sucesso!")