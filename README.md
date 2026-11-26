<<<<<<< HEAD
# ps-uclv-2026-2027-laboratorio-1-git-github-ronaldo-castillo-basso

# Proyecto Final — Servidor Concurrente TCP de Calificaciones IMDb

**Asignatura:** Programación de Sistemas
**Curso:** 2026–2027
**Equipo:** <Ronaldo_Castillo_Basso>


## Integrantes

| Nombre completo 		| 	Usuario GitHub	     |
|-------------------------------|----------------------------|
| Ronaldo Castillo Basso        | rcastillobasso111@gmail.com|


### Nota: 5

## Descripción

Servidor concurrente TCP en C que consulta una base de datos de episodios
de "A Knight of the Seven Kingdoms" y responde a clientes que envían
códigos como `S01E05`. Escucha en el puerto **6666**.

El servidor atiende cada conexión en un proceso hijo creado con `fork()`.
El hijo envía la dirección IP del cliente al padre a través de una tubería
anónima (`pipe()`) antes de atender la solicitud. El padre cosecha a los
hijos terminados con `SIGCHLD` + `sigaction()` + `waitpid(-1, NULL, WNOHANG)`
para evitar procesos zombis.

## Estructura del repositorio

.
├── Makefile
├── README.md
├── monitor.c / monitor.h # Base de datos y utilidades (libmonitor.a)
├── server.c # Servidor concurrente TCP
├── client.c # Cliente de consola
├── libmonitor.a # Generada por make all
├── server / client # Generados por make all
└── screenshot/ # Capturas de funcionamiento
text


## Compilación

```bash
make all
=======
# ps-uclv-2026-2027-proyecto-integrador-ps-uclv-2026-2027-group-7
>>>>>>> a41e97c7bd3d7a3f198cf88502feac32ea24d653
