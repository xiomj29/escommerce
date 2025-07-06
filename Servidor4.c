/*Funciones implementadas:
-Mostrar inventario (manejo de archivos, cliente y vendedor)
-Agregar productos (con manejo de archivos)
-Seleccionar producto (manejo de archivos)
-Ver carrito (manejo de archivos)
-Generar ticket (manejo de archivos)


Funciones de inicio de sesión e interfaz faltantes (TODAS) con manejo de archivos

*/

/*Compilación
    gcc Servidor2.c -o servidor -lpthread*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <pthread.h>
#include <openssl/sha.h>
#include <form.h>
#include <curses.h>
#include <errno.h>
#include <signal.h>

#define MAX_ITEM_NAME_LENGTH 50
#define MAX_DESCRIPTION_LENGTH 100
#define MAX_INVENTORY_ITEMS 100
#define MAX_USERNAME_LENGTH 50
#define MAX_PASSWORD_LENGTH 50
#define WIDTH 30
#define HEIGHT 10
#define MAX_USUARIOS 100
#define NOMBRE_ARCHIVO_USUARIOS "usuarios.txt"
#define INVENTARIO_FILE "inventario.txt"
#define CARRITO_FILE "carrito.txt"
#define LOCK_FILE "/tmp/server.lock"

typedef struct
{
    char user[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
    char tipo[1]; // 1 para cliente, 2 para vendedor
} Usuario;

typedef struct
{
    int item_id;
    char item_name[MAX_ITEM_NAME_LENGTH];
    char description[MAX_DESCRIPTION_LENGTH];
    float price;
    int units;
} InventoryItem;

typedef struct
{
    int item_count;
    InventoryItem items[MAX_INVENTORY_ITEMS];
} Inventario;

typedef struct
{
    int item_count;
    InventoryItem items[MAX_INVENTORY_ITEMS];
} Carrito;

int crea_semaforo(key_t llave, int valor_inicial)
{
    int semid = semget(llave, 1, IPC_CREAT | 0777);
    if (semid == -1)
    {
        perror("semget");
        exit(1);
    }
    semctl(semid, 0, SETVAL, valor_inicial);
    return semid;
}

void down(int semid)
{
    struct sembuf op_p[] = {0, -1, 0};
    semop(semid, op_p, 1);
}

void up(int semid)
{
    struct sembuf op_v[] = {0, +1, 0};
    semop(semid, op_v, 1);
}

void replaceSpacesWithUnderscores(char *str)
{
    for (int i = 0; str[i]; i++)
    {
        if (str[i] == ' ')
            str[i] = '_';
    }
}

void guardarInventarioEnArchivo(Inventario *inventario)
{
    FILE *inventario_file = fopen(INVENTARIO_FILE, "w");
    if (inventario_file == NULL)
    {
        perror("Error al abrir archivo de inventario");
        exit(1);
    }
    for (int i = 0; i < inventario->item_count; i++)
    {
        InventoryItem item = inventario->items[i];
        fprintf(inventario_file, "%d %s %s %.2f %d\n", item.item_id, item.item_name, item.description, item.price, item.units);
    }
    fclose(inventario_file);
}

void *servicio(void *arg)
{
    int id_usuario, id_inventario, id_carrito;
    Usuario *usuario;
    Inventario *inventario;
    Carrito *carrito;
    key_t llave_usuario, llave_inventario, llave_carrito;

    llave_usuario = ftok("Servidor3.c", 'u');
    id_usuario = shmget(llave_usuario, sizeof(Usuario), IPC_CREAT | 0777);
    usuario = (Usuario *)shmat(id_usuario, 0, 0);

    llave_inventario = ftok("Servidor3.c", 'i');
    id_inventario = shmget(llave_inventario, sizeof(Inventario), IPC_CREAT | 0777);
    inventario = (Inventario *)shmat(id_inventario, 0, 0);

    llave_carrito = ftok("Servidor3.c", 'k');
    id_carrito = shmget(llave_carrito, sizeof(Carrito), IPC_CREAT | 0777);
    carrito = (Carrito *)shmat(id_carrito, 0, 0);

    printf("\nAtendiendo al cliente %d...\n", getpid());
    printf("\nRecibiendo usuario: %s\n", usuario->user);
    printf("\nRecibiendo contrasena: %s\n", usuario->password);
    printf("\nTipo de usuario: %s\n", usuario->tipo);

    FILE *usuarios_file = fopen("usuarios.txt", "a");
    if (usuarios_file == NULL)
    {
        perror("Error al abrir archivo de usuarios");
        exit(1);
    }
    fprintf(usuarios_file, "%s %s %s\n", usuario->user, usuario->password, usuario->tipo);
    fclose(usuarios_file);

    if (!strcmp(usuario->tipo, "2"))
    {
        InventoryItem nuevo_item = inventario->items[inventario->item_count - 1];
        // Guardar el inventario actualizado en el archivo
        guardarInventarioEnArchivo(inventario);

        // Mostrar mensaje
        printf("\nCargando inventario...\n");
        sleep(1);
        printf("\nInventario listo\n");
        sleep(1);
    }
    else if (!strcmp(usuario->tipo, "1"))
    {

        printf("\nCargando inventario...\n");
        sleep(1);
        printf("\nInventario listo\n");
        sleep(1);
    }

    shmdt(usuario);
    shmdt(inventario);
    shmdt(carrito);
    pthread_exit(NULL);
}

/*
****************************CAMBIOS**************************
En la función main ahora se crea un hilo que ejecuta la parte del
cliente (Solo debe ejecutar el agregar producto)
La función en cuestion es *menu(void *)
*/
/*
Funciones para trabajar con los semáforos
*/
/**
 * Estructura para operaciones con los semáforos
 */
