import os
import pandas as pd
import matplotlib.pyplot as plt

ARQUIVO_CSV = "resultados.csv"
PASTA_SAIDA = "graficos"
THREADS_REFERENCIA = 20  # 16 não existe neste conjunto de dados


def garantir_pasta_saida():
    os.makedirs(PASTA_SAIDA, exist_ok=True)


def extrair_tamanho_gb(nome_arquivo):
    """
    Extrai o tamanho do arquivo a partir do nome:
    arquivo0.1GB.txt -> 0.1
    arquivo10GB.txt -> 10.0
    """
    nome = nome_arquivo.replace("arquivo", "").replace("GB.txt", "")
    return float(nome)


def carregar_dados():
    df = pd.read_csv(ARQUIVO_CSV, sep=";")

    colunas_necessarias = [
        "implementacao",
        "arquivo",
        "palavra",
        "threads",
        "tempo_seq",
        "tempo_par",
        "speedup",
        "ocorrencias",
    ]

    for col in colunas_necessarias:
        if col not in df.columns:
            raise ValueError(f"Coluna obrigatória ausente no CSV: {col}")

    df["threads"] = pd.to_numeric(df["threads"], errors="coerce")
    df["tempo_seq"] = pd.to_numeric(df["tempo_seq"], errors="coerce")
    df["tempo_par"] = pd.to_numeric(df["tempo_par"], errors="coerce")
    df["speedup"] = pd.to_numeric(df["speedup"], errors="coerce")
    df["ocorrencias"] = pd.to_numeric(df["ocorrencias"], errors="coerce")
    df["tamanho_arquivo_gb"] = df["arquivo"].apply(extrair_tamanho_gb)

    df = df.dropna().copy()
    df = df.sort_values(["implementacao", "tamanho_arquivo_gb", "palavra", "threads"])
    return df


def salvar_grafico(nome_arquivo):
    caminho = os.path.join(PASTA_SAIDA, nome_arquivo)
    plt.tight_layout()
    plt.savefig(caminho, dpi=220, bbox_inches="tight")
    plt.close()
    print(f"Gráfico salvo: {caminho}")


def grafico_tempo_sequencial_por_tamanho(df):
    plt.figure(figsize=(9, 5))

    for impl in sorted(df["implementacao"].unique()):
        sub = (
            df[df["implementacao"] == impl]
            .groupby("tamanho_arquivo_gb", as_index=False)["tempo_seq"]
            .mean()
            .sort_values("tamanho_arquivo_gb")
        )

        plt.plot(
            sub["tamanho_arquivo_gb"],
            sub["tempo_seq"],
            marker="o",
            label=impl
        )

    plt.xlabel("Tamanho do arquivo (GB)")
    plt.ylabel("Tempo sequencial médio (s)")
    plt.title("Tempo sequencial médio por tamanho de arquivo")
    plt.grid(True, alpha=0.3)
    plt.legend()

    salvar_grafico("tempo_sequencial_por_tamanho.png")


def grafico_tempo_paralelo_por_tamanho(df, threads_referencia=THREADS_REFERENCIA):
    plt.figure(figsize=(9, 5))

    for impl in sorted(df["implementacao"].unique()):
        sub = df[
            (df["implementacao"] == impl) &
            (df["threads"] == threads_referencia)
        ]

        agrupado = (
            sub.groupby("tamanho_arquivo_gb", as_index=False)["tempo_par"]
            .mean()
            .sort_values("tamanho_arquivo_gb")
        )

        plt.plot(
            agrupado["tamanho_arquivo_gb"],
            agrupado["tempo_par"],
            marker="o",
            label=f"{impl} ({threads_referencia} threads)"
        )

    plt.xlabel("Tamanho do arquivo (GB)")
    plt.ylabel("Tempo paralelo médio (s)")
    plt.title(f"Tempo paralelo médio por tamanho de arquivo ({threads_referencia} threads)")
    plt.grid(True, alpha=0.3)
    plt.legend()

    salvar_grafico(f"tempo_paralelo_por_tamanho_{threads_referencia}_threads.png")


def grafico_speedup_por_threads(df):
    for impl in sorted(df["implementacao"].unique()):
        plt.figure(figsize=(9, 5))

        sub_impl = df[df["implementacao"] == impl]
        tamanhos = sorted(sub_impl["tamanho_arquivo_gb"].unique())

        for tamanho in tamanhos:
            sub = sub_impl[sub_impl["tamanho_arquivo_gb"] == tamanho]

            agrupado = (
                sub.groupby("threads", as_index=False)["speedup"]
                .mean()
                .sort_values("threads")
            )

            plt.plot(
                agrupado["threads"],
                agrupado["speedup"],
                marker="o",
                label=f"{tamanho:g} GB"
            )

        plt.xlabel("Número de threads")
        plt.ylabel("Speedup médio")
        plt.title(f"Speedup médio por número de threads - {impl}")
        plt.grid(True, alpha=0.3)
        plt.legend(fontsize=8, ncol=2)

        salvar_grafico(f"speedup_por_threads_{impl}.png")


