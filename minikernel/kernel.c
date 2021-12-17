/*
 *  kernel/kernel.c
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero que contiene la funcionalidad del sistema operativo
 *
 */

#include "kernel.h"	/* Contiene defs. usadas por este modulo */
#include "stdlib.h"
#include "string.h"

// Creado por nosotros
int nivelAnterior; // Variable global que almacena el nivel previo a una interrupcion
int num_mutex = 0; // Variable global que almacena el numero actual de mutex en el sistema;
int id_mutex = 0;
//

/*
 *
 * Funciones relacionadas con la tabla de procesos:
 *	iniciar_tabla_proc buscar_BCP_libre
 *
 */

/*
 * Funci�n que inicia la tabla de procesos
 */
static void iniciar_tabla_proc(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		tabla_procs[i].estado=NO_USADA;
}

/*
 * Funci�n que busca una entrada libre en la tabla de procesos
 */
static int buscar_BCP_libre(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		if (tabla_procs[i].estado==NO_USADA)
			return i;
	return -1;
}

/*
 *
 * Funciones que facilitan el manejo de las listas de BCPs
 *	insertar_ultimo eliminar_primero eliminar_elem
 *
 * NOTA: PRIMERO SE DEBE LLAMAR A eliminar Y LUEGO A insertar
 */

/*
 * Inserta un BCP al final de la lista.
 */
static void insertar_ultimo(lista_BCPs *lista, BCP * proc){
	if (lista->primero==NULL)
		lista->primero= proc;
	else
		lista->ultimo->siguiente=proc;
	lista->ultimo= proc;
	proc->siguiente=NULL;
}

/*
 * Elimina el primer BCP de la lista.
 */
static void eliminar_primero(lista_BCPs *lista){

	if (lista->ultimo==lista->primero)
		lista->ultimo=NULL;
	lista->primero=lista->primero->siguiente;
}

/*
 * Elimina un determinado BCP de la lista.
 */
static void eliminar_elem(lista_BCPs *lista, BCP * proc){
	BCP *paux=lista->primero;

	if (paux==proc)
		eliminar_primero(lista);
	else {
		for ( ; ((paux) && (paux->siguiente!=proc));
			paux=paux->siguiente);
		if (paux) {
			if (lista->ultimo==paux->siguiente)
				lista->ultimo=paux;
			paux->siguiente=paux->siguiente->siguiente;
		}
	}
}

/*
 *
 * Funciones relacionadas con la planificacion
 *	espera_int planificador
 */

/*
 * Espera a que se produzca una interrupcion
 */
static void espera_int(){
	int nivel;

	printk("-> NO HAY LISTOS. ESPERA INT\n");

	/* Baja al m�nimo el nivel de interrupci�n mientras espera */
	nivel=fijar_nivel_int(NIVEL_1);
	halt();
	fijar_nivel_int(nivel);
}

/*
 * Funci�n de planificacion que implementa un algoritmo FIFO.
 */
static BCP * planificador(){
	while (lista_listos.primero==NULL)
		espera_int();		/* No hay nada que hacer */
	return lista_listos.primero;
}

/*
 *
 * Funcion auxiliar que termina proceso actual liberando sus recursos.
 * Usada por llamada terminar_proceso y por rutinas que tratan excepciones
 *
 */
static void liberar_proceso(){
	BCP * p_proc_anterior;
	
	liberar_imagen(p_proc_actual->info_mem); /* liberar mapa */

	p_proc_actual->estado=TERMINADO;
	eliminar_primero(&lista_listos); /* proc. fuera de listos */

	/* Realizar cambio de contexto */
	p_proc_anterior=p_proc_actual;
	p_proc_actual=planificador();

	//Inicializamos la rodaja del nuevo proceso en su totalidad
	p_proc_actual->tiempo_rodaja=TICKS_POR_RODAJA;


	printk("-> C.CONTEXTO POR FIN: de %d a %d\n",
			p_proc_anterior->id, p_proc_actual->id);

	liberar_pila(p_proc_anterior->pila);
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
        return; /* no deber�a llegar aqui */
}

