#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <errno.h>


#define M_PADRAO 100000L
#define VALOR_MAXIMO 10000000U


typedef struct {

    int *buffer;

    size_t N;

    size_t entrada;
    size_t saida;
    size_t ocupacao;


    /*
     * Semaforos
     */
    sem_t vazios;
    sem_t cheios;
    sem_t mutex;


    /*
     * Quantidade de numeros.
     */
    long M;

    long reservados;
    long consumidos;


    /*
     * Mostra ou nao cada numero.
     */
    int verbose;


    /*
     * Dados utilizados para
     * o grafico de ocupacao.
     */
    int log_habilitado;

    int *historico_ocupacao;

    double *historico_tempo;

    size_t historico_tamanho;

    size_t historico_capacidade;


    struct timespec inicio;

} Compartilhado;


typedef struct {

    Compartilhado *c;

    int id;

} ThreadArg;


/*
 * sem_wait pode ser interrompido
 * por um sinal.
 *
 * Esta funcao simplesmente tenta
 * novamente nesse caso.
 */
static void sem_wait_sem_interrupcao(
    sem_t *s
) {

    while (sem_wait(s) == -1) {

        if (errno != EINTR) {

            perror("sem_wait");

            exit(EXIT_FAILURE);
        }
    }
}


/*
 * Calcula o tempo desde o inicio
 * da execucao.
 */
static double segundos_desde_inicio(
    const struct timespec *inicio
) {

    struct timespec agora;

    clock_gettime(
        CLOCK_MONOTONIC,
        &agora
    );


    return
        (double)
        (
            agora.tv_sec -
            inicio->tv_sec
        )
        +
        (double)
        (
            agora.tv_nsec -
            inicio->tv_nsec
        ) / 1e9;
}


/*
 * Registra a ocupacao atual
 * para posteriormente gerar
 * os graficos.
 *
 * Deve ser chamada com mutex.
 */
static void registra_ocupacao(
    Compartilhado *c
) {

    if (!c->log_habilitado)
        return;


    if (
        c->historico_tamanho >=
        c->historico_capacidade
    )
        return;


    size_t i =
        c->historico_tamanho++;


    c->historico_ocupacao[i] =
        (int)c->ocupacao;


    c->historico_tempo[i] =
        segundos_desde_inicio(
            &c->inicio
        );
}


/*
 * Verificacao de numero primo.
 */
static int eh_primo(
    uint32_t n
) {

    if (n < 2)
        return 0;

    if (n == 2)
        return 1;

    if (n % 2 == 0)
        return 0;


    for (
        uint32_t d = 3;
        d <= n / d;
        d += 2
    ) {

        if (n % d == 0)
            return 0;
    }


    return 1;
}


/*
 * Gerador pseudoaleatorio
 * independente para cada thread.
 */
static uint32_t xorshift32(
    uint32_t *estado
) {

    uint32_t x = *estado;


    if (x == 0)
        x = 0xA341316Cu;


    x ^= x << 13;

    x ^= x >> 17;

    x ^= x << 5;


    *estado = x;


    return x;
}


/*
 * Coloca um valor no buffer.
 */
static void coloca_no_buffer(
    Compartilhado *c,
    int valor
) {

    /*
     * Precisa existir uma
     * posicao livre.
     */
    sem_wait_sem_interrupcao(
        &c->vazios
    );


    /*
     * Apenas uma thread pode
     * alterar o buffer.
     */
    sem_wait_sem_interrupcao(
        &c->mutex
    );


    c->buffer[c->entrada] =
        valor;


    c->entrada =
        (c->entrada + 1)
        % c->N;


    c->ocupacao++;


    registra_ocupacao(c);


    /*
     * Libera regiao critica.
     */
    sem_post(
        &c->mutex
    );


    /*
     * Agora existe mais
     * uma posicao ocupada.
     */
    sem_post(
        &c->cheios
    );
}


/*
 * Remove um valor do buffer.
 */
static int retira_do_buffer(
    Compartilhado *c
) {

    /*
     * Precisa existir
     * algum elemento.
     */
    sem_wait_sem_interrupcao(
        &c->cheios
    );


    sem_wait_sem_interrupcao(
        &c->mutex
    );


    int valor =
        c->buffer[c->saida];


    c->saida =
        (c->saida + 1)
        % c->N;


    c->ocupacao--;


    registra_ocupacao(c);


    sem_post(
        &c->mutex
    );


    /*
     * Agora existe mais
     * uma posicao livre.
     */
    sem_post(
        &c->vazios
    );


    return valor;
}


/*
 * THREAD PRODUTORA
 */