def grafico_speedup_medio_por_tamanho(df):
    plt.figure(figsize=(9, 5))

    for impl in sorted(df["implementacao"].unique()):
        sub = (
            df[df["implementacao"] == impl]
            .groupby("tamanho_arquivo_gb", as_index=False)["speedup"]
            .mean()
            .sort_values("tamanho_arquivo_gb")
        )

        plt.plot(
            sub["tamanho_arquivo_gb"],
            sub["speedup"],
            marker="o",
            label=impl
        )

    plt.xlabel("Tamanho do arquivo (GB)")
    plt.ylabel("Speedup médio")
    plt.title("Speedup médio por tamanho de arquivo")
    plt.grid(True, alpha=0.3)
    plt.legend()

    salvar_grafico("speedup_medio_por_tamanho.png")


def grafico_speedup_por_palavra(df):
    palavras = sorted(df["palavra"].unique())
    x = range(len(palavras))
    largura = 0.35

    resumo = (
        df.groupby(["implementacao", "palavra"], as_index=False)["speedup"]
        .mean()
    )

    c_vals = []
    j_vals = []

    for palavra in palavras:
        c = resumo[
            (resumo["implementacao"] == "C") &
            (resumo["palavra"] == palavra)
        ]["speedup"]

        j = resumo[
            (resumo["implementacao"] == "Java") &
            (resumo["palavra"] == palavra)
        ]["speedup"]

        c_vals.append(float(c.iloc[0]) if not c.empty else 0.0)
        j_vals.append(float(j.iloc[0]) if not j.empty else 0.0)

    plt.figure(figsize=(9, 5))
    plt.bar([i - largura / 2 for i in x], c_vals, width=largura, label="C")
    plt.bar([i + largura / 2 for i in x], j_vals, width=largura, label="Java")

    plt.xticks(list(x), palavras)
    plt.ylabel("Speedup médio")
    plt.title("Speedup médio por palavra")
    plt.grid(True, axis="y", alpha=0.3)
    plt.legend()

    salvar_grafico("speedup_por_palavra.png")


def grafico_tempo_medio_por_palavra(df):
    palavras = sorted(df["palavra"].unique())
    x = range(len(palavras))
    largura = 0.2

    resumo = (
        df.groupby(["implementacao", "palavra"], as_index=False)
        .agg({
            "tempo_seq": "mean",
            "tempo_par": "mean"
        })
    )

    c_seq_vals = []
    c_par_vals = []
    j_seq_vals = []
    j_par_vals = []

    for palavra in palavras:
        c_row = resumo[
            (resumo["implementacao"] == "C") &
            (resumo["palavra"] == palavra)
        ]
        j_row = resumo[
            (resumo["implementacao"] == "Java") &
            (resumo["palavra"] == palavra)
        ]

        c_seq_vals.append(float(c_row["tempo_seq"].iloc[0]) if not c_row.empty else 0.0)
        c_par_vals.append(float(c_row["tempo_par"].iloc[0]) if not c_row.empty else 0.0)
        j_seq_vals.append(float(j_row["tempo_seq"].iloc[0]) if not j_row.empty else 0.0)
        j_par_vals.append(float(j_row["tempo_par"].iloc[0]) if not j_row.empty else 0.0)

    plt.figure(figsize=(11, 5.5))
    plt.bar([i - 1.5 * largura for i in x], c_seq_vals, width=largura, label="C Seq")
    plt.bar([i - 0.5 * largura for i in x], c_par_vals, width=largura, label="C Par")
    plt.bar([i + 0.5 * largura for i in x], j_seq_vals, width=largura, label="Java Seq")
    plt.bar([i + 1.5 * largura for i in x], j_par_vals, width=largura, label="Java Par")

    plt.xticks(list(x), palavras)
    plt.ylabel("Tempo médio (s)")
    plt.title("Tempo médio por palavra (C e Java: sequencial vs paralelo)")
    plt.grid(True, axis="y", alpha=0.3)
    plt.legend(ncol=2)

    salvar_grafico("tempo_medio_por_palavra_seq_par.png")


def gerar_tabela_tempo_por_palavra(df):
    resumo_palavra = (
        df.groupby(["implementacao", "palavra"], as_index=False)
        .agg({
            "tempo_seq": "mean",
            "tempo_par": "mean"
        })
        .sort_values(["palavra", "implementacao"])
    )

    caminho_csv = os.path.join(PASTA_SAIDA, "tabela_tempo_medio_por_palavra.csv")
    resumo_palavra.to_csv(caminho_csv, sep=";", index=False)
    print(f"Tabela de tempo médio por palavra salva: {caminho_csv}")

    return resumo_palavra