union semun
{
    int val;               // Valor para SETVAL
    struct semid_ds *buf;  // Buffer para IPC_STAT y IPC_SET
    unsigned short *array; // Array para GETALL y SETALL
};

/**
 * @brief Envía una señal al semáforo no. sem_num asociado al id semid
 *
 * @param semid ID del semáforo al cual "enviar la señal"
 * @param sem_num Número del semáforo asociado al ID. "0" si solo
 * hay un semáforo asociado
 *
 *
 */
void sem_signal(int semid, int sem_num)
{
    struct sembuf sem_op;
    sem_op.sem_num = sem_num;
    sem_op.sem_op = 1;
    sem_op.sem_flg = 0;
    if (semop(semid, &sem_op, 1) == -1)
    {
        perror("semop signal");
        exit(1);
    }
}

/*
    @brief Espera la señal de un semáforo
    @param semid ID del semáforo en cuestión
    @param sem_num Número de semáforo asociado
                            al ID del semáforo. Por defecto
                            se trata del semáforo 0, indicando
                            que solo hay un semáforo asociado
*/
void sem_wait(int semid, int sem_num)
{
    struct sembuf sem_op;
    sem_op.sem_num = sem_num;
    sem_op.sem_op = -1;
    sem_op.sem_flg = 0;
    if (semop(semid, &sem_op, 1) == -1)
    {
        perror("semop wait");
        exit(1);
    }
}

/*Función agregarProducto
Funcionamiento:
    Espera la señal del semáforo para comenzar con el proceso de registro
    de un nuevo producto. Leerá los datos del nuevo producto de la memoria compartida
*/
void agregarProducto()
{

    // Obtiene el espacio de memoria compartida donde se encuentra el inventario
    key_t llave_inventario = ftok("Servidor3.c", 'i');
    int id_inventario = shmget(llave_inventario, sizeof(Inventario), IPC_CREAT | 0777);
    Inventario *inventario = (Inventario *)shmat(id_inventario, 0, 0);

    // ID del semáforo que seleccionará la opcion en el menu
    int semID;
    // Key que permitirá acceder al espacio de memoria correspondiente a la opción
    key_t semKey = 2004, newItemKey = 2208;

    // Crear el semáforo
    semID = semget(semKey, 1, 0666 | IPC_CREAT);
    if (semID == -1)
    {
        perror("semget");
        exit(1);
    }

    // Inicializar el semáforo
    union semun sem_union;
    // Inicializar el semáforo a 0
    sem_union.val = 0;
    if (semctl(semID, 0, SETVAL, sem_union) == -1)
    {
        perror("semctl");
        exit(1);
    }

    // Obtiene la referencia al espacio de memoria compartida
    // La memoria compartida es tratada como un elemento Item
    int shmItemID = shmget(newItemKey, sizeof(InventoryItem), IPC_CREAT | 0777);
    if (shmItemID < 0)
    {
        perror("Error en shmget - agregarProducto (S): ");
        exit(1);
    }
    // Adjunta el espacio de memoria compartida al registro de memoria del hilo
    InventoryItem *nuevo_item = (InventoryItem *)shmat(shmItemID, NULL, 0);
    if (nuevo_item == (InventoryItem *)-1)
    {
        perror("Error en shmat - agregarProducto (S): ");
        exit(1);
    }

    // Espera a que el semáforo se active
    sem_wait(semID, 0);
    // COMIENZA CON EL PROCESO DE AGREGAR EL NUEVO PRODUCTO

    // Verifica que el inventario aún tenga espacio
    if (inventario->item_count >= MAX_INVENTORY_ITEMS)
    {
        printf("El inventario está lleno, no se pueden agregar más productos.\n");
        return;
    }

    // Incrementa el número de items y le asigna el nuevo producto
    inventario->items[inventario->item_count++] = *nuevo_item;

    // Abre el archivo de inventario
    FILE *inventario_file = fopen(INVENTARIO_FILE, "a");
    if (inventario_file == NULL)
    {
        perror("Error al abrir archivo de inventario");
        return;
    }
    // Escribe los datos del nuevo item en el archivo
    fprintf(inventario_file, "%d %s %s %.2f %d\n", nuevo_item->item_id, nuevo_item->item_name, nuevo_item->description, nuevo_item->price, nuevo_item->units);
    fclose(inventario_file);

    printf("Producto '%s' añadido al inventario con ID %d.\n", nuevo_item->item_name, nuevo_item->item_id);
    // Desadjuntar los segmentos de memoria
    shmdt(nuevo_item);
}