static void *produtor(
    void *arg
) {

    ThreadArg *a =
        (ThreadArg *)arg;


    Compartilhado *c =
        a->c;


    uint32_t estado =
        (uint32_t)time(NULL)
        ^
        (uint32_t)
        (
            a->id *
            0x9E3779B9u
        )
        ^
        (uint32_t)
        (uintptr_t)
        pthread_self();


    for (;;) {

        /*
         * Descobre se ainda
         * precisamos produzir.
         */
        sem_wait_sem_interrupcao(
            &c->mutex
        );


        if (
            c->reservados >=
            c->M
        ) {

            sem_post(
                &c->mutex
            );

            break;
        }


        c->reservados++;


        sem_post(
            &c->mutex
        );


        /*
         * Numero entre
         * 1 e 10^7.
         */
        int valor =
            (int)
            (
                1U +
                (
                    xorshift32(
                        &estado
                    )
                    %
                    VALOR_MAXIMO
                )
            );


        coloca_no_buffer(
            c,
            valor
        );
    }


    return NULL;
}


/*
 * THREAD CONSUMIDORA
 */
static void *consumidor(
    void *arg
) {

    ThreadArg *a =
        (ThreadArg *)arg;


    Compartilhado *c =
        a->c;


    for (;;) {

        int valor =
            retira_do_buffer(c);


        /*
         * Zero e usado
         * como sentinela.
         */
        if (valor == 0)
            break;


        int primo =
            eh_primo(
                (uint32_t)valor
            );


        sem_wait_sem_interrupcao(
            &c->mutex
        );


        c->consumidos++;


        long ordem =
            c->consumidos;


        sem_post(
            &c->mutex
        );


        if (c->verbose) {

            printf(
                "Consumidor %d: "
                "#%ld valor=%d -> %s primo\n",

                a->id,

                ordem,

                valor,

                primo
                    ? "e"
                    : "nao e"
            );
        }
    }


    return NULL;
}


/*
 * Salva o historico de ocupacao
 * em CSV.
 */
static int salva_historico(
    const char *arquivo,
    Compartilhado *c
) {

    FILE *f =
        fopen(
            arquivo,
            "w"
        );


    if (!f) {

        perror(
            "fopen do arquivo de ocupacao"
        );

        return -1;
    }


    fprintf(
        f,
        "evento,tempo_s,ocupacao\n"
    );


    for (
        size_t i = 0;
        i < c->historico_tamanho;
        i++
    ) {

        fprintf(
            f,
            "%zu,%.9f,%d\n",

            i + 1,

            c->historico_tempo[i],

            c->historico_ocupacao[i]
        );
    }


    fclose(f);


    return 0;
}


/*
 * Converte argumento para inteiro.
 */
static long le_long_positivo(
    const char *texto,
    const char *nome
) {

    char *fim;


    long v =
        strtol(
            texto,
            &fim,
            10
        );


    if (
        *fim != '\0' ||
        v <= 0
    ) {

        fprintf(
            stderr,
            "%s invalido: %s\n",
            nome,
            texto
        );

        exit(EXIT_FAILURE);
    }


    return v;
}


