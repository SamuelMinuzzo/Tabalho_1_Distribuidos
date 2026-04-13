import os
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from main import parse_tamanho, copiar_arquivo_repetindo

try:
    import windnd
except ImportError:
    windnd = None


def montar_tamanho(valor, unidade):
    valor = valor.strip()
    if not valor:
        raise ValueError("Informe o tamanho desejado.")
    return f"{valor}{unidade}"


def sugerir_saida(origem):
    base, ext = os.path.splitext(origem)
    if not ext:
        ext = ".bin"
    return f"{base}_gerado{ext}"


class App:
    def __init__(self, root):
        self.root = root
        self.root.title("Gerador de Arquivo")
        self.root.geometry("720x290")
        self.root.resizable(False, False)

        self.origem_var = tk.StringVar()
        self.saida_var = tk.StringVar()
        self.valor_var = tk.StringVar(value="10")
        self.unidade_var = tk.StringVar(value="GB")
        self.status_var = tk.StringVar(value="Status: pronto")

        self._criar_layout()
        self._configurar_drag_drop()

    def _criar_layout(self):
        frame = ttk.Frame(self.root, padding=16)
        frame.pack(fill="both", expand=True)

        ttk.Label(frame, text="Gerador de Arquivo Grande", font=("Segoe UI", 14, "bold")).grid(
            row=0, column=0, columnspan=3, sticky="w", pady=(0, 10)
        )

        ttk.Label(frame, text="Arquivo de origem (arraste para a janela ou clique em Procurar):").grid(
            row=1, column=0, columnspan=3, sticky="w"
        )

        ttk.Entry(frame, textvariable=self.origem_var, width=75).grid(row=2, column=0, sticky="we", pady=4)
        ttk.Button(frame, text="Procurar", command=self.escolher_origem).grid(row=2, column=1, padx=8)

        ttk.Label(frame, text="Arquivo de saída:").grid(row=3, column=0, columnspan=3, sticky="w", pady=(8, 0))
        ttk.Entry(frame, textvariable=self.saida_var, width=75).grid(row=4, column=0, sticky="we", pady=4)
        ttk.Button(frame, text="Salvar como", command=self.escolher_saida).grid(row=4, column=1, padx=8)

        ttk.Label(frame, text="Tamanho desejado:").grid(row=5, column=0, sticky="w", pady=(10, 0))
        ttk.Entry(frame, textvariable=self.valor_var, width=10).grid(row=5, column=0, sticky="w", padx=(120, 0), pady=(10, 0))
        ttk.Combobox(frame, textvariable=self.unidade_var, values=["KB", "MB", "GB", "TB"], width=6, state="readonly").grid(
            row=5, column=0, sticky="w", padx=(200, 0), pady=(10, 0)
        )

        ttk.Button(frame, text="Gerar arquivo", command=self.gerar_arquivo).grid(row=6, column=0, sticky="w", pady=16)
        ttk.Button(frame, text="Sair", command=self.root.destroy).grid(row=6, column=1, sticky="w", pady=16)

        ttk.Label(frame, textvariable=self.status_var, foreground="#1f2937").grid(row=7, column=0, columnspan=3, sticky="w")

        frame.columnconfigure(0, weight=1)

    def _configurar_drag_drop(self):
        if windnd is None:
            self.status_var.set("Status: pronto (drag-and-drop indisponível: instale windnd)")
            return

        def on_drop(files):
            if not files:
                return
            caminho = files[0].decode("utf-8", errors="ignore")
            caminho = caminho.strip().strip('"')
            if os.path.isfile(caminho):
                self.origem_var.set(caminho)
                if not self.saida_var.get().strip():
                    self.saida_var.set(sugerir_saida(caminho))
                self.status_var.set("Status: arquivo carregado por arrastar e soltar")

        windnd.hook_dropfiles(self.root, func=on_drop)

    def escolher_origem(self):
        caminho = filedialog.askopenfilename(title="Selecione o arquivo de origem")
        if caminho:
            self.origem_var.set(caminho)
            if not self.saida_var.get().strip():
                self.saida_var.set(sugerir_saida(caminho))

    def escolher_saida(self):
        caminho = filedialog.asksaveasfilename(
            title="Escolha o arquivo de saída",
            defaultextension=".bin",
            filetypes=[("Arquivo binário", "*.bin"), ("Todos os arquivos", "*.*")],
        )
        if caminho:
            self.saida_var.set(caminho)

    def gerar_arquivo(self):
        origem = self.origem_var.get().strip().strip('"')
        saida = self.saida_var.get().strip().strip('"')

        try:
            if not origem:
                raise ValueError("Selecione um arquivo de origem.")
            if not os.path.exists(origem):
                raise ValueError("Arquivo de origem não encontrado.")
            if not saida:
                raise ValueError("Informe o arquivo de saída.")

            tamanho_txt = montar_tamanho(self.valor_var.get(), self.unidade_var.get())
            tamanho_desejado = parse_tamanho(tamanho_txt)

            self.status_var.set("Status: gerando arquivo... aguarde")
            self.root.update_idletasks()

            tamanho_final = copiar_arquivo_repetindo(origem, saida, tamanho_desejado)
            self.status_var.set(f"Status: concluído. Arquivo criado com {tamanho_final} bytes")
            messagebox.showinfo("Concluído", f"Arquivo criado com sucesso:\n{saida}\n\nTamanho: {tamanho_final} bytes")
        except Exception as e:
            self.status_var.set(f"Status: erro - {e}")
            messagebox.showerror("Erro", str(e))


def main():
    root = tk.Tk()
    App(root)
    root.mainloop()


if __name__ == "__main__":
    main()