/*
Función para el thread que actuará como selector de opciones - menú
Funcionamiento:
    Esta función es llamada por un hilo(thread). Obtendrá la referencia al semáforo
    asociado con la key especificada en el código. Posteriormente, esperará a que el
    semáforo reciba una señal, permitiendo la ejecución de la función
    agregarProducto.
Consideraciones:
    No se como matar el hilo XD desconozco si muere junto con el servidor
*/
void *addProductLauncher(void *arg)
{

    // ID del semáforo que seleccionará la opcion en el menu
    int semID;
    // Key que permitirá acceder al espacio de memoria correspondiente a la opción
    key_t semKey = 2001;

    // Crear el semáforo
    semID = semget(semKey, 1, 0666 | IPC_CREAT);
    if (semID == -1)
    {
        perror("semget");
        exit(1);
    }

    // Inicializar el semáforo
    union semun sem_union;
    // Inicializar el semáforo a 0
    sem_union.val = 0;
    if (semctl(semID, 0, SETVAL, sem_union) == -1)
    {
        perror("semctl");
        exit(1);
    }
    // Indefinidamente esperará por si se quiere agregar un nuevo producto
    while (true)
    {
        sem_wait(semID, 0);
        agregarProducto();
    }
}

/*
************************************************************
*/

void create_lock_file()
{
    FILE *lock = fopen(LOCK_FILE, "w");
    if (lock == NULL)
    {
        // fprintf(stderr, "Error al crear el archivo de bloqueo: %s\n", strerror(errno));
        exit(1);
    }
    // printf("Archivo de bloqueo creado correctamente en %s\n", LOCK_FILE);
    fclose(lock);
}

void cleanup()
{
    // Eliminar el archivo de bloqueo
    if (unlink(LOCK_FILE) == -1)
    {
        perror("Error al finalizar adecuadamente el servidor");
    }
    else
    {
        printf("Servidor finalizado con exito.\n");
    }
}

void signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
        // Llamar a la función de limpieza al recibir SIGINT o SIGTERM
        cleanup();
        exit(EXIT_SUCCESS);
    }
}

int main()
{
    int mutex, clientes, i = 0;
    key_t llave_mutex, llave_clientes;
    pthread_t id_hilo;
    pthread_attr_t atributos;

    llave_mutex = ftok("Servidor3.c", 's');
    llave_clientes = ftok("Servidor3.c", 'c');

    clientes = crea_semaforo(llave_clientes, 0);
    mutex = crea_semaforo(llave_mutex, 0);

    pthread_attr_init(&atributos);
    pthread_attr_setdetachstate(&atributos, PTHREAD_CREATE_DETACHED);

    // Crear hilo para el menu
    pthread_t hilo_menu;
    pthread_create(&hilo_menu, &atributos, addProductLauncher, NULL);
    create_lock_file(); // Crear archivo de bloqueo
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    while (1)
    {
        printf("Servidor con pid %d esperando cliente...\n", getpid());
        down(clientes);
        down(mutex);
        pthread_create(&id_hilo, &atributos, servicio, NULL);
    }

    cleanup(); // Eliminar archivo de bloqueo
    return 0;
}
