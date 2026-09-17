#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>


/*
 * Handler do SIGUSR1
 */
static void handler_usr1(int sig) {

    (void)sig;

    const char msg[] =
        "Recebi SIGUSR1: primeira acao.\n";

    write(
        STDOUT_FILENO,
        msg,
        sizeof(msg) - 1
    );
}


/*
 * Handler do SIGUSR2
 */
static void handler_usr2(int sig) {

    (void)sig;

    const char msg[] =
        "Recebi SIGUSR2: segunda acao.\n";

    write(
        STDOUT_FILENO,
        msg,
        sizeof(msg) - 1
    );
}


/*
 * Handler do SIGTERM
 *
 * Este sinal encerra o processo.
 */
static void handler_term(int sig) {

    (void)sig;

    const char msg[] =
        "Recebi SIGTERM: encerrando o processo.\n";

    write(
        STDOUT_FILENO,
        msg,
        sizeof(msg) - 1
    );

    _exit(EXIT_SUCCESS);
}


/*
 * Funcao auxiliar para instalar os handlers.
 */
static int instala_handler(
    int sinal,
    void (*funcao)(int)
) {

    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = funcao;

    sigemptyset(&sa.sa_mask);

    sa.sa_flags = 0;

    return sigaction(
        sinal,
        &sa,
        NULL
    );
}


int main(int argc, char *argv[]) {

    if (
        argc != 2 ||
        (
            strcmp(argv[1], "busy") != 0 &&
            strcmp(argv[1], "blocking") != 0
        )
    ) {

        fprintf(
            stderr,
            "Uso: %s <busy|blocking>\n",
            argv[0]
        );

        return EXIT_FAILURE;
    }


    /*
     * Instala os tres handlers.
     */
    if (
        instala_handler(
            SIGUSR1,
            handler_usr1
        ) == -1 ||

        instala_handler(
            SIGUSR2,
            handler_usr2
        ) == -1 ||

        instala_handler(
            SIGTERM,
            handler_term
        ) == -1
    ) {

        perror("sigaction");

        return EXIT_FAILURE;
    }


    printf(
        "PID do receptor: %d\n",
        getpid()
    );

    printf(
        "Modo de espera: %s\n",
        argv[1]
    );

    fflush(stdout);


    /*
     * BUSY WAIT
     */
    if (
        strcmp(argv[1], "busy") == 0
    ) {

        for (;;) {

            /*
             * O processo fica executando
             * continuamente enquanto espera.
             */
        }
    }


    /*
     * BLOCKING WAIT
     */
    else {

        for (;;) {

            /*
             * pause() bloqueia o processo.
             *
             * Ele somente volta a executar
             * quando algum sinal chega.
             */
            pause();
        }
    }
}