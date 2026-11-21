#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "monitor.h"

#define PUERTO      6666
#define BACKLOG     10
#define TAM_CODIGO  6
#define TAM_RESP    512

static int crear_socket_servidor(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(PUERTO);

    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    if (listen(fd, BACKLOG) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

static void atender_cliente(int fd_cli, struct sockaddr_in *cli_addr)
{
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli_addr->sin_addr, ip, sizeof ip);

    printf("[PID %d] Cliente conectado desde %s:%d\n",
           (int)getpid(), ip, ntohs(cli_addr->sin_port));
    fflush(stdout);

    char codigo[TAM_CODIGO + 1];
    memset(codigo, 0, sizeof codigo);

    ssize_t total = 0;
    while (total < TAM_CODIGO) {
        ssize_t n = read(fd_cli, codigo + total, TAM_CODIGO - total);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("read");
            return;
        }
        if (n == 0) {
            printf("[PID %d] Cliente cerro antes de enviar el codigo.\n",
                   (int)getpid());
            fflush(stdout);
            return;
        }
        total += n;
    }

    printf("[PID %d] Solicitud recibida: %s\n", (int)getpid(), codigo);
    fflush(stdout);

    char resp[TAM_RESP];
    const Episodio *ep = monitor_buscar(codigo);
    int n;
    if (ep == NULL) {
        n = snprintf(resp, sizeof resp,
                     "ERROR: codigo '%s' no encontrado\n", codigo);
    } else {
        n = monitor_formatear(ep, resp, sizeof resp);
    }
    if (n < 0) {
        fprintf(stderr, "[PID %d] Error formateando respuesta\n",
                (int)getpid());
        return;
    }

    ssize_t enviado = write(fd_cli, resp, (size_t)n + 1);
    if (enviado < 0) {
        if (errno != EINTR)
            perror("write");
        return;
    }

    printf("[PID %d] Respuesta enviada (%zd bytes).\n",
           (int)getpid(), enviado);
    fflush(stdout);
}

static void manejador_sigchld(int sig)
{
    (void)sig;   /* no lo usamos, pero la firma lo exige */

    /*
     * Cosechar TODOS los hijos terminados.
     * El bucle es obligatorio: SIGCHLD no se encola, una sola
     * senal puede representar varias terminaciones.
     *
     * waitpid con WNOHANG devuelve:
     *   > 0  -> PID del hijo cosechado, seguir en el bucle
     *     0  -> hay hijos pero ninguno terminado, salir
     *    -1  -> error (normalmente ECHILD = no quedan hijos), salir
     */
    int estado;
    while (waitpid(-1, &estado, WNOHANG) > 0)
        ;
}


static int instalar_manejadores(void)
{
    struct sigaction sa;

    /* ---------- SIGINT: ignorar ---------- */
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction SIGINT");
        return -1;
    }

    /* ---------- SIGCHLD: cosechar hijos ---------- */
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = manejador_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction SIGCHLD");
        return -1;
    }

    return 0;
}

int main(void)
{
    if (instalar_manejadores() != 0) {
        fprintf(stderr, "Error instalando manejadores de senales\n");
        return 1;
    }

    if (monitor_init() != 0) {
        fprintf(stderr, "Error inicializando la base de datos\n");
        return 1;
    }

    int fd_srv = crear_socket_servidor();
    if (fd_srv < 0)
        return 1;

    printf("[PID %d] Servidor escuchando en puerto %d...\n",
           (int)getpid(), PUERTO);
    fflush(stdout);

    for (;;) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof cli_addr;

        int fd_cli = accept(fd_srv, (struct sockaddr *)&cli_addr, &cli_len);
        if (fd_cli < 0) {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }

        fflush(stdout);

        /*
         * Tubería por conexión. Se crea ANTES del fork para que el hijo
         * la herede. fd_pipe[0] = lectura (padre), fd_pipe[1] = escritura (hijo).
         */
        int fd_pipe[2];
        if (pipe(fd_pipe) < 0) {
            perror("pipe");
            close(fd_cli);
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(fd_pipe[0]);
            close(fd_pipe[1]);
            close(fd_cli);
            continue;
        }

        if (pid == 0) {
            /* ---------- HIJO ---------- */
            close(fd_srv);
            close(fd_pipe[0]);              /* el hijo solo escribe */

            /*
             * Construir y enviar "PID IP\n" en un solo write.
             * Al ser < PIPE_BUF, es atomico segun POSIX.
             */
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &cli_addr.sin_addr, ip, sizeof ip);

            char msg[64];
            int n = snprintf(msg, sizeof msg, "%d %s\n",
                             (int)getpid(), ip);
            if (n > 0 && (size_t)n < sizeof msg) {
                ssize_t w = write(fd_pipe[1], msg, (size_t)n);
                (void)w;   /* si falla, el padre no recibira nada, no es critico */
            }
            close(fd_pipe[1]);

            atender_cliente(fd_cli, &cli_addr);
            close(fd_cli);
            _exit(0);
        }

        /* ---------- PADRE ---------- */
        close(fd_cli);
        close(fd_pipe[1]);                  /* el padre solo lee */

        /*
         * Leer la linea "PID IP\n" que envia el hijo.
         * El hijo la escribe justo tras el fork, asi que esta lectura
         * no se bloquea salvo microsegundos.
         */
        FILE *f = fdopen(fd_pipe[0], "r");
        if (f == NULL) {
            perror("fdopen");
            close(fd_pipe[0]);
        } else {
            char linea[128];
            if (fgets(linea, sizeof linea, f) != NULL) {
                int pid_hijo;
                char ip[INET_ADDRSTRLEN];
                if (sscanf(linea, "%d %45s", &pid_hijo, ip) == 2) {
                    printf("[PID %d] Hijo %d atiende a cliente %s\n",
                           (int)getpid(), pid_hijo, ip);
                    fflush(stdout);
                } else {
                    fprintf(stderr, "[PID %d] Mensaje malformado del hijo: %s",
                            (int)getpid(), linea);
                }
            } else {
                /* El hijo cerro la tuberia sin escribir nada. */
                fprintf(stderr, "[PID %d] Sin datos del hijo %d\n",
                        (int)getpid(), (int)pid);
            }
            fclose(f);   /* cierra fd_pipe[0] tambien */
        }

        printf("[PID %d] Hijo creado: PID %d\n", (int)getpid(), (int)pid);
        fflush(stdout);
    }

    close(fd_srv);
    return 0;
}