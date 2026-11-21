#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define SERVIDOR_IP "127.0.0.1"
#define PUERTO      6666
#define TAM_CODIGO  6
#define TAM_RESP    512

static int conectar_servidor(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(PUERTO);

    if (inet_pton(AF_INET, SERVIDOR_IP, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }

    return fd;
}

static int leer_codigo(char *codigo)
{
    char linea[64];
    printf("Ingrese codigo (6 caracteres, ej S01E05): ");
    fflush(stdout);

    if (fgets(linea, sizeof linea, stdin) == NULL)
        return -1;

    /* Quitar el '\n' final si lo hay. */
    size_t len = strcspn(linea, "\r\n");
    linea[len] = '\0';

    if (strlen(linea) != TAM_CODIGO) {
        fprintf(stderr, "Error: el codigo debe tener exactamente %d caracteres.\n",
                TAM_CODIGO);
        return -1;
    }

    memcpy(codigo, linea, TAM_CODIGO);
    return 0;
}

int main(void)
{
    char codigo[TAM_CODIGO + 1];
    if (leer_codigo(codigo) != 0)
        return 1;

    int fd = conectar_servidor();
    if (fd < 0)
        return 1;

    /* Enviar los 6 bytes exactos, incluso si write() escribe parcialmente. */
    size_t enviados = 0;
    while (enviados < TAM_CODIGO) {
        ssize_t n = write(fd, codigo + enviados, TAM_CODIGO - enviados);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("write");
            close(fd);
            return 1;
        }
        if (n == 0) {
            fprintf(stderr, "No se pudo completar el envio del codigo.\n");
            close(fd);
            return 1;
        }
        enviados += (size_t)n;
    }

    /* Leer hasta '\0'. */
    char resp[TAM_RESP];
    size_t total = 0;
    for (;;) {
        if (total >= sizeof resp - 1) {
            fprintf(stderr, "Respuesta demasiado larga\n");
            break;
        }
        ssize_t n = read(fd, resp + total, sizeof resp - 1 - total);
        if (n < 0) {
            perror("read");
            break;
        }
        if (n == 0)
            break;

        total += (size_t)n;
        /* ¿Ya llegó el '\0'? */
        if (memchr(resp, '\0', total) != NULL)
            break;
    }
    resp[total] = '\0';

    printf("%s", resp);

    /* ---Esperar a que el usuario presione 'q' --- */
    printf("\n[Presione 'q' y Enter para cerrar la conexion y terminar]: ");
    fflush(stdout);
    
    char salir[10];
    while (fgets(salir, sizeof salir, stdin) != NULL) {
        if (salir[0] == 'q' || salir[0] == 'Q') {
            break;
        }
        printf("[Presione 'q' y Enter para cerrar]: ");
        fflush(stdout);
    }

    close(fd);
    return 0;
}