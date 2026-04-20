import csv
import os
import subprocess
import time
from statistics import mean

EXEC_C = "./main" if os.name != "nt" else "main.exe"
EXEC_JAVA = ["java", "Main"]

PALAVRAS_TESTE = ["a", "como", "Bacamarte"]

ARQUIVOS = [
    "arquivo0.1GB.txt",
    "arquivo1GB.txt",
    "arquivo2GB.txt",
    "arquivo4GB.txt",
    "arquivo8GB.txt",
    "arquivo16GB.txt",
]

LISTA_THREADS = [1, 2, 8, 20, 40, 80, 120, 160, 200]
REPETICOES = 2
OUTPUT_FILE = "resultados.csv"
TIMEOUT = 3600


def executar_comando(cmd):
    result = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        timeout=TIMEOUT
    )

    if result.returncode != 0:
        raise RuntimeError(result.stderr)

    linhas = [l.strip() for l in result.stdout.splitlines() if l.strip()]
    return linhas[-1]


def parse_saida(linha):
    p = [x.strip() for x in linha.split(",")]

    if len(p) != 8:
        raise ValueError(f"Saída inesperada ({len(p)} campos): {linha}")

    return {
        "arquivo": p[0],
        "palavra": p[1],
        "tam": int(p[2]),
        "threads": int(p[3]),
        "tam_bytes": int(p[4]),
        "tempo_seq": float(p[5]),
        "tempo_par": float(p[6]),
        "ocorr": int(p[7]),
    }


def executar_teste(exec_cmd, nome_impl, arquivo, palavra, threads):
    cmd = exec_cmd + [arquivo, palavra, str(threads)] if isinstance(exec_cmd, list) else [exec_cmd, arquivo, palavra, str(threads)]

    linha = executar_comando(cmd)
    dados = parse_saida(linha)

    dados["impl"] = nome_impl
    return dados


def main():

    total = len(ARQUIVOS) * len(PALAVRAS_TESTE) * len(LISTA_THREADS) * REPETICOES * 2
    atual = 0

    with open(OUTPUT_FILE, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f, delimiter=";")

        writer.writerow([
            "implementacao",
            "arquivo",
            "palavra",
            "threads",
            "tempo_seq",
            "tempo_par",
            "speedup",
            "ocorrencias"
        ])

        for arquivo in ARQUIVOS:
            for palavra in PALAVRAS_TESTE:
                for threads in LISTA_THREADS:

                    for impl, exec_cmd in [
                        ("C", EXEC_C),
                        ("Java", EXEC_JAVA),
                    ]:

                        tempos_seq = []
                        tempos_par = []
                        ocorrencias = []

                        for r in range(REPETICOES):
                            atual += 1
                            perc = (atual / total) * 100

                            print(f"{impl} | {arquivo} | {palavra} | t={threads} | rep {r+1} | {perc:.2f}%")

                            try:
                                dados = executar_teste(exec_cmd, impl, arquivo, palavra, threads)

                                tempos_seq.append(dados["tempo_seq"])
                                tempos_par.append(dados["tempo_par"])
                                ocorrencias.append(dados["ocorr"])

                            except Exception as e:
                                print("ERRO:", e)

                        if tempos_seq:
                            seq = mean(tempos_seq)
                            par = mean(tempos_par)
                            occ = round(mean(ocorrencias))
                            speedup = seq / par if par > 0 else 0

                            writer.writerow([
                                impl,
                                arquivo,
                                palavra,
                                threads,
                                f"{seq:.6f}",
                                f"{par:.6f}",
                                f"{speedup:.6f}",
                                occ
                            ])

    print("\nFINALIZADO -> resultados.csv")


if __name__ == "__main__":
    main()