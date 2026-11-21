#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

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

    /* Permite reutilizar el puerto tras un cierre rápido. */
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

    printf("Cliente conectado desde %s:%d\n",
           ip, ntohs(cli_addr->sin_port));

    /* 1) Leer exactamente 6 bytes. */
    char codigo[TAM_CODIGO + 1];
    memset(codigo, 0, sizeof codigo);

    ssize_t total = 0;
    while (total < TAM_CODIGO) {
        ssize_t n = read(fd_cli, codigo + total, TAM_CODIGO - total);
        if (n < 0) {
            perror("read");
            return;
        }
        if (n == 0) {
            printf("Cliente cerro antes de enviar el codigo.\n");
            return;
        }
        total += n;
    }

    printf("Solicitud recibida: %s\n", codigo);

    /* 2) Buscar y formatear. */
    char resp[TAM_RESP];
    const Episodio *ep = monitor_buscar(codigo);
    int n;
    if (ep == NULL) {
        n = snprintf(resp, sizeof resp, "ERROR: codigo '%s' no encontrado\n", codigo);
    } else {
        n = monitor_formatear(ep, resp, sizeof resp);
    }

    if (n < 0) {
        fprintf(stderr, "Error formateando respuesta\n");
        return;
    }

    /* 3) Enviar respuesta. Se envía n+1 para incluir el '\0'. */
    ssize_t enviado = write(fd_cli, resp, (size_t)n + 1);
    if (enviado < 0) {
        perror("write");
        return;
    }
    printf("Respuesta enviada (%zd bytes).\n", enviado);
}

int main(void)
{
    if (monitor_init() != 0) {
        fprintf(stderr, "Error inicializando la base de datos\n");
        return 1;
    }

    int fd_srv = crear_socket_servidor();
    if (fd_srv < 0)
        return 1;

    printf("Servidor escuchando en puerto %d...\n", PUERTO);
    fflush(stdout);

    for (;;) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof cli_addr;

        int fd_cli = accept(fd_srv, (struct sockaddr *)&cli_addr, &cli_len);
        if (fd_cli < 0) {
            perror("accept");
            continue;
        }

        atender_cliente(fd_cli, &cli_addr);
        close(fd_cli);
    }

    close(fd_srv);
    return 0;
}