def grafico_comparacao_seq_vs_par(df, implementacao="C", threads_referencia=THREADS_REFERENCIA):
    plt.figure(figsize=(9, 5))

    sub = df[
        (df["implementacao"] == implementacao) &
        (df["threads"] == threads_referencia)
    ]

    seq = (
        sub.groupby("tamanho_arquivo_gb", as_index=False)["tempo_seq"]
        .mean()
        .sort_values("tamanho_arquivo_gb")
    )

    par = (
        sub.groupby("tamanho_arquivo_gb", as_index=False)["tempo_par"]
        .mean()
        .sort_values("tamanho_arquivo_gb")
    )

    plt.plot(
        seq["tamanho_arquivo_gb"],
        seq["tempo_seq"],
        marker="o",
        label="Sequencial"
    )

    plt.plot(
        par["tamanho_arquivo_gb"],
        par["tempo_par"],
        marker="o",
        label="Paralelo"
    )

    plt.xlabel("Tamanho do arquivo (GB)")
    plt.ylabel("Tempo médio (s)")
    plt.title(f"Comparação sequencial vs paralelo - {implementacao} ({threads_referencia} threads)")
    plt.grid(True, alpha=0.3)
    plt.legend()

    salvar_grafico(f"comparacao_seq_vs_par_{implementacao}_{threads_referencia}_threads.png")


def gerar_tabelas_resumo(df):
    resumo = (
        df.groupby(["implementacao", "tamanho_arquivo_gb"], as_index=False)
        .agg({
            "tempo_seq": "mean",
            "tempo_par": "mean",
            "speedup": "mean"
        })
        .sort_values(["implementacao", "tamanho_arquivo_gb"])
    )

    caminho_csv = os.path.join(PASTA_SAIDA, "tabela_resumo.csv")
    resumo.to_csv(caminho_csv, sep=";", index=False)
    print(f"Tabela resumo salva: {caminho_csv}")

    return resumo


def gerar_tabela_latex(resumo):
    caminho_tex = os.path.join(PASTA_SAIDA, "tabelas_resultados.tex")

    c = resumo[resumo["implementacao"] == "C"].copy()
    j = resumo[resumo["implementacao"] == "Java"].copy()

    tamanhos = sorted(resumo["tamanho_arquivo_gb"].unique())

    with open(caminho_tex, "w", encoding="utf-8") as f:
        f.write("% Tabelas geradas automaticamente\n\n")

        f.write("\\begin{table}[H]\n")
        f.write("\\centering\n")
        f.write("\\caption{Tempo médio de execução por tamanho de arquivo}\n")
        f.write("\\begin{tabular}{ccccc}\n")
        f.write("\\toprule\n")
        f.write("Arquivo (GB) & Java Seq & Java Par & C Seq & C Par \\\\\n")
        f.write("\\midrule\n")

        for t in tamanhos:
            j_row = j[j["tamanho_arquivo_gb"] == t]
            c_row = c[c["tamanho_arquivo_gb"] == t]

            j_seq = j_row["tempo_seq"].iloc[0] if not j_row.empty else 0
            j_par = j_row["tempo_par"].iloc[0] if not j_row.empty else 0
            c_seq = c_row["tempo_seq"].iloc[0] if not c_row.empty else 0
            c_par = c_row["tempo_par"].iloc[0] if not c_row.empty else 0

            f.write(f"{t:g} & {j_seq:.3f} & {j_par:.3f} & {c_seq:.3f} & {c_par:.3f} \\\\\n")

        f.write("\\bottomrule\n")
        f.write("\\end{tabular}\n")
        f.write("\\end{table}\n\n")

        f.write("\\begin{table}[H]\n")
        f.write("\\centering\n")
        f.write("\\caption{Speedup médio por tamanho de arquivo}\n")
        f.write("\\begin{tabular}{ccc}\n")
        f.write("\\toprule\n")
        f.write("Arquivo (GB) & Java & C \\\\\n")
        f.write("\\midrule\n")

        for t in tamanhos:
            j_row = j[j["tamanho_arquivo_gb"] == t]
            c_row = c[c["tamanho_arquivo_gb"] == t]

            j_sp = j_row["speedup"].iloc[0] if not j_row.empty else 0
            c_sp = c_row["speedup"].iloc[0] if not c_row.empty else 0

            f.write(f"{t:g} & {j_sp:.3f} & {c_sp:.3f} \\\\\n")

        f.write("\\bottomrule\n")
        f.write("\\end{tabular}\n")
        f.write("\\end{table}\n")

    print(f"Tabelas LaTeX salvas: {caminho_tex}")


def main():
    garantir_pasta_saida()
    df = carregar_dados()

    if df.empty:
        print("Nenhum dado válido encontrado em resultados.csv")
        return

    grafico_tempo_sequencial_por_tamanho(df)
    grafico_tempo_paralelo_por_tamanho(df, threads_referencia=THREADS_REFERENCIA)
    grafico_speedup_por_threads(df)
    grafico_speedup_medio_por_tamanho(df)
    grafico_speedup_por_palavra(df)
    grafico_tempo_medio_por_palavra(df)
    grafico_comparacao_seq_vs_par(df, implementacao="C", threads_referencia=THREADS_REFERENCIA)
    grafico_comparacao_seq_vs_par(df, implementacao="Java", threads_referencia=THREADS_REFERENCIA)

    resumo = gerar_tabelas_resumo(df)
    gerar_tabela_tempo_por_palavra(df)
    gerar_tabela_latex(resumo)

    print("\nTodos os gráficos e tabelas foram gerados com sucesso.")


if __name__ == "__main__":
    main()