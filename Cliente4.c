/*Funciones implementadas:
-Mostrar inventario (manejo de archivos, cliente y vendedor)
-Agregar productos (con manejo de archivos)
-Seleccionar producto (manejo de archivos)
-Ver carrito (manejo de archivos)
-Generar ticket (manejo de archivos)


Funciones de inicio de sesión e interfaz faltantes (TODAS) con manejo de archivos

*/

/*Compilación
    gcc Cliente2.c -o cliente -lform -lncurses -lcrypto*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <form.h>
#include <curses.h>
#include <menu.h>
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
#define BUFFER_SIZE 1024
#define LOCK_FILE "/tmp/server.lock"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define CTRLD 4

char *choices[] = {
    "Registrarse",
    "Iniciar Sesion",
    "Salir",
    (char *)NULL,
};

int n_choices = sizeof(choices) / sizeof(char *);
int startx = 0;
int starty = 0;
void print_in_middle(WINDOW *win, int starty, int startx, int width, char *string, chtype color);

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

/*
Funciones para trabajar con los semáforos
*/
union semun
{
    int val;               // Valor para SETVAL
    struct semid_ds *buf;  // Buffer para IPC_STAT y IPC_SET
    unsigned short *array; // Array para GETALL y SETALL
};
//"Envía" una señal en el semáforo
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

