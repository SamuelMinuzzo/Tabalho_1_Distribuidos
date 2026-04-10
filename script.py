import sys
import os

if len(sys.argv) != 3:
    print("Uso: python copiar_ate_2gb.py <arquivo_origem> <arquivo_saida>")
    sys.exit(1)

origem = sys.argv[1]
saida = sys.argv[2]
tamanho_desejado = 2147483648  # 2GB em bytes

if not os.path.exists(origem):
    print(f"Arquivo {origem} não encontrado!")
    sys.exit(1)

tamanho_origem = os.path.getsize(origem)
if tamanho_origem == 0:
    print("Arquivo origem está vazio!")
    sys.exit(1)

with open(origem, 'rb') as f_origem, open(saida, 'wb') as f_saida:
    while os.path.getsize(saida) < tamanho_desejado:
        f_origem.seek(0)  # Volta ao início
        chunk_size = 1024 * 1024  # 1MB por chunk
        bytes_restantes = tamanho_desejado - os.path.getsize(saida)
        if bytes_restantes < chunk_size:
            chunk_size = bytes_restantes
        
        while os.path.getsize(saida) < tamanho_desejado:
            chunk = f_origem.read(chunk_size)
            if not chunk:
                break  # Fim do arquivo origem
            escreve = min(len(chunk), tamanho_desejado - os.path.getsize(saida))
            f_saida.write(chunk[:escreve])
            if os.path.getsize(saida) >= tamanho_desejado:
                break

print(f"Arquivo {saida} criado com {os.path.getsize(saida)} bytes.")