#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <string.h>


#define MSG_SIZE 20


/*
 * Verifica se um numero e primo.
 */
static int eh_primo(uint64_t n) {

    if (n < 2)
        return 0;

    if (n == 2)
        return 1;

    if (n % 2 == 0)
        return 0;


    for (
        uint64_t d = 3;
        d <= n / d;
        d += 2
    ) {

        if (n % d == 0)
            return 0;
    }

    return 1;
}


/*
 * Garante que todos os bytes
 * sejam escritos no pipe.
 */
static int write_all(
    int fd,
    const void *buf,
    size_t count
) {

    const char *p =
        (const char *)buf;

    size_t total = 0;


    while (total < count) {

        ssize_t n = write(
            fd,
            p + total,
            count - total
        );


        if (n < 0) {

            if (errno == EINTR)
                continue;

            return -1;
        }


        total += (size_t)n;
    }


    return 0;
}


/*
 * Garante que exatamente
 * count bytes sejam lidos.
 */
static int read_all(
    int fd,
    void *buf,
    size_t count
) {

    char *p =
        (char *)buf;

    size_t total = 0;


    while (total < count) {

        ssize_t n = read(
            fd,
            p + total,
            count - total
        );


        if (n == 0) {

            return
                total == 0
                ? 0
                : -1;
        }


        if (n < 0) {

            if (errno == EINTR)
                continue;

            return -1;
        }


        total += (size_t)n;
    }


    return 1;
}


/*
 * Converte o numero para string
 * antes de escrever no pipe.
 */
static int envia_numero(
    int fd,
    uint64_t valor
) {

    char msg[MSG_SIZE];

    memset(
        msg,
        0,
        sizeof(msg)
    );


    int escritos = snprintf(
        msg,
        sizeof(msg),
        "%llu",
        (unsigned long long)valor
    );


    if (
        escritos < 0 ||
        escritos >= (int)sizeof(msg)
    ) {

        fprintf(
            stderr,
            "Numero grande demais.\n"
        );

        return -1;
    }


    return write_all(
        fd,
        msg,
        sizeof(msg)
    );
}


int main(
    int argc,
    char *argv[]
) {

    if (argc != 2) {

        fprintf(
            stderr,
            "Uso: %s <quantidade_de_numeros>\n",
            argv[0]
        );

        return EXIT_FAILURE;
    }


    char *fim;

    long quantidade =
        strtol(
            argv[1],
            &fim,
            10
        );


    if (
        *fim != '\0' ||
        quantidade <= 0
    ) {

        fprintf(
            stderr,
            "Quantidade invalida.\n"
        );

        return EXIT_FAILURE;
    }


    /*
     * Cria o pipe.
     *
     * fd[0] = leitura
     * fd[1] = escrita
     */
    int fd[2];


    if (pipe(fd) == -1) {

        perror("pipe");

        return EXIT_FAILURE;
    }


    /*
     * Duplica o processo.
     */
    pid_t pid = fork();


    if (pid == -1) {

        perror("fork");

        close(fd[0]);
        close(fd[1]);

        return EXIT_FAILURE;
    }


    /*
     * PROCESSO FILHO
     *
     * Sera o consumidor.
     */
    if (pid == 0) {

        /*
         * Consumidor nao escreve.
         *
         * Portanto fecha a ponta
         * de escrita.
         */
        close(fd[1]);


        char msg[MSG_SIZE];


        for (;;) {

            int r = read_all(
                fd[0],
                msg,
                sizeof(msg)
            );


            if (r == 0)
                break;


            if (r == -1) {

                perror("read");

                close(fd[0]);

                return EXIT_FAILURE;
            }


            msg[MSG_SIZE - 1] = '\0';


            uint64_t numero =
                strtoull(
                    msg,
                    NULL,
                    10
                );


            /*
             * 0 indica fim.
             */
            if (numero == 0) {

                printf(
                    "Consumidor: recebi 0, encerrando.\n"
                );

                break;
            }


            printf(
                "Consumidor: %llu %s primo.\n",
                (unsigned long long)numero,
                eh_primo(numero)
                    ? "e"
                    : "nao e"
            );
        }


        close(fd[0]);

        return EXIT_SUCCESS;
    }


    /*
     * PROCESSO PAI
     *
     * Sera o produtor.
     */

    /*
     * Produtor nao le.
     *
     * Portanto fecha a ponta
     * de leitura.
     */
    close(fd[0]);


    srand(
        (unsigned int)
        (
            time(NULL) ^
            (unsigned int)getpid()
        )
    );


    /*
     * N0 = 1
     */
    uint64_t atual = 1;


    for (
        long i = 0;
        i < quantidade;
        i++
    ) {

        /*
         * Delta entre 1 e 100.
         */
        int delta =
            1 + rand() % 100;


        /*
         * Ni = Ni-1 + Delta
         */
        atual +=
            (uint64_t)delta;


        printf(
            "Produtor: N%ld = %llu (delta = %d)\n",
            i + 1,
            (unsigned long long)atual,
            delta
        );


        /*
         * Envia para o consumidor.
         */
        if (
            envia_numero(
                fd[1],
                atual
            ) == -1
        ) {

            perror("write");

            close(fd[1]);

            waitpid(
                pid,
                NULL,
                0
            );

            return EXIT_FAILURE;
        }
    }


    /*
     * Envia zero para informar
     * que acabou.
     */
    envia_numero(
        fd[1],
        0
    );


    close(fd[1]);


    /*
     * Pai espera o filho terminar.
     */
    waitpid(
        pid,
        NULL,
        0
    );


    return EXIT_SUCCESS;
}