/*Función void semwait(int semid, int sem_num)
RECIBE:
    int semid       -       ID del semáforo en cuestión
    int sem_num     -       Número de semáforo asociado
                            al ID del semáforo. Por defecto
                            se trata del semáforo 0, indicando
                            que solo hay un semáforo asociado
FUNCIONAMIENTO:
    Espera la señal de un semáforo
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

void replaceSpacesWithUnderscores(char *str)
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == ' ')
        {
            str[i] = '_';
        }
    }
}

void replaceUnderscoresWithSpaces(char *str)
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == '_')
        {
            str[i] = ' ';
        }
    }
}

int isValidString(char *str)
{
    return str != NULL && strlen(str) > 0;
}

int isValidPrice(float price)
{
    return price >= 0.0;
}

int isValidUnits(int units)
{
    return units >= 0;
}

int Ingresa_semaforo(key_t llave)
{
    int semid = semget(llave, 1, 0); // ingreso al semaforo creado con esta llave
    if (semid == -1)
    {
        printf("Error al ingresar al semáforo\n");
        return -1;
    }
    return semid;
}

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

int check_server_running()
{
    // Verificar si el archivo de bloqueo existe y tiene permisos de lectura
    if (access(LOCK_FILE, F_OK | R_OK) == 0)
    {
        return 1; // El archivo de bloqueo existe y tiene permisos
    }
    else
    {
        return 0; // El archivo de bloqueo no existe o no tiene permisos adecuados
    }
}

void validarServidor()
{
    // Verificar si el servidor está corriendo antes de mostrar el menú
    if (!check_server_running())
    {
        fprintf(stderr, "El servidor no está en ejecución, ejecute primero el servidor\n");
        exit(EXIT_FAILURE);
    }
}

void cargar_usuarios(Usuario usuarios[], int *cantidad_usuarios)
{
    FILE *archivo = fopen(NOMBRE_ARCHIVO_USUARIOS, "r");
    if (!archivo)
    {
        return;
    }

    while (fscanf(archivo, "%s %s %s", usuarios[*cantidad_usuarios].user, usuarios[*cantidad_usuarios].password, usuarios[*cantidad_usuarios].tipo) != EOF)
    {
        (*cantidad_usuarios)++;
    }

    fclose(archivo);
}

Usuario encontrar_usuario(Usuario usuarios[], int cantidad_usuarios, const char *nombre_usuario)
{
    Usuario prueba = (Usuario){.user = "", .password = "", .tipo = "9"};
    for (int i = 0; i < cantidad_usuarios; i++)
    {
        if (strcmp(usuarios[i].user, nombre_usuario) == 0)
        {
            return usuarios[i];
        }
    }
    return prueba;
}

void mostrarInventario()
{
    key_t llave_inventario = ftok("Servidor3.c", 'i');
    int id_inventario = shmget(llave_inventario, sizeof(Inventario), IPC_CREAT | 0777);
    Inventario *inventario = (Inventario *)shmat(id_inventario, 0, 0);

    printf("\n======================================\n");
    printf("         INVENTARIO DISPONIBLE\n");
    printf("======================================\n");
    for (int i = 0; i < inventario->item_count; i++)
    {
        InventoryItem item = inventario->items[i];
        replaceUnderscoresWithSpaces(item.item_name);
        replaceUnderscoresWithSpaces(item.description);
        printf("ID: %d, PRODUCTO: %s, DESCRIPCION: %s, PRECIO: %.2f, PZAS DISPONIBLES: %d\n",
               item.item_id, item.item_name, item.description, item.price, item.units);
    }
    printf("======================================\n");

    shmdt(inventario);
}

void mostrarCarrito()
{
    key_t llave_carrito = ftok("Servidor3.c", 'k');
    int id_carrito = shmget(llave_carrito, sizeof(Carrito), 0777);
    Carrito *carrito = (Carrito *)shmat(id_carrito, 0, 0);

    printf("\n======================================\n");
    printf("                TU CARRITO\n");
    printf("======================================\n");
    for (int i = 0; i < carrito->item_count; i++)
    {
        InventoryItem item = carrito->items[i];
        replaceUnderscoresWithSpaces(item.item_name);
        replaceUnderscoresWithSpaces(item.description);
        printf("ID: %d, PRODUCTO: %s, DESCRIPCION: %s, PRECIO: %.2f\n",
               item.item_id, item.item_name, item.description, item.price);
    }
    printf("======================================\n");

    shmdt(carrito);
}

void seleccionarProducto()
{
    int item_id, found = 0;
    printf("Ingrese el ID del producto que desea seleccionar: ");
    scanf("%d", &item_id);

    FILE *inventario_file = fopen(INVENTARIO_FILE, "r");
    if (inventario_file == NULL)
    {
        perror("Error al abrir archivo de inventario");
        return;
    }

    FILE *temp_file = fopen("temp_inventario.txt", "w");
    if (temp_file == NULL)
    {
        perror("Error al abrir archivo temporal");
        fclose(inventario_file);
        return;
    }

    char buffer[BUFFER_SIZE];
    InventoryItem selected_item;

    while (fgets(buffer, BUFFER_SIZE, inventario_file) != NULL)
    {
        InventoryItem item;
        sscanf(buffer, "%d %s %s %f %d", &item.item_id, item.item_name, item.description, &item.price, &item.units);

        if (item.item_id == item_id)
        {
            found = 1;
            if (item.units > 0)
            {
                selected_item = item;
                selected_item.units--; // Decrementar la cantidad en 1
                if (selected_item.units > 0)
                {
                    fprintf(temp_file, "%d %s %s %.2f %d\n", selected_item.item_id, selected_item.item_name, selected_item.description, selected_item.price, selected_item.units);
                }
            }
            else
            {
                printf("El producto no está disponible.\n");
            }
        }
        else
        {
            fprintf(temp_file, "%d %s %s %.2f %d\n", item.item_id, item.item_name, item.description, item.price, item.units);
        }
    }

    fclose(inventario_file);
    fclose(temp_file);

    if (found)
    {
        remove(INVENTARIO_FILE);
        rename("temp_inventario.txt", INVENTARIO_FILE);

        key_t llave_carrito = ftok("Servidor3.c", 'k');
        int id_carrito = shmget(llave_carrito, sizeof(Carrito), IPC_CREAT | 0777);
        Carrito *carrito = (Carrito *)shmat(id_carrito, 0, 0);

        carrito->items[carrito->item_count++] = selected_item;

        shmdt(carrito);
    }
    else
    {
        printf("Producto no encontrado.\n");
        remove("temp_inventario.txt");
    }
}

void generarTicket()
{
    key_t llave_carrito = ftok("Servidor3.c", 'k');
    int id_carrito = shmget(llave_carrito, sizeof(Carrito), 0777);
    Carrito *carrito = (Carrito *)shmat(id_carrito, 0, 0);

    printf("\n======================================\n");
    printf("                TU CARRITO\n");
    printf("======================================\n");
    float total = 0.0;
    for (int i = 0; i < carrito->item_count; i++)
    {
        InventoryItem item = carrito->items[i];
        replaceUnderscoresWithSpaces(item.item_name);
        replaceUnderscoresWithSpaces(item.description);
        printf("ID: %d, PRODUCTO: %s, DESCRIPCION: %s, PRECIO: %.2f\n",
               item.item_id, item.item_name, item.description, item.price);
        total += item.price;
    }
    printf("======================================\n");
    printf("TOTAL: %.2f\n", total);

    int finalizar_compra;
    printf("Presione 1 para finalizar la compra, 0 para seguir comprando: ");
    scanf("%d", &finalizar_compra);

    if (finalizar_compra == 1)
    {
        // Solicitar número de ticket al servidor
        FILE *ticket_file = fopen("tickets.txt", "r+");
        int ticket_num;
        if (ticket_file == NULL)
        {
            ticket_file = fopen("tickets.txt", "w");
            ticket_num = 1;
            fprintf(ticket_file, "%d", ticket_num);
        }
        else
        {
            fscanf(ticket_file, "%d", &ticket_num);
            ticket_num++;
            fseek(ticket_file, 0, SEEK_SET);
            fprintf(ticket_file, "%d", ticket_num);
        }
        fclose(ticket_file);

        // Crear archivo de ticket
        char ticket_filename[20];
        sprintf(ticket_filename, "ticket_%d.txt", ticket_num);
        FILE *ticket = fopen(ticket_filename, "w");
        if (ticket == NULL)
        {
            perror("Error al crear archivo de ticket");
            shmdt(carrito);
            return;
        }

        for (int i = 0; i < carrito->item_count; i++)
        {
            InventoryItem item = carrito->items[i];
            fprintf(ticket, "ID: %d, PRODUCTO: %s, DESCRIPCION: %s, PRECIO: %.2f\n",
                    item.item_id, item.item_name, item.description, item.price);
        }
        fprintf(ticket, "TOTAL: %.2f\n", total);
        fclose(ticket);

        // Limpiar carrito
        carrito->item_count = 0;
    }

    shmdt(carrito);
}

void menuCliente()
{
    int highlight = 1;
    int choice = 0;
    int salir = 0;
    WINDOW *sub_win = newwin(HEIGHT, WIDTH, starty, startx);
    keypad(sub_win, TRUE);

    char *acciones[] = {
        "Mostrar inventario",
        "Seleccionar producto",
        "Ver carrito",
        "Generar ticket final",
        "Salir",
    };
    int n_acciones = sizeof(acciones) / sizeof(char *);
    highlight = 1;
    choice = 0;
    int c;

    while (!salir)
    {
        clear();
        refresh();
        box(sub_win, 0, 0);
        for (int i = 0; i < n_acciones; ++i)
        {
            if (highlight == i + 1)
            {
                wattron(sub_win, A_REVERSE);
                mvwprintw(sub_win, i + 2, 2, "%s", acciones[i]);
                wattroff(sub_win, A_REVERSE);
            }
            else
            {
                mvwprintw(sub_win, i + 2, 2, "%s", acciones[i]);
            }
        }
        wrefresh(sub_win);
        c = wgetch(sub_win);
        switch (c)
        {
        case KEY_UP:
            if (highlight == 1)
                highlight = n_acciones;
            else
                --highlight;
            break;
        case KEY_DOWN:
            if (highlight == n_acciones)
                highlight = 1;
            else
                ++highlight;
            break;
        case 10:
            choice = highlight;
            break;
        default:
            refresh();
            break;
        }
        if (choice != 0)
        {
            clrtoeol();
            refresh();
            endwin();
            if (choice == 1)
            {
                validarServidor();
                mostrarInventario();
                getch();
            }
            else if (choice == 2)
            {
                validarServidor();
                seleccionarProducto();
                getch();
            }
            else if (choice == 3)
            {
                validarServidor();
                mostrarCarrito();
                getch();
            }
            else if (choice == 4)
            {
                validarServidor();
                generarTicket();
                getch();
            }
            else if (choice == 5)
            {
                endwin();
                exit(-1);
            }
            choice = 0;
        }
    }

    clrtoeol();
    refresh();
    endwin();
    /*int opcion;
    while (1)
    {

        printf("\nMenu Cliente:\n");
        printf("1. Mostrar inventario\n");
        printf("2. Seleccionar producto\n");
        printf("3. Ver carrito\n");
        printf("4. Generar ticket\n");
        printf("5. Cerrar sesión\n");
        printf("Selecciona una opción: ");
        scanf("%d", &opcion);
        switch (opcion)
        {
        case 1:
            validarServidor();
            mostrarInventario();
            break;
        case 2:
            validarServidor();
            seleccionarProducto();
            break;
        case 3:
            validarServidor();
            mostrarCarrito();
            break;
        case 4:
            validarServidor();
            generarTicket();
            break;
        case 5:
            validarServidor();
            exit(-1);
        default:
            validarServidor();
            printf("Opción inválida.\n");
        }
    }*/
}

