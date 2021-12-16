/*
 *  minikernel/include/kernel.h
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero de cabecera que contiene definiciones usadas por kernel.c
 *
 *      SE DEBE MODIFICAR PARA INCLUIR NUEVA FUNCIONALIDAD
 *
 */

#ifndef _KERNEL_H
#define _KERNEL_H

// Creado por nosotros
#define NO_RECURSIVO 0
#define RECURSIVO 1
// Estados de los mutex
#define BLOQUEADO_MUTEX 0
#define DESBLOQUEADO_MUTEX 1
//

#include "const.h"
#include "HAL.h"
#include "llamsis.h"

/*
 *
 * Definicion del tipo que corresponde con el BCP.
 * Se va a modificar al incluir la funcionalidad pedida.
 *
 */

typedef struct BCP_t *BCPptr;

typedef struct BCP_t {
        int id;				/* ident. del proceso */
        int estado;			/* TERMINADO|LISTO|EJECUCION|BLOQUEADO*/
        contexto_t contexto_regs;	/* copia de regs. de UCP */
        void * pila;
		//Creado por nosotros
		int dormir_t;
		int lista_mutex[NUM_MUT_PROC];
		int num_mutex_asignados;
		int tiempo_rodaja;
		//
	BCPptr siguiente;		/* puntero a otro BCP */
	void *info_mem;			/* descriptor del mapa de memoria */
} BCP;

/*
 *
 * Definicion del tipo que corresponde con la cabecera de una lista
 * de BCPs. Este tipo se puede usar para diversas listas (procesos listos,
 * procesos bloqueados en sem�foro, etc.).
 *
 */

typedef struct{
	BCP *primero;
	BCP *ultimo;
} lista_BCPs;


/*
 * Variable global que identifica el proceso actual
 */

BCP * p_proc_actual=NULL;

/*
 * Variable global que representa la tabla de procesos
 */

BCP tabla_procs[MAX_PROC];

/*
 * Variable global que representa la cola de procesos listos
 */
lista_BCPs lista_listos = {NULL, NULL};

// Creado por nosotros
lista_BCPs lista_procesos_esperando_plazos = {NULL, NULL};
//

/*
 *
 * Definici�n del tipo que corresponde con una entrada en la tabla de
 * llamadas al sistema.
 *
 */
typedef struct{
	int (*fservicio)();
} servicio;


//Creado por nosotros
typedef struct mutex_t *mutexPtr;

typedef struct mutex_t{
	char nombre[MAX_NOM_MUT];
	int id;
	int tipo;
	int estado; // 0 bloqueado 1 desbloqueado
	int num_procesos_usandolo;
	mutexPtr siguiente;
	int veces_bloqueado;
	int id_proceso_propietario;
	lista_BCPs lista_procesos_lock;
}mutex;

typedef struct{
	mutex *primero;
	mutex *ultimo;
} lista_Mutex;

// Lista global de mutex del sistema
lista_Mutex lista_mutex_global = {NULL, NULL};
//
/*
 * Prototipos de las rutinas que realiza = NULLn cada llamada al sistema
 */
int sis_crear_proceso();
int sis_terminar_proceso();
int sis_escribir();
int obtener_id_pr();
// Creado por nostoros
int sis_dormir();
int sis_crear_mutex();
int sis_abrir_mutex();
int sis_lock_mutex();
int sis_unlock_mutex();
int sis_cerrar_mutex();
//

/*
 * Variable global que contiene las rutinas que realizan cada llamada
 */
servicio tabla_servicios[NSERVICIOS]={	{sis_crear_proceso},
					{sis_terminar_proceso},
					{sis_escribir},
					{obtener_id_pr},
					// Creado por nosotros
					{sis_dormir},
					{sis_crear_mutex},
					{sis_abrir_mutex},
					{sis_lock_mutex},
					{sis_unlock_mutex},
					{sis_cerrar_mutex}};
					//

#endif /* _KERNEL_H */

