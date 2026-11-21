#ifndef MONITOR_H
#define MONITOR_H

#include <stddef.h>   /* size_t */

/*
 * Estructura pública de un episodio.
 * El código tiene exactamente 6 caracteres + '\0'  -> "S01E01".
 */
typedef struct {
    char   codigo[7];
    char   titulo[64];
    float  puntuacion;
    int    votos;
} Episodio;

/*
 * Inicializa la base de datos interna.
 * Es idempotente: puede llamarse varias veces sin efectos adversos.
 * Devuelve 0 en éxito, -1 en error.
 */
int  monitor_init(void);

/*
 * Busca un episodio por código exacto de 6 caracteres.
 * Devuelve un puntero a la estructura interna (solo lectura),
 * o NULL si el código no existe o es inválido.
 */
const Episodio *monitor_buscar(const char *codigo);

/*
 * Formatea la respuesta que se enviará al cliente.
 * `buf` debe tener al menos `tam` bytes.
 * Devuelve el número de bytes escritos (sin contar '\0'),
 * o -1 si hubo error (episodio NULL o buffer insuficiente).
 */
int  monitor_formatear(const Episodio *ep, char *buf, size_t tam);

/*
 * Devuelve cuántos episodios hay en la base de datos.
 * Útil para tests y para el --test-run.
 */
int  monitor_total(void);

#endif /* MONITOR_H */