/**
 * @brief Agrega un producto al inventario dado
 *
 * @param inventario Referencia al inventario donde se va a trabajar
 */
void agregarProducto(Inventario *inventario)
{
    // Verifica que el inventario aún tenga espacio
    if (inventario->item_count >= MAX_INVENTORY_ITEMS)
    {
        printf("El inventario está lleno, no se pueden agregar más productos.\n");
        return;
    }

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
    int shmItemID = shmget(newItemKey, sizeof(InventoryItem), 0777 | IPC_CREAT);
    if (shmItemID < 0)
    {
        perror("Error en shmget - agregar producto (C): ");
        exit(1);
    }
    // Adjunta el espacio de memoria compartida al registro de memoria del hilo
    InventoryItem *nuevo_item = (InventoryItem *)shmat(shmItemID, NULL, 0);
    if (nuevo_item == (InventoryItem *)-1)
    {
        perror("Error en shmat - agregarProducto (C): ");
        exit(1);
    }

    // Asigna ID de acuerdo al numero de items en el inventario
    nuevo_item->item_id = inventario->item_count + 1;
    // Ingresa los datos del nuevo producto
    printf("Ingrese el nombre del producto: ");
    scanf(" %[^\n]s", nuevo_item->item_name);
    replaceSpacesWithUnderscores(nuevo_item->item_name);
    printf("Ingrese el precio del producto: ");
    scanf("%f", &nuevo_item->price);
    printf("Ingrese la descripción del producto (máx 100 caracteres): ");
    scanf(" %[^\n]s", nuevo_item->description);
    replaceSpacesWithUnderscores(nuevo_item->description);
    printf("Ingrese la cantidad de producto que tiene a su disposición: ");
    scanf("%d", &nuevo_item->units);

    // Envía la señal al semáforo indicando que ya es posible leer de la memoria
    sem_signal(semID, 0);
}