/*
 *
 * Funciones relacionadas con el tratamiento de interrupciones
 *	excepciones: exc_arit exc_mem
 *	interrupciones de reloj: int_reloj
 *	interrupciones del terminal: int_terminal
 *	llamadas al sistemas: llam_sis
 *	interrupciones SW: int_sw
 *
 */

/*
 * Tratamiento de excepciones aritmeticas
 */
static void exc_arit(){

	if (!viene_de_modo_usuario())
		panico("excepcion aritmetica cuando estaba dentro del kernel");


	printk("-> EXCEPCION ARITMETICA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de excepciones en el acceso a memoria
 */
static void exc_mem(){

	if (!viene_de_modo_usuario())
		panico("excepcion de memoria cuando estaba dentro del kernel");


	printk("-> EXCEPCION DE MEMORIA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de interrupciones de terminal
 */
static void int_terminal(){
	char car;

	car = leer_puerto(DIR_TERMINAL);
	printk("-> TRATANDO INT. DE TERMINAL %c\n", car);

        return;
}

/*
 * Tratamiento de interrupciones de reloj
 */
static void int_reloj(){

	printk("-> TRATANDO INT. DE RELOJ\n");
	//Planificacion round robin
	printk("Proceso actual tiempo rodaja: %d\n", p_proc_actual->tiempo_rodaja);
	if(p_proc_actual->tiempo_rodaja<=0){
		//Fija el nivel de interrupcion solo
		activar_int_SW();
	}
	else{
		//En caso de que no queden mas procesos, el proceso padre de todos no consume la rodaja hasta que no hayan mas procesos en la cola.ls
		if(lista_listos.primero!=NULL)
			p_proc_actual->tiempo_rodaja--;
	}
	//

	BCP * index_lista_bloqueados = lista_procesos_esperando_plazos.primero;
	while(index_lista_bloqueados != NULL){
		// Guardamos el proceso siguiente en caso de que se borre el proceso porque haya terminado
		BCP * proceso_siguiente = index_lista_bloqueados->siguiente;
		if(index_lista_bloqueados->dormir_t % 100 == 0)
			printk("Proceso ID(%d): Quedan %d segundos\n", index_lista_bloqueados-> id, index_lista_bloqueados->dormir_t/TICK);
		
		if(index_lista_bloqueados->dormir_t <= 0){

			printk("El proceso con id = %d despierta\n", index_lista_bloqueados->id);
			index_lista_bloqueados->estado = LISTO;
			eliminar_elem(&lista_procesos_esperando_plazos, index_lista_bloqueados);
			insertar_ultimo(&lista_listos, index_lista_bloqueados);
		}
		else
			index_lista_bloqueados->dormir_t--;

		//index_lista_bloqueados = index_lista_bloqueados->siguiente;
		index_lista_bloqueados = proceso_siguiente;
	}
	// Restauramos al nivel previo de la interrupcion 
	fijar_nivel_int(nivelAnterior);
	return;
}

/*
 * Tratamiento de llamadas al sistema
 */
static void tratar_llamsis(){
	int nserv, res;

	nserv=leer_registro(0);
	if (nserv<NSERVICIOS)
		res=(tabla_servicios[nserv].fservicio)();
	else
		res=-1;		/* servicio no existente */
	escribir_registro(0,res);
	return;
}

/*
 * Tratamiento de interrupciuones software
 */


static void int_sw(){
	printk("-> TRATANDO INT. SW\n");
	//creado por nosotros
	BCP* procesoActual;
	p_proc_actual->estado=LISTO;
	procesoActual=p_proc_actual;
	eliminar_primero(&lista_listos);
	insertar_ultimo(&lista_listos, procesoActual);
	p_proc_actual=planificador();

	p_proc_actual->tiempo_rodaja=TICKS_POR_RODAJA;
	
	cambio_contexto(&procesoActual->contexto_regs, &p_proc_actual->contexto_regs);
	//	

	return;
}

/*
 *
 * Funcion auxiliar que crea un proceso reservando sus recursos.
 * Usada por llamada crear_proceso.
 *
 */
static int crear_tarea(char *prog){
	void * imagen, *pc_inicial;
	int error=0;
	int proc;
	BCP *p_proc;

	proc=buscar_BCP_libre();
	if (proc==-1)
		return -1;	/* no hay entrada libre */

	/* A rellenar el BCP ... */
	p_proc=&(tabla_procs[proc]);

	/* crea la imagen de memoria leyendo ejecutable */
	imagen=crear_imagen(prog, &pc_inicial);
	if (imagen)
	{
		p_proc->info_mem=imagen;
		p_proc->pila=crear_pila(TAM_PILA);
		fijar_contexto_ini(p_proc->info_mem, p_proc->pila, TAM_PILA,
			pc_inicial,
			&(p_proc->contexto_regs));
		p_proc->id=proc;
		p_proc->estado=LISTO;
		//Creado por nosotros
		p_proc->num_mutex_asignados = 0;
		p_proc->tiempo_rodaja = TICKS_POR_RODAJA;
		//

		/* lo inserta al final de cola de listos */
		insertar_ultimo(&lista_listos, p_proc);
		error= 0;
	}
	else
		error= -1; /* fallo al crear imagen */

	return error;
}

/*
 *
 * Rutinas que llevan a cabo las llamadas al sistema
 *	sis_crear_proceso sis_escribir
 *
 */

/*
 * Tratamiento de llamada al sistema crear_proceso. Llama a la
 * funcion auxiliar crear_tarea sis_terminar_proceso
 */
int sis_crear_proceso(){
	char *prog;
	int res;

	printk("-> PROC %d: CREAR PROCESO\n", p_proc_actual->id);
	prog=(char *)leer_registro(1);
	res=crear_tarea(prog);
	return res;
}

/*
 * Tratamiento de llamada al sistema escribir. Llama simplemente a la
 * funcion de apoyo escribir_ker
 */
int sis_escribir()
{
	char *texto;
	unsigned int longi;

	texto=(char *)leer_registro(1);
	longi=(unsigned int)leer_registro(2);

	escribir_ker(texto, longi);
	return 0;
}

/*
 * Tratamiento de llamada al sistema terminar_proceso. Llama a la
 * funcion auxiliar liberar_proceso
 */
int sis_terminar_proceso(){
//Creado por nosotros
	// Entra si hay algun mutex abierto. Elimina siempre el que esta en la posicion 0 porque por dentro se 
	// actualiza la lista sola.
	if(p_proc_actual->num_mutex_asignados > 0){
		for (int i = 0; i < p_proc_actual->num_mutex_asignados;){
			escribir_registro(1 ,p_proc_actual->lista_mutex[i]);
			mutex* auxMutex=lista_mutex_global.primero;
			while(auxMutex!=NULL){
				if(auxMutex->id==p_proc_actual->lista_mutex[i]){
					break;
				}
				auxMutex=auxMutex->siguiente;
			}
			//Desbloqueamos todos los mutex que han sido bloqueados por el proceso que se va a cerrar
			while(auxMutex->veces_bloqueado>0 && auxMutex!=NULL && auxMutex->id_proceso_propietario == p_proc_actual->id)
				sis_unlock_mutex();
			sis_cerrar_mutex();
		}
		
	}
	printk("-> FIN PROCESO %d\n", p_proc_actual->id);
	liberar_proceso();

    return 0; /* no deber�a llegar aqui */
}

int obtener_id_pr(){
	int id = p_proc_actual->id;
	printk("ID del proceso actual es: %d\n", id);
	return id;
}

// Creado por nosotros


int sis_dormir(){
	// Leemos del registro el valor de segundos a dormir
	unsigned int segs = leer_registro(1);
	// Eliminamos el proceso de la lista de listos
	eliminar_primero(&lista_listos);
	int id = p_proc_actual->id;
	printk("Mandando a dormir el proceso ID(%d) %d segundos\n", id ,segs);
	// Indicamos los TICKS que se ha de dormir el proceso
	p_proc_actual->dormir_t = segs*TICK;
	//Cambiamos el estado a BLOQUEADO 
	p_proc_actual->estado = BLOQUEADO;
	BCP * p_bloqueado = p_proc_actual;
	insertar_ultimo(&lista_procesos_esperando_plazos, p_bloqueado);
	// Asignamos el siguiente proceso de la lista de listos como como el actual
	p_proc_actual = planificador();
	// Cambiamos el contexto para salvar los registros del proceso bloqueado y asignar los del nuevo
	cambio_contexto(&p_bloqueado->contexto_regs, &p_proc_actual->contexto_regs);


	// Cambiamos el nivel para ejecutar la interrupcion de reloj y guardamos el anterior
	nivelAnterior =  fijar_nivel_int(NIVEL_3);
	printk("\nTermina interrupcion de reloj\n");

	return 0;
}

/*
 * Inserta un mutex al final de la lista.
 */
static void insertar_ultimo_mutex(lista_Mutex* lista, mutex* mutex){
	if (lista->primero==NULL)
		lista->primero= mutex;
	else
		lista->ultimo->siguiente=mutex;
	lista->ultimo= mutex;
	mutex->siguiente=NULL;
}

/*
 * Elimina el primer mutex de la lista.
 */
static void eliminar_primero_mutex(lista_Mutex* lista){

	if (lista->ultimo == lista->primero)
		lista->ultimo=NULL;
	lista->primero = lista->primero->siguiente;
}

/*
 * Elimina un determinado mutex de la lista.
 *
 * ¡ATENCION! Esta funcion esta reutilizada de eliminar_elem().
 * En esta funcion, a la hora de eliminar los mutex, no se libera la memoria correspondiente a ellos
 * pudiendo desembocar en una ocupacion total de la memoria del sistema. 
 */
static void eliminar_elem_mutex(lista_Mutex* lista, mutex* mutex_a_eliminar){
	mutex* maux = lista->primero;

	if (maux == mutex_a_eliminar)
		eliminar_primero_mutex(lista);
	else {
		for ( ; ((maux) && (maux->siguiente!=mutex_a_eliminar)); maux = maux->siguiente);
		if (maux) {
			if (lista->ultimo == maux->siguiente)
				lista->ultimo = maux;
			maux->siguiente = maux->siguiente->siguiente;
		}
	}
}

int sis_crear_mutex(){
	// Comprobar si existen ya mutex con el nombre pasado y si num_mutex == MAX_MUTEX
	// Si ya no se pueden crear mas mutex, devuelve -1.
	// Si existe un mutex con el nombre, devuelve -2.

	mutex *newMutex = (mutex*)malloc(sizeof(mutex));
	mutex *auxMutex = lista_mutex_global.primero;
	
	char* nombre=(char*)leer_registro(1);
	while(auxMutex != NULL){
		if(strcmp(auxMutex->nombre, nombre)==0){
			free(newMutex);
			return -2;
		}
		auxMutex = auxMutex->siguiente;
	}
	//Inicializamos el mutex
	strcpy(newMutex->nombre, nombre);
	newMutex->tipo = (int)leer_registro(2);
	newMutex->estado = DESBLOQUEADO_MUTEX;
	newMutex->siguiente = NULL;
	newMutex->num_procesos_usandolo = 0;
	newMutex->veces_bloqueado=0;
	newMutex->id_proceso_propietario=-1;
	newMutex->lista_procesos_lock.primero=NULL;
	newMutex->lista_procesos_lock.ultimo=NULL;
	// Para poder bloquear un proceso tenemos que pasar a sis_domir el
	// valor de cuanto tiempo queremos que duerma. En nuestro caso asignamos 1 seg
	while(num_mutex == NUM_MUT){
		// Bloquear proceso 1 seg hasta que haya un hueco en la lista de mutex
		escribir_registro(1, 1);
		sis_dormir();
	}
	// Asignamos e incrementamos el numero de mutex del sistema
	newMutex->id = id_mutex++;
	num_mutex++;
	//Añadimos a la lista global el nuevo mutex
	insertar_ultimo_mutex(&lista_mutex_global, newMutex);
	//Incrementamos las variables de los contadores de mutex tanto de la lista global como de la lista del proceso, además de insertarlo en dicha lista
	p_proc_actual->lista_mutex[p_proc_actual->num_mutex_asignados] = newMutex->id;
	p_proc_actual->num_mutex_asignados++;
	newMutex->num_procesos_usandolo++;
	return newMutex->id;
}

int sis_abrir_mutex(){
	// Añadimos a la lista local de mutex del proceso. 
	if(p_proc_actual->num_mutex_asignados == NUM_MUT_PROC){
		return -1;
	}
	else{
		char *nombre = (char*)leer_registro(1);
		mutex *mutexAbrir = lista_mutex_global.primero;
		while(mutexAbrir != NULL){
			//Buscamos el mutex en la lista global
			if(strcmp(mutexAbrir->nombre, nombre) == 0){
				for (int i = 0; i < p_proc_actual->num_mutex_asignados; i++){
					//Se comprueba si el proceso habia abierto ya con anterioridad el mutex
					if(mutexAbrir->id == p_proc_actual->lista_mutex[i])
						return mutexAbrir->id; // Si ya esta asignado al proceso
				}
				//En caso de que no se haya abierto con anterioridad por el proceso, se añade a la lista local del proceso
				p_proc_actual->lista_mutex[p_proc_actual->num_mutex_asignados] = mutexAbrir->id;
				mutexAbrir->num_procesos_usandolo++;
				p_proc_actual->num_mutex_asignados++;
				return mutexAbrir->id;
			}
			mutexAbrir = mutexAbrir->siguiente;	
		}
		//Devuelve -3 si el mutex no existe
		return -3;
	}
}

int sis_lock_mutex(){
	int id_mutex= leer_registro(1);
	//booleano donde 0 no existe y 1 existe
	int existe=0;
	//Buscamos si existe o no existe el mutex
	for (int i = 0; i < p_proc_actual->num_mutex_asignados; i++)
	{
		if(id_mutex==p_proc_actual->lista_mutex[i]){
			existe=1;
			break;
		}
	}
	if(existe==0){
		//Error -3: El mutex no existe
		return -3;
	}
	else{
		mutex* auxMutex=lista_mutex_global.primero;
		mutex* mutexLock;
		//Buscamos el mutex en la lista global
		while(auxMutex!=NULL){
			if(id_mutex==auxMutex->id){
				mutexLock=auxMutex;
				break;
			}
			auxMutex=auxMutex->siguiente;
		}
		if(mutexLock->tipo==NO_RECURSIVO){
			if(mutexLock->id_proceso_propietario==p_proc_actual->id){
				//Error: Ya es propietario del mutex no recursivo a bloquear 
				return -5;	
			}
			//Bloqueamos todo proceso intente acceder a este mutex 
			while(mutexLock->estado==BLOQUEADO_MUTEX){
				bloquearMutex(mutexLock);
				
			}
			mutexLock->estado=BLOQUEADO_MUTEX;
			mutexLock->id_proceso_propietario=p_proc_actual->id;
			mutexLock->veces_bloqueado=1;
			return 0;
		}
		else{
			//Bloqueamos todo proceso que este asociado al mutex y no sea el actual
			while(mutexLock->id_proceso_propietario!=p_proc_actual->id && mutexLock->id_proceso_propietario!=-1){
				bloquearMutex(mutexLock);
			}
			if(mutexLock->estado==DESBLOQUEADO_MUTEX)
				mutexLock->estado=BLOQUEADO_MUTEX;
			mutexLock->veces_bloqueado++;
			mutexLock->id_proceso_propietario=p_proc_actual->id;
			return 0;
		}
	}
	
	
}

int sis_unlock_mutex(){
	int id_mutex=leer_registro(1);
	//0 no existe, 1 existe
	int existe=0;
	for (int i = 0; i < p_proc_actual->num_mutex_asignados; i++)
	{
		if(id_mutex==p_proc_actual->lista_mutex[i]){
			existe=1;
			break;
		}
	}
	if(existe==0){
		//Error -3: El mutex no existe
		return -3;
	}
	else{
		mutex* auxMutex=lista_mutex_global.primero;
		mutex* mutexLock;
		while(auxMutex!=NULL){
			if(id_mutex==auxMutex->id){
				mutexLock=auxMutex;
				break;
			}
			auxMutex=auxMutex->siguiente;
		}
		if(mutexLock->tipo==NO_RECURSIVO){
			//Comprobamos que el proceso actual es el propietario y que el mutex tiene algun propietario
			if(mutexLock->id_proceso_propietario==p_proc_actual->id && mutexLock->id_proceso_propietario!=-1){
				mutexLock->estado=DESBLOQUEADO_MUTEX;
				//Quitamos al propietario que lo tenia bloqueado
				mutexLock->id_proceso_propietario=-1;
				mutexLock->veces_bloqueado=0;
				if(mutexLock->lista_procesos_lock.primero!=NULL){
					//Desbloqueamos todos los procesos que se habian quedado esperando en el mutex 
					while(mutexLock->lista_procesos_lock.primero!=NULL){
						//Lanzamos una interrupcion de reloj para poder desbloquear a los procesos
						nivelAnterior=fijar_nivel_int(NIVEL_3);
						mutexLock->lista_procesos_lock.primero->estado=LISTO;
						BCP* proceso=mutexLock->lista_procesos_lock.primero;
						eliminar_primero(&mutexLock->lista_procesos_lock);
						insertar_ultimo(&lista_listos, proceso);
						//Volvemos a este nivel
						fijar_nivel_int(nivelAnterior);
					}
				}
				return 0;
			}
			//Error no es el propietario
			return -6;
		}
		else{
			if(mutexLock->id_proceso_propietario!=p_proc_actual->id && mutexLock->id_proceso_propietario!=-1){
				//Error -6: El proceso no es el propietario del mutex
				return -6;
			}
			mutexLock->veces_bloqueado--;
			if(mutexLock->veces_bloqueado==0){
				mutexLock->estado=DESBLOQUEADO_MUTEX;
				//Quitamos al propietario que lo tenia bloqueado
				mutexLock->id_proceso_propietario=-1;
			}
			//Comprobamos que el mutex no está bloqueado y que tiene procesos que lo esta usando
			if(mutexLock->lista_procesos_lock.primero!=NULL&&mutexLock->veces_bloqueado==0){
				//despertamos a todos los procesos que estaban esperando al lock del mutex
				while(mutexLock->lista_procesos_lock.primero!=NULL){
					nivelAnterior=fijar_nivel_int(NIVEL_3);
					mutexLock->lista_procesos_lock.primero->estado=LISTO;
					BCP* proceso=mutexLock->lista_procesos_lock.primero;
					eliminar_primero(&mutexLock->lista_procesos_lock);
				
					insertar_ultimo(&lista_listos, proceso);
					fijar_nivel_int(nivelAnterior);
				}
			}
			return 0;
		}
	}
}

int sis_cerrar_mutex(){
	int idMutexCerrar = leer_registro(1);

	// Eliminado esta a false
	int eliminado = 0;

	//Eliminamos el mutex indicado
	for (int i = 0; i < p_proc_actual->num_mutex_asignados; i++){
		if(p_proc_actual->lista_mutex[i] == idMutexCerrar){
			p_proc_actual->lista_mutex[i] = -1;
			eliminado = 1;
		}
		//En caso de que se haya eliminado tenemos que actualizar el array
		if(eliminado == 1){
			//Desde el elemento eliminado hasta el penultimo, trasladamos los id's una posicion a la izquierda
			if(i < p_proc_actual->num_mutex_asignados - 1)
				p_proc_actual->lista_mutex[i] = p_proc_actual->lista_mutex[i + 1];
			else
				p_proc_actual->lista_mutex[i] = -1;
		}
	}

	if(eliminado == 1){
		//Decrementamos el numero de mutex asignados al proceso
		p_proc_actual->num_mutex_asignados--;
		mutex *auxMutex = lista_mutex_global.primero;
		while(auxMutex != NULL){
			if(auxMutex->id == idMutexCerrar){
				//Decrementamos el numero de procesos que tienen abierto el mutex
				auxMutex->num_procesos_usandolo--;
				if(auxMutex->id_proceso_propietario==p_proc_actual->id){
					//Quitamos propietario
					auxMutex->id_proceso_propietario=-1;
				}
				//En caso de que nadie lo este usando, lo eliminamos de la lista global
				if(auxMutex->num_procesos_usandolo == 0){
					eliminar_elem_mutex(&lista_mutex_global, auxMutex);
					num_mutex--;
				}
				//Buscamos en la lista los procesos bloqueados por el mutex
				BCP *auxProceso=auxMutex->lista_procesos_lock.primero;
				//Avanzamos en la lista solo cogiendo el primer proceso
				while(auxProceso!=NULL){
					
					auxProceso->estado=LISTO;
					eliminar_primero(&auxMutex->lista_procesos_lock);
					insertar_ultimo(&lista_listos, auxProceso);
					auxProceso=auxMutex->lista_procesos_lock.primero;
				}
				return 0;
			}
			auxMutex = auxMutex->siguiente;
		}
		return 0;
	}else{
		// No se ha podido eliminar el mutex
		return -3;
	}
}
//Funcion auxiliar que usamos en el lock y en el unlock para bloquear un proceso en el mutex que se pasa por parametro
void bloquearMutex(mutex* mutexLock){
	nivelAnterior =  fijar_nivel_int(NIVEL_3);
	eliminar_primero(&lista_listos);
	int id = p_proc_actual->id;
	//Cambiamos el estado a BLOQUEADO 
	p_proc_actual->estado = BLOQUEADO;
	BCP * p_bloqueado = p_proc_actual;

	insertar_ultimo(&mutexLock->lista_procesos_lock, p_bloqueado);
	// Asignamos el sieguiente proceso de la lista de listos como como el actual
	p_proc_actual = planificador();
	// Cambiamos el contexto para salvar los registros del proceso bloqueado y asignar los del nuevo
	cambio_contexto(&p_bloqueado->contexto_regs, &p_proc_actual->contexto_regs);


	// Cambiamos el nivel para ejecutar la interrupcion de reloj y guardamos el anterior
	fijar_nivel_int(nivelAnterior);
}

//
/*
 *
 * Rutina de inicializaci�n invocada en arranque
 *
 */
int main(){
	/* se llega con las interrupciones prohibidas */

	instal_man_int(EXC_ARITM, exc_arit); 
	instal_man_int(EXC_MEM, exc_mem); 
	instal_man_int(INT_RELOJ, int_reloj); 
	instal_man_int(INT_TERMINAL, int_terminal); 
	instal_man_int(LLAM_SIS, tratar_llamsis); 
	instal_man_int(INT_SW, int_sw); 

	iniciar_cont_int();		/* inicia cont. interr. */
	iniciar_cont_reloj(TICK);	/* fija frecuencia del reloj */
	iniciar_cont_teclado();		/* inici cont. teclado */

	iniciar_tabla_proc();		/* inicia BCPs de tabla de procesos */

	/* crea proceso inicial */
	if (crear_tarea((void *)"init")<0)
		panico("no encontrado el proceso inicial");
	
	/* activa proceso inicial */
	p_proc_actual=planificador();
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
	panico("S.O. reactivado inesperadamente");
	return 0;
}