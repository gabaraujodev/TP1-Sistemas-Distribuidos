import csv
import glob
import re
from collections import defaultdict

import matplotlib.pyplot as plt


casos = [
    (1, 1),
    (1, 2),
    (1, 4),
    (1, 8),
    (2, 1),
    (4, 1),
    (8, 1),
]

Ns = [1, 10, 100, 1000]


# ==================================================
# 1. LEITURA DOS TEMPOS
# ==================================================

tempos = defaultdict(list)

with open("tempos.csv", newline="") as arquivo:

    leitor = csv.DictReader(arquivo)

    for linha in leitor:

        N = int(linha["N"])
        Np = int(linha["Np"])
        Nc = int(linha["Nc"])
        tempo = float(linha["tempo_s"])

        tempos[(N, Np, Nc)].append(tempo)


# ==================================================
# 2. CALCULO DOS TEMPOS MEDIOS
# ==================================================

medias = {}

for chave, valores in tempos.items():

    medias[chave] = sum(valores) / len(valores)


# ==================================================
# 3. SALVA OS TEMPOS MEDIOS
# ==================================================

with open("tempos_medios.csv", "w", newline="") as arquivo:

    escritor = csv.writer(arquivo)

    escritor.writerow([
        "N",
        "Np",
        "Nc",
        "tempo_medio_s"
    ])

    for N in Ns:

        for Np, Nc in casos:

            media = medias[(N, Np, Nc)]

            escritor.writerow([
                N,
                Np,
                Nc,
                media
            ])


# ==================================================
# 4. GRAFICO DE TEMPO MEDIO
# ==================================================

rotulos = [
    f"({Np},{Nc})"
    for Np, Nc in casos
]

x = list(range(len(casos)))

plt.figure(figsize=(10, 6))


for N in Ns:

    y = [
        medias[(N, Np, Nc)]
        for Np, Nc in casos
    ]

    plt.plot(
        x,
        y,
        marker="o",
        label=f"N={N}"
    )


plt.xticks(x, rotulos)

plt.xlabel("(Np, Nc)")

plt.ylabel("Tempo medio de execucao (s)")

plt.title("Tempo medio de execucao")

plt.grid(True)

plt.legend()

plt.tight_layout()

plt.savefig(
    "grafico_tempo_medio.png",
    dpi=200
)

plt.close()


# ==================================================
# 5. GRAFICOS DE OCUPACAO DO BUFFER
# ==================================================

arquivos = glob.glob(
    "ocupacao_N*_Np*_Nc*.csv"
)


# EXPRESSAO REGULAR CORRIGIDA
padrao = re.compile(
    r"ocupacao_N(\d+)_Np(\d+)_Nc(\d+)\.csv"
)


quantidade_graficos = 0


for nome in arquivos:

    resultado = padrao.search(nome)

    if not resultado:
        print(
            f"Nao foi possivel reconhecer: {nome}"
        )
        continue


    N = int(resultado.group(1))

    Np = int(resultado.group(2))

    Nc = int(resultado.group(3))


    tempo = []

    ocupacao = []


    with open(nome, newline="") as arquivo:

        leitor = csv.DictReader(arquivo)

        for linha in leitor:

            tempo.append(
                float(linha["tempo_s"])
            )

            ocupacao.append(
                int(linha["ocupacao"])
            )


    plt.figure(figsize=(10, 5))


    plt.plot(
        tempo,
        ocupacao
    )


    plt.xlabel(
        "Tempo (s)"
    )

    plt.ylabel(
        "Ocupacao do buffer"
    )

    plt.title(
        f"Ocupacao do buffer - "
        f"N={N}, Np={Np}, Nc={Nc}"
    )

    plt.ylim(
        0,
        N + 1
    )

    plt.grid(True)

    plt.tight_layout()


    saida = (
        f"grafico_ocupacao_"
        f"N{N}_Np{Np}_Nc{Nc}.png"
    )


    plt.savefig(
        saida,
        dpi=200
    )

    plt.close()


    quantidade_graficos += 1

    print(
        f"Gerado: {saida}"
    )


print()

print(
    "Grafico de tempo medio gerado."
)

print(
    f"Graficos de ocupacao gerados: "
    f"{quantidade_graficos}"
)

print(
    f"Total de graficos: "
    f"{quantidade_graficos + 1}"
)