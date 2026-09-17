#!/bin/bash

set -e

EXECUTAVEL="./pc_semaforos"

echo "N,Np,Nc,execucao,tempo_s" > tempos.csv


for N in 1 10 100 1000
do

    for PAR in \
        "1 1" \
        "1 2" \
        "1 4" \
        "1 8" \
        "2 1" \
        "4 1" \
        "8 1"
    do

        read NP NC <<< "$PAR"

        echo
        echo "================================"
        echo "N=$N Np=$NP Nc=$NC"
        echo "================================"


        #
        # As 10 execucoes utilizadas
        # no calculo da media.
        #
        for RUN in $(seq 1 10)
        do

            SAIDA=$(
                $EXECUTAVEL \
                "$N" \
                "$NP" \
                "$NC" \
                - \
                0
            )


            TEMPO=$(
                echo "$SAIDA" |
                awk -F= \
                '/TEMPO_SEGUNDOS=/{print $2}'
            )


            echo \
                "$N,$NP,$NC,$RUN,$TEMPO" \
                >> tempos.csv


            echo \
                "Execucao $RUN: $TEMPO s"

        done


        #
        # Faz uma execucao adicional
        # somente para armazenar o
        # historico de ocupacao.
        #
        ARQUIVO="ocupacao_N${N}_Np${NP}_Nc${NC}.csv"


        $EXECUTAVEL \
            "$N" \
            "$NP" \
            "$NC" \
            "$ARQUIVO" \
            0 \
            > /dev/null


        echo \
            "Ocupacao salva em $ARQUIVO"

    done

done


echo
echo "Experimentos concluidos."
echo "Resultados em tempos.csv"