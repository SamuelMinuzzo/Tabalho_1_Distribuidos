import subprocess
import os
import numpy as np
import time
from pathlib import Path

# Configurações do experimento
EXECUTAVEL = "main.exe" if os.name == 'nt' else "./main"
PALAVRAS_TESTE = ["a", "de", "como", "itaguai", "Bacamarte", "Evarista"]
ARQUIVOS = [
    "arquivo0.1GB.txt", 
    "arquivo0.5GB.txt", 
    "arquivo1GB.txt"]
#    "arquivo2GB.txt",
 #   "arquivo4GB.txt",
  #  "arquivo6GB.txt",
   # "arquivo8GB.txt",
    #"arquivo10GB.txt",
    #"arquivo15GB.txt",

#]
LISTA_THREADS = [1, 2, 4, 8, 12, 16, 24, 32, 40, 50] 
REPETICOES = 3 
OUTPUT_FILE = "resultados.csv"

def validar_arquivos():
    """Verifica se os arquivos de teste existem"""
    faltantes = [a for a in ARQUIVOS if not os.path.exists(a)]
    if faltantes:
        print(f"  Arquivos não encontrados: {faltantes}")
        return False
    return True

def executar_teste(arquivo, palavra, threads):
    try:
        resultado = subprocess.run(
            [EXECUTAVEL, arquivo, palavra, str(threads)],
            capture_output=True,
            text=True,
            timeout=300
        )
        
        if resultado.returncode == 0:
            dados = resultado.stdout.strip().split(',')
            if len(dados) >= 8:
                return {
                    'tempo_seq': float(dados[5]),
                    'tempo_par': float(dados[6]),
                    'count': float(dados[7]),
                    'sucesso': True
                }
        
        return {'sucesso': False, 'erro': resultado.stderr[:100]}
    
    except subprocess.TimeoutExpired:
        return {'sucesso': False, 'erro': 'TIMEOUT'}
    except Exception as e:
        return {'sucesso': False, 'erro': str(e)[:100]}

def formatar_tempo(segundos):
    """Formata tempo em unidade legível"""
    if segundos < 1:
        return f"{segundos*1000:.2f}ms"
    elif segundos < 60:
        return f"{segundos:.2f}s"
    else:
        return f"{segundos/60:.2f}min"

def main():

    if not validar_arquivos():
        return
    
    if not os.path.exists(EXECUTAVEL):
        print(f" Executável não encontrado: {EXECUTAVEL}")
        return
    
    total_testes = len(ARQUIVOS) * len(PALAVRAS_TESTE) * len(LISTA_THREADS) * REPETICOES
    print(f"\n{'='*80}")
    print(f"BATERIA DE TESTES INICIADA")
    print(f"{'='*80}")
    print(f" Configuração:")
    print(f"   Arquivos: {len(ARQUIVOS)}")
    print(f"   Palavras: {len(PALAVRAS_TESTE)}")
    print(f"   Threads por teste: {len(LISTA_THREADS)}")
    print(f"   Repetições: {REPETICOES}")
    print(f"   Total de execuções: {total_testes}")
    print(f"{'='*80}\n")
    
    # Escreve cabeçalho CSV
    with open(OUTPUT_FILE, "w") as f:
        f.write("arquivo;palavra;tam_palavra;threads;tamanho_arquivo (GB);tempo_sequencial (s);tempo_paralelo (s);ocorrencias\n")
    
    contador_global = 0
    tempo_inicio_total = time.time()
    
    for arquivo in ARQUIVOS:
        tamanho_arquivo = os.path.getsize(arquivo)
        tamanho_mb = tamanho_arquivo / (1024**3)
        
        for palavra in PALAVRAS_TESTE:
            print(f"\n{'─'*80}")
            print(f" {arquivo} ({tamanho_mb:.2f}GB) |  Palavra: '{palavra}'")
            print(f"{'─'*80}")
            
            for threads in LISTA_THREADS:
                tempos_seq = []      # COLETA TODOS OS TEMPOS SEQUENCIAIS
                tempos_par = []
                ocorrencias = []
                
                for repeticao in range(REPETICOES):
                    contador_global += 1
                    percentual = (contador_global / total_testes) * 100
                    
                    status_bar = f"[{'='*40}{' '*10}]" if percentual >= 100 else \
                                f"[{'='*int(percentual/2.5)}{' '*int(40-percentual/2.5)}]"
                    
                    print(f"  Threads: {threads:2d} | Rep {repeticao+1}/{REPETICOES} | {percentual:5.1f}% {status_bar}", 
                          end=' ', flush=True)
                    
                    resultado = executar_teste(arquivo, palavra, threads)
                    
                    if resultado['sucesso']:
                        tempo_seq = resultado['tempo_seq']
                        tempo_par = resultado['tempo_par']
                        count = resultado['count']
                        
                        tempos_seq.append(tempo_seq)      # ADICIONA À LISTA
                        tempos_par.append(tempo_par)
                        ocorrencias.append(count)
                        
                        print(f"✓ ({formatar_tempo(tempo_par)})")
                    else:
                        print(f"✗ ({resultado['erro']})")
                
                # Calcula estatísticas
                if tempos_par and ocorrencias and tempos_seq:
                    tempo_seq_medio = np.mean(tempos_seq)       
                    tempo_par_medio = np.mean(tempos_par)
                    ocorr_media = np.mean(ocorrencias)
                    
                    # Formata saída
                    print(f"    Seq: {formatar_tempo(tempo_seq_medio)} | Par: {formatar_tempo(tempo_par_medio)}")
                    tamanho_gb = tamanho_arquivo / (1024**3)
                    # Salva no CSV - USANDO A MÉDIA DO SEQUENCIAL
                    linha = f"{arquivo};{palavra};{len(palavra)};{threads};{tamanho_gb:.3f};{tempo_seq_medio:.6f};{tempo_par_medio:.6f};{ocorr_media:.2f}\n"
                    with open(OUTPUT_FILE, "a") as f:
                        f.write(linha)
    
    tempo_total = time.time() - tempo_inicio_total
    
    print(f"\n{'='*80}")
    print(f"TESTE CONCLUÍDO COM SUCESSO!")
    print(f"{'='*80}")
    print(f"Resultados salvos em: {OUTPUT_FILE}")
    print(f"Tempo total: {formatar_tempo(tempo_total)}")
    print(f"{'='*80}\n")

if __name__ == "__main__":
    main()