int main(
    int argc,
    char *argv[]
) {

    if (
        argc < 5 ||
        argc > 7
    ) {

        fprintf(
            stderr,

            "Uso: %s "
            "<N> <Np> <Nc> "
            "<ocupacao.csv|-> "
            "[verbose 0|1] "
            "[M]\n",

            argv[0]
        );

        return EXIT_FAILURE;
    }


    long N_l =
        le_long_positivo(
            argv[1],
            "N"
        );


    long Np_l =
        le_long_positivo(
            argv[2],
            "Np"
        );


    long Nc_l =
        le_long_positivo(
            argv[3],
            "Nc"
        );


    /*
     * Normalmente M = 100000.
     *
     * Permitimos alterar apenas
     * para testes pequenos.
     */
    long M =
        (argc >= 7)

        ? le_long_positivo(
            argv[6],
            "M"
        )

        : M_PADRAO;


    int verbose =
        (argc >= 6)

        ? atoi(argv[5]) != 0

        : 0;


    int log_habilitado =
        strcmp(
            argv[4],
            "-"
        ) != 0;


    Compartilhado c;


    memset(
        &c,
        0,
        sizeof(c)
    );


    c.N =
        (size_t)N_l;


    c.M = M;


    c.verbose =
        verbose;


    c.log_habilitado =
        log_habilitado;


    /*
     * Cria buffer compartilhado.
     */
    c.buffer =
        calloc(
            c.N,
            sizeof(int)
        );


    if (!c.buffer) {

        perror("calloc buffer");

        return EXIT_FAILURE;
    }


    /*
     * Vetores para o grafico.
     */
    if (log_habilitado) {

        c.historico_capacidade =
            (size_t)
            (
                2 * M +
                2 * Nc_l +
                16
            );


        c.historico_ocupacao =
            malloc(
                c.historico_capacidade
                *
                sizeof(int)
            );


        c.historico_tempo =
            malloc(
                c.historico_capacidade
                *
                sizeof(double)
            );


        if (
            !c.historico_ocupacao ||
            !c.historico_tempo
        ) {

            perror(
                "malloc historico"
            );

            return EXIT_FAILURE;
        }
    }


    /*
     * Inicializacao dos semaforos.
     */

    /*
     * Inicialmente existem N
     * posicoes livres.
     */
    if (
        sem_init(
            &c.vazios,
            0,
            (unsigned int)c.N
        ) == -1
    ) {

        perror("sem_init");

        return EXIT_FAILURE;
    }


    /*
     * Nenhuma posicao esta
     * ocupada inicialmente.
     */
    if (
        sem_init(
            &c.cheios,
            0,
            0
        ) == -1
    ) {

        perror("sem_init");

        return EXIT_FAILURE;
    }


    /*
     * Mutex binario.
     */
    if (
        sem_init(
            &c.mutex,
            0,
            1
        ) == -1
    ) {

        perror("sem_init");

        return EXIT_FAILURE;
    }


    /*
     * Vetores de threads.
     */
    pthread_t *produtores =
        malloc(
            (size_t)Np_l *
            sizeof(pthread_t)
        );


    pthread_t *consumidores =
        malloc(
            (size_t)Nc_l *
            sizeof(pthread_t)
        );


    ThreadArg *args_p =
        malloc(
            (size_t)Np_l *
            sizeof(ThreadArg)
        );


    ThreadArg *args_c =
        malloc(
            (size_t)Nc_l *
            sizeof(ThreadArg)
        );


    if (
        !produtores ||
        !consumidores ||
        !args_p ||
        !args_c
    ) {

        perror("malloc threads");

        return EXIT_FAILURE;
    }


    /*
     * Inicia cronometro.
     */
    clock_gettime(
        CLOCK_MONOTONIC,
        &c.inicio
    );


    /*
     * Cria consumidores.
     */
    for (
        long i = 0;
        i < Nc_l;
        i++
    ) {

        args_c[i].c =
            &c;


        args_c[i].id =
            (int)(i + 1);


        if (
            pthread_create(
                &consumidores[i],
                NULL,
                consumidor,
                &args_c[i]
            ) != 0
        ) {

            fprintf(
                stderr,
                "Erro ao criar consumidor %ld\n",
                i + 1
            );

            return EXIT_FAILURE;
        }
    }


    /*
     * Cria produtores.
     */
    for (
        long i = 0;
        i < Np_l;
        i++
    ) {

        args_p[i].c =
            &c;


        args_p[i].id =
            (int)(i + 1);


        if (
            pthread_create(
                &produtores[i],
                NULL,
                produtor,
                &args_p[i]
            ) != 0
        ) {

            fprintf(
                stderr,
                "Erro ao criar produtor %ld\n",
                i + 1
            );

            return EXIT_FAILURE;
        }
    }


    /*
     * Aguarda todos os produtores.
     */
    for (
        long i = 0;
        i < Np_l;
        i++
    ) {

        pthread_join(
            produtores[i],
            NULL
        );
    }


    /*
     * Depois que os M numeros
     * foram produzidos, envia
     * uma sentinela para cada
     * consumidor.
     */
    for (
        long i = 0;
        i < Nc_l;
        i++
    ) {

        coloca_no_buffer(
            &c,
            0
        );
    }


    /*
     * Aguarda os consumidores.
     */
    for (
        long i = 0;
        i < Nc_l;
        i++
    ) {

        pthread_join(
            consumidores[i],
            NULL
        );
    }


    /*
     * Tempo total.
     */
    double tempo_total =
        segundos_desde_inicio(
            &c.inicio
        );


    printf(
        "RESUMO "
        "N=%zu "
        "Np=%ld "
        "Nc=%ld "
        "M=%ld "
        "consumidos=%ld\n",

        c.N,
        Np_l,
        Nc_l,
        c.M,
        c.consumidos
    );


    printf(
        "TEMPO_SEGUNDOS=%.9f\n",
        tempo_total
    );


    /*
     * Salva ocupacao.
     */
    if (log_habilitado) {

        if (
            salva_historico(
                argv[4],
                &c
            ) == 0
        ) {

            printf(
                "OCUPACAO_CSV=%s\n",
                argv[4]
            );
        }
    }


    /*
     * Limpeza.
     */

    sem_destroy(
        &c.vazios
    );

    sem_destroy(
        &c.cheios
    );

    sem_destroy(
        &c.mutex
    );


    free(c.buffer);

    free(c.historico_ocupacao);

    free(c.historico_tempo);

    free(produtores);

    free(consumidores);

    free(args_p);

    free(args_c);


    return
        c.consumidos == c.M

        ? EXIT_SUCCESS

        : EXIT_FAILURE;
}