/**
 * @brief Menú desplegado para los usuarios Vendedores.
 * Envía una señal al semáforo especificado en caso de seleccionarse
 * la opción de agregar un producto.
 *
 * @param inventario Referencia al inventario sobre el cual se está trabajando
 */
void menuVendedor(Inventario *inventario)
{

    // ID del semáforo que seleccionará la opcion en el menu
    int semID, opcion;
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

    int highlight = 1;
    int choice = 0;
    int salir = 0;
    WINDOW *sub_win = newwin(HEIGHT, WIDTH, starty, startx);
    keypad(sub_win, TRUE);

    char *acciones[] = {
        "Mostrar inventario",
        "Agregar producto",
        "Salir",
    };
    int n_acciones = sizeof(acciones) / sizeof(char *);
    highlight = 1;
    choice = 0;
    int c;

    while (!salir)
    {
        clear();
        refresh();
        box(sub_win, 0, 0);
        for (int i = 0; i < n_acciones; ++i)
        {
            if (highlight == i + 1)
            {
                wattron(sub_win, A_REVERSE);
                mvwprintw(sub_win, i + 2, 2, "%s", acciones[i]);
                wattroff(sub_win, A_REVERSE);
            }
            else
            {
                mvwprintw(sub_win, i + 2, 2, "%s", acciones[i]);
            }
        }
        wrefresh(sub_win);
        c = wgetch(sub_win);
        switch (c)
        {
        case KEY_UP:
            if (highlight == 1)
                highlight = n_acciones;
            else
                --highlight;
            break;
        case KEY_DOWN:
            if (highlight == n_acciones)
                highlight = 1;
            else
                ++highlight;
            break;
        case 10:
            choice = highlight;
            break;
        default:
            refresh();
            break;
        }
        if (choice != 0)
        {
            clrtoeol();
            refresh();
            endwin();
            if (choice == 1)
            {
                validarServidor();
                mostrarInventario();
                getch();
            }
            else if (choice == 2)
            {
                validarServidor();
                sem_signal(semID, 0);
                sleep(1);
                agregarProducto(inventario);
                getch();
            }

            else if (choice == 3)
            {
                salir = 1;
                endwin();
                exit(-1);
            }
            choice = 0;
        }
    }

    clrtoeol();
    refresh();
    endwin();
}

