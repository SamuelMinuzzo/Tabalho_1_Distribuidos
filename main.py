import sys
import os

def parse_tamanho(valor):
    unidades = {
        "KB": 1024,
        "MB": 1024 ** 2,
        "GB": 1024 ** 3,
        "TB": 1024 ** 4,
    }

    valor = valor.strip().upper()

    if valor.isdigit():
        tamanho = int(valor)
        if tamanho <= 0:
            raise ValueError("O tamanho deve ser maior que zero.")
        return tamanho

    for unidade, multiplicador in unidades.items():
        if valor.endswith(unidade):
            numero = valor[:-len(unidade)].strip()
            if not numero:
                raise ValueError("Informe um valor numérico antes da unidade.")
            tamanho = int(float(numero) * multiplicador)
            if tamanho <= 0:
                raise ValueError("O tamanho deve ser maior que zero.")
            return tamanho

    raise ValueError("Formato inválido. Use bytes ou sufixo KB, MB, GB, TB.")


def copiar_arquivo_repetindo(origem, saida, tamanho_desejado):
    if not os.path.exists(origem):
        raise FileNotFoundError(f"Arquivo {origem} não encontrado!")

    if os.path.getsize(origem) == 0:
        raise ValueError("Arquivo origem está vazio!")

    with open(origem, 'rb') as f_origem, open(saida, 'wb') as f_saida:
        bytes_escritos = 0
        while bytes_escritos < tamanho_desejado:
            f_origem.seek(0)  # Volta ao início
            while bytes_escritos < tamanho_desejado:
                bytes_restantes = tamanho_desejado - bytes_escritos
                chunk_size = min(1024 * 1024, bytes_restantes)  # 1MB por chunk
                chunk = f_origem.read(chunk_size)
                if not chunk:
                    break  # Fim do arquivo origem
                f_saida.write(chunk)
                bytes_escritos += len(chunk)

    return os.path.getsize(saida)


def main():
    if len(sys.argv) != 4:
        print("Uso: python main.py <arquivo_origem> <arquivo_saida> <tamanho>")
        print("Exemplos de tamanho: 10GB, 512MB, 2147483648")
        sys.exit(1)

    origem = sys.argv[1]
    saida = sys.argv[2]

    try:
        tamanho_desejado = parse_tamanho(sys.argv[3])
    except ValueError as e:
        print(f"Erro no tamanho: {e}")
        sys.exit(1)

    try:
        tamanho_final = copiar_arquivo_repetindo(origem, saida, tamanho_desejado)
        print(f"Arquivo {saida} criado com {tamanho_final} bytes.")
    except (FileNotFoundError, ValueError) as e:
        print(e)
        sys.exit(1)


if __name__ == '__main__':
    main()