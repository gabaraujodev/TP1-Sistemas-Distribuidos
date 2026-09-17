#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>

int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr,
                "Uso: %s <PID> <numero_do_sinal>\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    char *fim;

    long pid_l = strtol(argv[1], &fim, 10);

    if (*fim != '\0' || pid_l <= 0) {
        fprintf(stderr, "PID invalido.\n");
        return EXIT_FAILURE;
    }

    long sig_l = strtol(argv[2], &fim, 10);

    if (*fim != '\0' || sig_l <= 0 || sig_l > INT_MAX) {
        fprintf(stderr, "Numero de sinal invalido.\n");
        return EXIT_FAILURE;
    }

    pid_t pid = (pid_t) pid_l;
    int sig = (int) sig_l;

    /*
     * kill(pid, 0) nao envia sinal.
     * Ele serve apenas para verificar se o processo existe.
     */
    if (kill(pid, 0) == -1) {

        if (errno == ESRCH) {
            fprintf(stderr,
                    "Erro: o processo %d nao existe.\n",
                    pid);
        }
        else if (errno == EPERM) {
            fprintf(stderr,
                    "O processo existe, mas voce nao tem permissao.\n");
        }
        else {
            perror("kill(pid, 0)");
        }

        return EXIT_FAILURE;
    }

    /*
     * Agora realmente enviamos o sinal.
     */
    if (kill(pid, sig) == -1) {
        perror("Erro ao enviar sinal");
        return EXIT_FAILURE;
    }

    printf(
        "Sinal %d enviado com sucesso ao processo %d.\n",
        sig,
        pid
    );

    return EXIT_SUCCESS;
}