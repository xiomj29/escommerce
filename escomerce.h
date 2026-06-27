#ifndef ESCOMERCE_H
#define ESCOMERCE_H

#define MAX_ITEM_NAME_LENGTH 50
#define MAX_DESCRIPTION_LENGTH 100
#define MAX_INVENTORY_ITEMS 100
#define MAX_USERNAME_LENGTH 50
#define MAX_PASSWORD_LENGTH 80
#define MAX_USUARIOS 100

#define NOMBRE_ARCHIVO_USUARIOS "usuarios.txt"
#define INVENTARIO_FILE "inventario.txt"
#define TICKETS_FILE "tickets.txt"
#define LOCK_FILE "/tmp/server.lock"

#define KEY_SEM_CLIENTES 4001
#define KEY_SEM_MUTEX 4002
#define KEY_SHM_USUARIO 4003
#define KEY_SHM_INVENTARIO 4004
#define KEY_SHM_CARRITO 4005

#define KEY_SEM_ADD_REQ 4010
#define KEY_SEM_ADD_DONE 4011
#define KEY_SHM_NEWITEM 4012

#define KEY_SEM_SEL_REQ 4020
#define KEY_SEM_SEL_DONE 4021
#define KEY_SHM_SELID 4022

#define KEY_SEM_TKT_REQ 4030
#define KEY_SEM_TKT_DONE 4031
#define KEY_SHM_TKTNUM 4032

typedef struct
{
    char user[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
    char tipo[8];
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

typedef struct
{
    int item_id;
    int status;
} SeleccionMsg;

union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

#endif