void quitarEspacios(char *palabra)
{
    int i;
    int count = 0;
    for (i = 0; palabra[i]; i++)
    {
        if (palabra[i] != ' ')
        {
            palabra[count++] = palabra[i];
        }
    }
    palabra[count] = '\0';
}

void hashear_contrasena(const char *contrasena, char *contrasena_hasheada)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char *)contrasena, strlen(contrasena), hash);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        sprintf(contrasena_hasheada + (i * 2), "%02x", hash[i]);
    }
    contrasena_hasheada[SHA256_DIGEST_LENGTH * 2] = '\0';
}

int iniciar_sesion(WINDOW *win)
{
    char contrasena_hasheada[SHA256_DIGEST_LENGTH * 2 + 1];
    int inicioCorrecto = 0;
    werase(win);
    FIELD *field[3];
    FORM *my_form;
    int ch, rows, cols;

    /* Initialize curses */
    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    /* Initialize few color pairs */
    init_pair(1, COLOR_RED, COLOR_BLACK);

    /* Initialize the fields */
    field[0] = new_field(1, 20, 6, 14, 0, 0);
    field[1] = new_field(1, 20, 8, 14, 0, 0);
    field[2] = NULL;

    /* Set field options */
    set_field_back(field[0], A_UNDERLINE);
    field_opts_off(field[0], O_AUTOSKIP); /* Don't go to next field when this */
                                          /* Field is filled up             */
    set_field_back(field[1], A_UNDERLINE);
    field_opts_off(field[1], O_AUTOSKIP);

    /* Create the form and post it */
    my_form = new_form(field);

    /* Calculate the area required for the form */
    scale_form(my_form, &rows, &cols);

    /* Create the window to be associated with the form */
    win = newwin(rows + 8, cols + 20, 2, 4);
    keypad(win, TRUE);

    /* Set main window and sub window */
    set_form_win(my_form, win);
    set_form_sub(my_form, derwin(win, rows, cols, 2, 2));

    /* Print a border around the main window and print a title */
    box(win, 0, 0);
    print_in_middle(win, 1, 0, cols + 10, "Iniciar Sesion", COLOR_PAIR(1));

    post_form(my_form);
    wrefresh(win);
    refresh();

    mvwprintw(win, 8, 1, "Usuario:");
    mvwprintw(win, 10, 1, "Contraseña:");
    refresh();

    char pass[20] = {'\0'};
    int posPass = 0;

    /* Loop through to get user requests */
     while ((ch = wgetch(win)) != KEY_F(1) && !inicioCorrecto)
    {
        switch (ch)
        {
        case KEY_DOWN:
            /* Go to next field */
            form_driver(my_form, REQ_NEXT_FIELD);
            /* Go to the end of the present buffer */
            /* Leaves nicely at the last character */
            form_driver(my_form, REQ_END_LINE);
            break;
        case KEY_UP:
            /* Go to previous field */
            form_driver(my_form, REQ_PREV_FIELD);
            form_driver(my_form, REQ_END_LINE);
            break;
        case KEY_BACKSPACE:
            form_driver(my_form, REQ_PREV_CHAR);
            form_driver(my_form, REQ_DEL_CHAR);
            pass[posPass] = '\0';
            if (posPass > 0)
            {
                posPass--;
            }
            break;
        case 10:
            Usuario usuarios[MAX_USUARIOS];
            int cantidad_usuarios = 0;
            int id_inventario = shmget(ftok("Servidor3.c", 'i'), sizeof(Inventario), IPC_CREAT | 0777);
            Inventario *inventario = (Inventario *)shmat(id_inventario, 0, 0);
            cargar_usuarios(usuarios, &cantidad_usuarios);
            form_driver(my_form, REQ_VALIDATION);
            quitarEspacios(field_buffer(field[0], 0));
            Usuario validado = encontrar_usuario(usuarios, cantidad_usuarios, field_buffer(field[0], 0));
            if (!strcmp(validado.tipo, "9"))
            {
                mvwprintw(win, 3, 1, "Usuario no encontrado!");
                mvwprintw(win, 4, 1, "%s", validado.user);
            }
            else
            {
                // Hash the entered password
                hashear_contrasena(pass, contrasena_hasheada);
                if (strcmp(validado.password, contrasena_hasheada) == 0)
                {
                    if (!strcmp(validado.tipo, "1"))
                    {
                        menuCliente();
                    }
                    else
                    {
                        menuVendedor(inventario);
                    }
                    inicioCorrecto = 1;
                }
                else
                {
                    mvwprintw(win, 3, 1, "Contraseña invalida!");
                }
            }

            break;
	default:
		/* If this is a normal character, it gets */
		/* Printed                                */
		if (current_field(my_form) == field[1])
		{
		form_driver(my_form, '*');
		pass[posPass] = (char)ch;
		posPass++;
		}
		else
		{
		form_driver(my_form, ch);
		}

		break;

        }
    }

    /* Un post form and free the memory */
    unpost_form(my_form);
    free_form(my_form);
    free_field(field[0]);
    free_field(field[1]);

    endwin();
}

