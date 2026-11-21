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

static int servidor_test_run(int fd_srv)
{
    printf("[TEST] Servidor en modo --test-run\n");

    /* --- 1) Validar la base de datos --- */
    if (monitor_total() != 6) {
        fprintf(stderr, "[TEST] FALLO: la BD no tiene 6 episodios\n");
        return 1;
    }
    const Episodio *ep = monitor_buscar("S01E01");
    if (ep == NULL || strcmp(ep->titulo, "The Hedge Knight") != 0) {
        fprintf(stderr, "[TEST] FALLO: S01E01 no encontrado o incorrecto\n");
        return 1;
    }
    printf("[TEST] BD OK: %d episodios, S01E01 = '%s'\n",
           monitor_total(), ep->titulo);

    /* --- 2) Lanzar cliente interno en un hijo --- */
    fflush(stdout);

    pid_t pid = fork();
    if (pid < 0) {
        perror("[TEST] fork");
        return 1;
    }

    if (pid == 0) {
        /* ---------- HIJO: cliente interno ---------- */
        /* Pequeña pausa para que el padre llegue al accept. */
        usleep(100 * 1000);   /* 100 ms */

        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            perror("[TEST hijo] socket");
            _exit(1);
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof addr);
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(PUERTO);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
            perror("[TEST hijo] connect");
            close(fd);
            _exit(1);
        }

        /* Enviar codigo. */
        const char *codigo = "S01E01";
        if (write(fd, codigo, TAM_CODIGO) != TAM_CODIGO) {
            perror("[TEST hijo] write");
            close(fd);
            _exit(1);
        }

        /* Leer respuesta hasta '\0'. */
        char resp[TAM_RESP];
        size_t total = 0;
        for (;;) {
            if (total >= sizeof resp - 1)
                break;
            ssize_t n = read(fd, resp + total, sizeof resp - 1 - total);
            if (n <= 0)
                break;
            total += (size_t)n;
            if (memchr(resp, '\0', total) != NULL)
                break;
        }
        resp[total] = '\0';
        close(fd);

        /* Validar respuesta. */
        if (strstr(resp, "The Hedge Knight") == NULL) {
            fprintf(stderr, "[TEST hijo] FALLO: respuesta inesperada:\n%s\n", resp);
            _exit(1);
        }

        printf("[TEST hijo] OK: respuesta correcta:\n%s", resp);
        _exit(0);
    }

    /* ---------- PADRE: servidor, un solo accept ---------- */
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof cli_addr;

    int fd_cli = accept(fd_srv, (struct sockaddr *)&cli_addr, &cli_len);
    if (fd_cli < 0) {
        perror("[TEST] accept");
        /* Matar al hijo para no dejarlo colgado. */
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        return 1;
    }

    /* Atender al cliente interno igual que en produccion. */
    atender_cliente(fd_cli, &cli_addr);
    close(fd_cli);

    /* Esperar al hijo y comprobar su codigo de salida. */
    int estado = 0;
    if (waitpid(pid, &estado, 0) < 0) {
        perror("[TEST] waitpid");
        return 1;
    }

    if (!WIFEXITED(estado) || WEXITSTATUS(estado) != 0) {
        fprintf(stderr, "[TEST] FALLO: el cliente interno fallo\n");
        return 1;
    }

    printf("[TEST] OK: servidor responde correctamente\n");
    return 0;
}

static void servidor_normal(int fd_srv)
{
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
            /* HIJO */
            struct sigaction sa;
            memset(&sa, 0, sizeof sa);
            sa.sa_handler = SIG_DFL;
            sigemptyset(&sa.sa_mask);
            sigaction(SIGCHLD, &sa, NULL);

            close(fd_srv);
            close(fd_pipe[0]);

            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &cli_addr.sin_addr, ip, sizeof ip);
            char msg[64];
            int n = snprintf(msg, sizeof msg, "%d %s\n", (int)getpid(), ip);
            if (n > 0 && (size_t)n < sizeof msg) {
                ssize_t w = write(fd_pipe[1], msg, (size_t)n);
                (void)w;
            }
            close(fd_pipe[1]);

            atender_cliente(fd_cli, &cli_addr);
            close(fd_cli);
            _exit(0);
        }

        /* PADRE */
        close(fd_cli);
        close(fd_pipe[1]);

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
                }
            }
            fclose(f);
        }

        printf("[PID %d] Hijo creado: PID %d\n", (int)getpid(), (int)pid);
        fflush(stdout);
    }
}

int main(int argc, char *argv[])
{
    int modo_test = 0;

    if (argc > 1) {
        if (strcmp(argv[1], "--test-run") == 0) {
            modo_test = 1;
        } else {
            fprintf(stderr, "Uso: %s [--test-run]\n", argv[0]);
            return 1;
        }
    }

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

    printf("[PID %d] Servidor escuchando en puerto %d%s\n",
           (int)getpid(), PUERTO,
           modo_test ? " (modo test)" : "");
    fflush(stdout);

    int rc;
    if (modo_test) {
        rc = servidor_test_run(fd_srv);
    } else {
        servidor_normal(fd_srv);
        rc = 0;   /* inalcanzable */
    }

    close(fd_srv);
    return rc;
}