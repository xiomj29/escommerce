#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "escomerce.h"

int crea_semaforo(key_t llave, int valor_inicial)
{
    int semid = semget(llave, 1, IPC_CREAT | 0777);
    if (semid == -1)
    {
        perror("semget");
        exit(1);
    }
    union semun arg;
    arg.val = valor_inicial;
    if (semctl(semid, 0, SETVAL, arg) == -1)
    {
        perror("semctl SETVAL");
        exit(1);
    }
    return semid;
}

int obtiene_semaforo(key_t llave)
{
    int semid = semget(llave, 1, 0);
    if (semid == -1)
    {
        perror("semget (obtiene)");
        exit(1);
    }
    return semid;
}

void sem_signal(int semid, int sem_num)
{
    struct sembuf sem_op = {.sem_num = sem_num, .sem_op = 1, .sem_flg = 0};
    if (semop(semid, &sem_op, 1) == -1)
    {
        perror("semop signal");
        exit(1);
    }
}

void sem_wait(int semid, int sem_num)
{
    struct sembuf sem_op = {.sem_num = sem_num, .sem_op = -1, .sem_flg = 0};
    if (semop(semid, &sem_op, 1) == -1)
    {
        perror("semop wait");
        exit(1);
    }
}

void down(int semid)
{
    struct sembuf op_p = {0, -1, 0};
    semop(semid, &op_p, 1);
}

void *attach_shm(key_t llave, size_t tam)
{
    int id = shmget(llave, tam, IPC_CREAT | 0777);
    if (id < 0)
    {
        perror("shmget");
        exit(1);
    }
    void *p = shmat(id, NULL, 0);
    if (p == (void *)-1)
    {
        perror("shmat");
        exit(1);
    }
    return p;
}

void guardarInventarioEnArchivo(Inventario *inventario)
{
    FILE *f = fopen(INVENTARIO_FILE, "w");
    if (f == NULL)
    {
        perror("Error al abrir archivo de inventario");
        return;
    }
    for (int i = 0; i < inventario->item_count; i++)
    {
        InventoryItem it = inventario->items[i];
        fprintf(f, "%d %s %s %.2f %d\n", it.item_id, it.item_name, it.description, it.price, it.units);
    }
    fclose(f);
}

void cargarInventarioDesdeArchivo(Inventario *inventario)
{
    inventario->item_count = 0;
    FILE *f = fopen(INVENTARIO_FILE, "r");
    if (f == NULL)
        return;

    while (inventario->item_count < MAX_INVENTORY_ITEMS)
    {
        InventoryItem it;
        int n = fscanf(f, "%d %49s %99s %f %d",
                       &it.item_id, it.item_name, it.description, &it.price, &it.units);
        if (n != 5)
            break;
        inventario->items[inventario->item_count++] = it;
    }
    fclose(f);
    printf("[servidor] Inventario cargado: %d producto(s).\n", inventario->item_count);
}

void *servicio(void *arg)
{
    Usuario *usuario = (Usuario *)attach_shm(KEY_SHM_USUARIO, sizeof(Usuario));

    printf("\n[servidor] Registrando usuario '%s' (tipo %s)...\n", usuario->user, usuario->tipo);

    FILE *usuarios_file = fopen(NOMBRE_ARCHIVO_USUARIOS, "a");
    if (usuarios_file == NULL)
    {
        perror("Error al abrir archivo de usuarios");
        pthread_exit(NULL);
    }
    fprintf(usuarios_file, "%s %s %s\n", usuario->user, usuario->password, usuario->tipo);
    fclose(usuarios_file);

    shmdt(usuario);
    pthread_exit(NULL);
}

void *addProductLauncher(void *arg)
{
    Inventario *inventario = (Inventario *)attach_shm(KEY_SHM_INVENTARIO, sizeof(Inventario));
    InventoryItem *nuevo = (InventoryItem *)attach_shm(KEY_SHM_NEWITEM, sizeof(InventoryItem));
    int req = obtiene_semaforo(KEY_SEM_ADD_REQ);
    int done = obtiene_semaforo(KEY_SEM_ADD_DONE);

    while (1)
    {
        sem_wait(req, 0);

        if (inventario->item_count >= MAX_INVENTORY_ITEMS)
        {
            printf("[servidor] Inventario lleno, no se agrego el producto.\n");
        }
        else
        {
            InventoryItem item = *nuevo;
            item.item_id = inventario->item_count + 1;
            inventario->items[inventario->item_count++] = item;
            guardarInventarioEnArchivo(inventario);
            printf("[servidor] Producto '%s' agregado con ID %d.\n", item.item_name, item.item_id);
        }

        sem_signal(done, 0);
    }
    return NULL;
}

void *selectProductLauncher(void *arg)
{
    Inventario *inventario = (Inventario *)attach_shm(KEY_SHM_INVENTARIO, sizeof(Inventario));
    Carrito *carrito = (Carrito *)attach_shm(KEY_SHM_CARRITO, sizeof(Carrito));
    SeleccionMsg *msg = (SeleccionMsg *)attach_shm(KEY_SHM_SELID, sizeof(SeleccionMsg));
    int req = obtiene_semaforo(KEY_SEM_SEL_REQ);
    int done = obtiene_semaforo(KEY_SEM_SEL_DONE);

    while (1)
    {
        sem_wait(req, 0);

        int found = 0;
        for (int i = 0; i < inventario->item_count; i++)
        {
            if (inventario->items[i].item_id == msg->item_id)
            {
                found = 1;
                if (inventario->items[i].units > 0)
                {
                    inventario->items[i].units--;
                    InventoryItem comprado = inventario->items[i];
                    comprado.units = 1;
                    if (carrito->item_count < MAX_INVENTORY_ITEMS)
                        carrito->items[carrito->item_count++] = comprado;
                    guardarInventarioEnArchivo(inventario);
                    msg->status = 0;
                    printf("[servidor] Producto %d agregado al carrito.\n", msg->item_id);
                }
                else
                {
                    msg->status = -2;
                }
                break;
            }
        }
        if (!found)
            msg->status = -1;

        sem_signal(done, 0);
    }
    return NULL;
}