void registrar_usuario(WINDOW *win)
{
    int mutex = Ingresa_semaforo(ftok("Servidor3.c", 's'));
    int clientes = Ingresa_semaforo(ftok("Servidor3.c", 'c'));
    int id_usuario = shmget(ftok("Servidor3.c", 'u'), sizeof(Usuario), IPC_CREAT | 0777);
    Usuario *usuario = (Usuario *)shmat(id_usuario, 0, 0);

    int id_inventario = shmget(ftok("Servidor3.c", 'i'), sizeof(Inventario), IPC_CREAT | 0777);
    Inventario *inventario = (Inventario *)shmat(id_inventario, 0, 0);
    char contrasena_hasheada[SHA256_DIGEST_LENGTH * 2 + 1];
    int inicioCorrecto = 0;
    werase(win);
    FIELD *field[4];
    FORM *my_form;
    int ch, rows, cols;

    /* Initialize curses */
    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    /* Initialize few color pairs */
    init_pair(1, COLOR_RED, COLOR_BLACK);

    /* Initialize the fields */
    field[0] = new_field(1, 20, 6, 14, 0, 0);
    field[1] = new_field(1, 20, 8, 14, 0, 0);
    field[2] = new_field(1, 10, 10, 14, 0, 0);
    field[3] = NULL;

    /* Set field options */
    set_field_back(field[0], A_UNDERLINE);
    field_opts_off(field[0], O_AUTOSKIP); /* Don't go to next field when this */
                                          /* Field is filled up             */
    set_field_back(field[1], A_UNDERLINE);
    field_opts_off(field[1], O_AUTOSKIP);

    set_field_back(field[2], A_UNDERLINE);
    field_opts_off(field[2], O_AUTOSKIP);

    /* Create the form and post it */
    my_form = new_form(field);

    /* Calculate the area required for the form */
    scale_form(my_form, &rows, &cols);

    /* Create the window to be associated with the form */
    win = newwin(rows + 8, cols + 20, 2, 4);
    keypad(win, TRUE);

    /* Set main window and sub window */
    set_form_win(my_form, win);
    set_form_sub(my_form, derwin(win, rows, cols, 2, 2));

    /* Print a border around the main window and print a title */
    box(win, 0, 0);
    print_in_middle(win, 1, 0, cols + 10, "Registrar Usuario", COLOR_PAIR(1));

    post_form(my_form);
    wrefresh(win);
    refresh();

    mvwprintw(win, 8, 1, "Usuario:");
    mvwprintw(win, 10, 1, "Contraseña:");
    mvwprintw(win, 12, 1, "Tipo:");
    refresh();

    char pass[20] = {'\0'};
    int posPass = 0;

    /* Loop through to get user requests */
    while ((ch = wgetch(win)) != KEY_F(1) && !inicioCorrecto)
    {
        switch (ch)
        {
        case KEY_DOWN:
            /* Go to next field */
            form_driver(my_form, REQ_NEXT_FIELD);
            /* Go to the end of the present buffer */
            /* Leaves nicely at the last character */
            form_driver(my_form, REQ_END_LINE);
            break;
        case KEY_UP:
            /* Go to previous field */
            form_driver(my_form, REQ_PREV_FIELD);
            form_driver(my_form, REQ_END_LINE);
            break;
        case KEY_BACKSPACE:
            form_driver(my_form, REQ_PREV_CHAR);
            form_driver(my_form, REQ_DEL_CHAR);
            pass[posPass] = '\0';
            if (posPass > 0)
            {
                posPass--;
            }
            break;
        case 10:
            form_driver(my_form, REQ_VALIDATION);
            quitarEspacios(field_buffer(field[0], 0));
            quitarEspacios(field_buffer(field[2], 0));
            strcpy(usuario->user, field_buffer(field[0], 0));
            // Hash the password before storing it
            hashear_contrasena(pass, contrasena_hasheada);
            strcpy(usuario->password, contrasena_hasheada);
            strcpy(usuario->tipo, field_buffer(field[2], 0));
            up(mutex);
            unpost_form(my_form);
            free_form(my_form);
            free_field(field[0]);
            free_field(field[1]);
            free_field(field[2]);
            werase(win);
            endwin();
            if (!strcmp(usuario->tipo, "1"))
            {
                menuCliente();
            }
            else if (!strcmp(usuario->tipo, "2"))
            {
                menuVendedor(inventario);
            }
            break;
        default:
            /* If this is a normal character, it gets */
            /* Printed                                */
            if (current_field(my_form) == field[1])
            {
                form_driver(my_form, '*');
                pass[posPass] = (char)ch;
                posPass++;
            }
            else
            {
                form_driver(my_form, ch);
            }

            break;
        }
    }

    /* Un post form and free the memory */
    unpost_form(my_form);
    free_form(my_form);
    free_field(field[0]);
    free_field(field[1]);
    free_field(field[2]);

    endwin();
}

