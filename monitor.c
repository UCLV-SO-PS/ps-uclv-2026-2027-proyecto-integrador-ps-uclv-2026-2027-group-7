#include "monitor.h"

#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Tabla interna. Es estática: nadie fuera de este archivo la ve.
 * --------------------------------------------------------------- */
static Episodio bd[] = {
    { "S01E01", "The Hedge Knight",         9.2f, 45120 },
    { "S01E02", "Hard Salt Beef",           8.8f, 38900 },
    { "S01E03", "The Squire",               9.5f, 52400 },
    { "S01E04", "Seven",                    9.1f, 41200 },
    { "S01E05", "In the Name of the Mother",9.7f, 68300 },
    { "S01E06", "The Morrow",               9.4f, 49800 },
};

#define TOTAL_EPISODIOS ((int)(sizeof(bd) / sizeof(bd[0])))

/* Bandera de inicialización. */
static int inicializada = 0;

/* ---------------------------------------------------------------
 * monitor_init
 * --------------------------------------------------------------- */
int monitor_init(void)
{
    if (inicializada)
        return 0;

    /*
     * Aquí no hay nada que cargar (la tabla es estática), pero
     * dejamos el punto de entrada para que en el futuro se pueda
     * leer de un archivo sin cambiar la interfaz pública.
     */
    inicializada = 1;
    return 0;
}

/* ---------------------------------------------------------------
 * monitor_buscar
 * Compara el código exacto (6 caracteres). Acepta cualquier
 * cadena; si no mide 6 caracteres, no puede coincidir.
 * --------------------------------------------------------------- */
const Episodio *monitor_buscar(const char *codigo)
{
    if (codigo == NULL)
        return NULL;

    if (strlen(codigo) != 6)
        return NULL;

    for (int i = 0; i < TOTAL_EPISODIOS; i++) {
        if (strcmp(bd[i].codigo, codigo) == 0)
            return &bd[i];
    }
    return NULL;
}

/* ---------------------------------------------------------------
 * monitor_formatear
 * Escribe una respuesta de varias líneas.
 * Devuelve bytes escritos (sin '\0') o -1.
 * --------------------------------------------------------------- */
int monitor_formatear(const Episodio *ep, char *buf, size_t tam)
{
    if (ep == NULL || buf == NULL || tam == 0)
        return -1;

    int n = snprintf(buf, tam,
        "Codigo      : %s\n"
        "Titulo      : %s\n"
        "Puntuacion  : %.1f\n"
        "Votos       : %d\n",
        ep->codigo, ep->titulo, ep->puntuacion, ep->votos);

    if (n < 0 || (size_t)n >= tam)
        return -1;

    return n;
}

/* ---------------------------------------------------------------
 * monitor_total
 * --------------------------------------------------------------- */
int monitor_total(void)
{
    return TOTAL_EPISODIOS;
}