void *ticketLauncher(void *arg)
{
    Carrito *carrito = (Carrito *)attach_shm(KEY_SHM_CARRITO, sizeof(Carrito));
    int *tnum = (int *)attach_shm(KEY_SHM_TKTNUM, sizeof(int));
    int req = obtiene_semaforo(KEY_SEM_TKT_REQ);
    int done = obtiene_semaforo(KEY_SEM_TKT_DONE);

    while (1)
    {
        sem_wait(req, 0);

        if (carrito->item_count == 0)
        {
            *tnum = 0;
            sem_signal(done, 0);
            continue;
        }

        int ticket_num;
        FILE *ticket_file = fopen(TICKETS_FILE, "r+");
        if (ticket_file == NULL)
        {
            ticket_file = fopen(TICKETS_FILE, "w");
            ticket_num = 1;
        }
        else
        {
            if (fscanf(ticket_file, "%d", &ticket_num) != 1)
                ticket_num = 0;
            ticket_num++;
            rewind(ticket_file);
        }
        fprintf(ticket_file, "%d", ticket_num);
        fclose(ticket_file);

        char ticket_filename[32];
        snprintf(ticket_filename, sizeof(ticket_filename), "ticket_%d.txt", ticket_num);
        FILE *ticket = fopen(ticket_filename, "w");
        if (ticket != NULL)
        {
            float total = 0.0f;
            fprintf(ticket, "===== TICKET #%d =====\n", ticket_num);
            for (int i = 0; i < carrito->item_count; i++)
            {
                InventoryItem it = carrito->items[i];
                fprintf(ticket, "ID: %d, PRODUCTO: %s, DESCRIPCION: %s, PRECIO: %.2f\n",
                        it.item_id, it.item_name, it.description, it.price);
                total += it.price;
            }
            fprintf(ticket, "TOTAL: %.2f\n", total);
            fclose(ticket);
            printf("[servidor] Ticket #%d generado (%d articulos).\n", ticket_num, carrito->item_count);
        }
        else
        {
            perror("Error al crear archivo de ticket");
        }

        carrito->item_count = 0;
        *tnum = ticket_num;
        sem_signal(done, 0);
    }
    return NULL;
}

void create_lock_file()
{
    FILE *lock = fopen(LOCK_FILE, "w");
    if (lock == NULL)
        exit(1);
    fclose(lock);
}

void cleanup()
{
    if (unlink(LOCK_FILE) == -1)
        perror("Error al finalizar adecuadamente el servidor");
    else
        printf("\nServidor finalizado con exito.\n");
}

void signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
        cleanup();
        exit(EXIT_SUCCESS);
    }
}

int main()
{
    pthread_t id_hilo;
    pthread_attr_t atributos;

    int clientes = crea_semaforo(KEY_SEM_CLIENTES, 0);
    int mutex = crea_semaforo(KEY_SEM_MUTEX, 0);

    crea_semaforo(KEY_SEM_ADD_REQ, 0);
    crea_semaforo(KEY_SEM_ADD_DONE, 0);
    crea_semaforo(KEY_SEM_SEL_REQ, 0);
    crea_semaforo(KEY_SEM_SEL_DONE, 0);
    crea_semaforo(KEY_SEM_TKT_REQ, 0);
    crea_semaforo(KEY_SEM_TKT_DONE, 0);

    attach_shm(KEY_SHM_USUARIO, sizeof(Usuario));
    attach_shm(KEY_SHM_NEWITEM, sizeof(InventoryItem));
    attach_shm(KEY_SHM_SELID, sizeof(SeleccionMsg));
    attach_shm(KEY_SHM_TKTNUM, sizeof(int));

    Inventario *inventario = (Inventario *)attach_shm(KEY_SHM_INVENTARIO, sizeof(Inventario));
    cargarInventarioDesdeArchivo(inventario);

    Carrito *carrito = (Carrito *)attach_shm(KEY_SHM_CARRITO, sizeof(Carrito));
    carrito->item_count = 0;

    pthread_attr_init(&atributos);
    pthread_attr_setdetachstate(&atributos, PTHREAD_CREATE_DETACHED);

    pthread_t hilo_add, hilo_sel, hilo_tkt;
    pthread_create(&hilo_add, &atributos, addProductLauncher, NULL);
    pthread_create(&hilo_sel, &atributos, selectProductLauncher, NULL);
    pthread_create(&hilo_tkt, &atributos, ticketLauncher, NULL);

    create_lock_file();
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    while (1)
    {
        printf("Servidor con pid %d esperando cliente...\n", getpid());
        down(clientes);
        down(mutex);
        pthread_create(&id_hilo, &atributos, servicio, NULL);
    }

    cleanup();
    return 0;
}