void print_in_middle(WINDOW *win, int starty, int startx, int width, char *string, chtype color)
{
    int length, x, y;
    float temp;

    if (win == NULL)
        win = stdscr;
    getyx(win, y, x);
    if (startx != 0)
        x = startx;
    if (starty != 0)
        y = starty;
    if (width == 0)
        width = 80;

    length = strlen(string);
    temp = (width - length) / 2;
    x = startx + (int)temp;
    wattron(win, color);
    mvwprintw(win, y, x, "%s", string);
    wattroff(win, color);
    refresh();
}

int main()
{

    validarServidor();

    int clientes, id_usuario, mutex;
    Usuario *usuario;
    Inventario *inventario;
    Carrito *carrito;
    key_t llave_clientes, llave_usuario, llave_mutex, llave_inventario, llave_carrito;

    llave_clientes = ftok("Servidor3.c", 'c');
    llave_usuario = ftok("Servidor3.c", 'u');
    llave_mutex = ftok("Servidor3.c", 's');
    llave_inventario = ftok("Servidor3.c", 'i');
    llave_carrito = ftok("Servidor3.c", 'k');

    clientes = crea_semaforo(llave_clientes, 0);
    mutex = crea_semaforo(llave_mutex, 0);
    id_usuario = shmget(llave_usuario, sizeof(Usuario), IPC_CREAT | 0777);
    usuario = (Usuario *)shmat(id_usuario, 0, 0);

    int id_inventario = shmget(llave_inventario, sizeof(Inventario), IPC_CREAT | 0777);
    inventario = (Inventario *)shmat(id_inventario, 0, 0);

    int id_carrito = shmget(llave_carrito, sizeof(Carrito), IPC_CREAT | 0777);
    carrito = (Carrito *)shmat(id_carrito, 0, 0);

    up(clientes);

    ITEM **my_items;
    int c;
    MENU *my_menu;
    WINDOW *my_menu_win;
    int n_choices, i;

    /* Initialize curses */
    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_CYAN, COLOR_BLACK);

    /* Create items */
    n_choices = ARRAY_SIZE(choices);
    my_items = (ITEM **)calloc(n_choices, sizeof(ITEM *));
    for (i = 0; i < n_choices; ++i)
    {
        my_items[i] = new_item(choices[i], choices[i]);
    }
    set_item_userptr(my_items[0], registrar_usuario);
    set_item_userptr(my_items[1], iniciar_sesion);
    /* Crate menu */
    my_menu = new_menu((ITEM **)my_items);

    /* Create the window to be associated with the menu */
    my_menu_win = newwin(10, 40, 4, 4);
    keypad(my_menu_win, TRUE);
    menu_opts_off(my_menu, O_SHOWDESC);

    /* Set main window and sub window */
    set_menu_win(my_menu, my_menu_win);
    set_menu_sub(my_menu, derwin(my_menu_win, 6, 38, 3, 1));
    set_menu_format(my_menu, 5, 1);

    /* Set menu mark to the string " * " */
    set_menu_mark(my_menu, " * ");

    /* Print a border around the main window and print a title */
    box(my_menu_win, 0, 0);
    print_in_middle(my_menu_win, 1, 0, 40, "Inicio ESCOMERCE", COLOR_PAIR(1));
    mvwaddch(my_menu_win, 2, 0, ACS_LTEE);
    mvwhline(my_menu_win, 2, 1, ACS_HLINE, 38);
    mvwaddch(my_menu_win, 2, 39, ACS_RTEE);

    /* Post the menu */
    post_menu(my_menu);
    wrefresh(my_menu_win);

    attron(COLOR_PAIR(2));
    mvprintw(LINES - 1, 0, "Usa las flechas para elegir una opcion");
    attroff(COLOR_PAIR(2));
    refresh();

    while ((c = wgetch(my_menu_win)) != KEY_F(1))
    {
        switch (c)
        {
        case KEY_DOWN:
            menu_driver(my_menu, REQ_DOWN_ITEM);
            break;
        case KEY_UP:
            menu_driver(my_menu, REQ_UP_ITEM);
            break;
        case KEY_NPAGE:
            menu_driver(my_menu, REQ_SCR_DPAGE);
            break;
        case KEY_PPAGE:
            menu_driver(my_menu, REQ_SCR_UPAGE);
            break;
        case 10: /* Enter */
        {
            ITEM *cur;
            void (*p)(WINDOW *);

            cur = current_item(my_menu);
            p = item_userptr(cur);
            p(my_menu_win);
            break;
        }
        }
        wrefresh(my_menu_win);
    }

    /* Unpost and free all the memory taken up */
    unpost_menu(my_menu);
    free_menu(my_menu);
    for (i = 0; i < n_choices; ++i)
        free_item(my_items[i]);
    endwin();
    /*while (1)
    {
        system("clear");
        printf("\nSoy el proceso: %d\n", getpid());
        printf("\nEscribe nombre de usuario: ");
        scanf("%s", usuario->user);
        printf("\nEscribe contrasena: ");
        scanf("%s", usuario->password);
        printf("\nTipo de usuario (1 para cliente, 2 para vendedor): ");
        scanf("%d", &usuario->tipo);

        sleep(1);

        if (usuario->tipo == 1)
        {
            menuCliente();
        }
        else if (usuario->tipo == 2)
        {
            menuVendedor(inventario);
        }
    }*/

    shmdt(usuario);
    shmdt(inventario);
    shmdt(carrito);
    return 0;
}
