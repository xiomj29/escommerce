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

#include "escomerce.h"

#define WIDTH 30
#define HEIGHT 10
#define BUFFER_SIZE 1024
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
    int semid = semget(llave, 1, 0);
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
    union semun arg;
    arg.val = valor_inicial;
    semctl(semid, 0, SETVAL, arg);
    return semid;
}

void down(int semid)
{
    struct sembuf op_p = {0, -1, 0};
    semop(semid, &op_p, 1);
}

void up(int semid)
{
    struct sembuf op_v = {0, +1, 0};
    semop(semid, &op_v, 1);
}

int check_server_running()
{

    if (access(LOCK_FILE, F_OK | R_OK) == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void validarServidor()
{

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

    while (*cantidad_usuarios < MAX_USUARIOS &&
           fscanf(archivo, "%49s %79s %7s",
                  usuarios[*cantidad_usuarios].user,
                  usuarios[*cantidad_usuarios].password,
                  usuarios[*cantidad_usuarios].tipo) == 3)
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
    key_t llave_inventario = KEY_SHM_INVENTARIO;
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
    key_t llave_carrito = KEY_SHM_CARRITO;
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
    int item_id;
    printf("Ingrese el ID del producto que desea seleccionar: ");
    scanf("%d", &item_id);

    int id_sel = shmget(KEY_SHM_SELID, sizeof(SeleccionMsg), IPC_CREAT | 0777);
    SeleccionMsg *msg = (SeleccionMsg *)shmat(id_sel, 0, 0);
    msg->item_id = item_id;
    msg->status = 0;

    int req = Ingresa_semaforo(KEY_SEM_SEL_REQ);
    int done = Ingresa_semaforo(KEY_SEM_SEL_DONE);
    sem_signal(req, 0);
    sem_wait(done, 0);

    if (msg->status == 0)
        printf("Producto agregado al carrito.\n");
    else if (msg->status == -2)
        printf("El producto no está disponible (sin stock).\n");
    else
        printf("Producto no encontrado.\n");

    shmdt(msg);
}

void generarTicket()
{
    int id_carrito = shmget(KEY_SHM_CARRITO, sizeof(Carrito), 0777);
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
    shmdt(carrito);

    int finalizar_compra;
    printf("Presione 1 para finalizar la compra, 0 para seguir comprando: ");
    scanf("%d", &finalizar_compra);

    if (finalizar_compra == 1)
    {
        int id_tnum = shmget(KEY_SHM_TKTNUM, sizeof(int), IPC_CREAT | 0777);
        int *tnum = (int *)shmat(id_tnum, 0, 0);

        int req = Ingresa_semaforo(KEY_SEM_TKT_REQ);
        int done = Ingresa_semaforo(KEY_SEM_TKT_DONE);
        sem_signal(req, 0);
        sem_wait(done, 0);

        if (*tnum > 0)
            printf("Compra finalizada. Ticket #%d generado.\n", *tnum);
        else
            printf("El carrito está vacío, no se generó ticket.\n");

        shmdt(tnum);
    }
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

}

void agregarProducto()
{

    int shmItemID = shmget(KEY_SHM_NEWITEM, sizeof(InventoryItem), 0777 | IPC_CREAT);
    if (shmItemID < 0)
    {
        perror("Error en shmget - agregar producto (C): ");
        return;
    }
    InventoryItem *nuevo_item = (InventoryItem *)shmat(shmItemID, NULL, 0);
    if (nuevo_item == (InventoryItem *)-1)
    {
        perror("Error en shmat - agregarProducto (C): ");
        return;
    }

    printf("Ingrese el nombre del producto: ");
    scanf(" %49[^\n]", nuevo_item->item_name);
    replaceSpacesWithUnderscores(nuevo_item->item_name);
    printf("Ingrese el precio del producto: ");
    scanf("%f", &nuevo_item->price);
    printf("Ingrese la descripción del producto (máx 100 caracteres): ");
    scanf(" %99[^\n]", nuevo_item->description);
    replaceSpacesWithUnderscores(nuevo_item->description);
    printf("Ingrese la cantidad de producto que tiene a su disposición: ");
    scanf("%d", &nuevo_item->units);
    if (nuevo_item->units < 0)
        nuevo_item->units = 0;

    int req = Ingresa_semaforo(KEY_SEM_ADD_REQ);
    int done = Ingresa_semaforo(KEY_SEM_ADD_DONE);
    sem_signal(req, 0);
    sem_wait(done, 0);
    printf("Producto enviado al servidor para su registro.\n");

    shmdt(nuevo_item);
}

void menuVendedor()
{
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
                agregarProducto();
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

void iniciar_sesion(WINDOW *menu_win)
{
    (void)menu_win;
    char contrasena_hasheada[SHA256_DIGEST_LENGTH * 2 + 1];
    int inicioCorrecto = 0;
    FIELD *field[3];
    FORM *my_form;
    int ch, rows, cols;

    field[0] = new_field(1, 20, 6, 14, 0, 0);
    field[1] = new_field(1, 20, 8, 14, 0, 0);
    field[2] = NULL;

    set_field_back(field[0], A_UNDERLINE);
    field_opts_off(field[0], O_AUTOSKIP);
    set_field_back(field[1], A_UNDERLINE);
    field_opts_off(field[1], O_AUTOSKIP);

    my_form = new_form(field);
    scale_form(my_form, &rows, &cols);

    WINDOW *win = newwin(rows + 8, cols + 20, 2, 4);
    keypad(win, TRUE);
    set_form_win(my_form, win);
    set_form_sub(my_form, derwin(win, rows, cols, 2, 2));

    box(win, 0, 0);
    print_in_middle(win, 1, 0, cols + 10, "Iniciar Sesion", COLOR_PAIR(1));
    post_form(my_form);

    mvwprintw(win, 8, 1, "Usuario:");
    mvwprintw(win, 10, 1, "Contrasena:");
    mvwprintw(win, rows + 5, 1, "Enter: siguiente campo  |  Enter en Clave: entrar");
    mvwprintw(win, rows + 6, 1, "Flechas: moverse  |  F1: cancelar");

    curs_set(1);
    set_current_field(my_form, field[0]);
    pos_form_cursor(my_form);
    wrefresh(win);

    char pass[64] = {'\0'};
    int posPass = 0;

    while ((ch = wgetch(win)) != KEY_F(1) && !inicioCorrecto)
    {
        switch (ch)
        {
        case KEY_DOWN:
        case '\t':
            form_driver(my_form, REQ_NEXT_FIELD);
            form_driver(my_form, REQ_END_LINE);
            break;
        case KEY_UP:
            form_driver(my_form, REQ_PREV_FIELD);
            form_driver(my_form, REQ_END_LINE);
            break;
        case KEY_BACKSPACE:
        case 127:
        case 8:
            if (current_field(my_form) == field[1])
            {
                if (posPass > 0)
                {
                    posPass--;
                    pass[posPass] = '\0';
                }
            }
            form_driver(my_form, REQ_DEL_PREV);
            break;
        case 10:
            if (current_field(my_form) != field[1])
            {
                form_driver(my_form, REQ_NEXT_FIELD);
                form_driver(my_form, REQ_END_LINE);
                break;
            }
            form_driver(my_form, REQ_VALIDATION);
            {
                Usuario usuarios[MAX_USUARIOS];
                int cantidad_usuarios = 0;
                cargar_usuarios(usuarios, &cantidad_usuarios);
                char userbuf[MAX_USERNAME_LENGTH];
                strncpy(userbuf, field_buffer(field[0], 0), sizeof(userbuf) - 1);
                userbuf[sizeof(userbuf) - 1] = '\0';
                quitarEspacios(userbuf);
                Usuario validado = encontrar_usuario(usuarios, cantidad_usuarios, userbuf);
                if (!strcmp(validado.tipo, "9"))
                {
                    mvwprintw(win, 3, 1, "Usuario no encontrado.   ");
                }
                else
                {
                    hashear_contrasena(pass, contrasena_hasheada);
                    if (strcmp(validado.password, contrasena_hasheada) == 0)
                    {
                        inicioCorrecto = 1;
                        curs_set(0);
                        unpost_form(my_form);
                        free_form(my_form);
                        free_field(field[0]);
                        free_field(field[1]);
                        delwin(win);
                        clear();
                        refresh();
                        if (!strcmp(validado.tipo, "1"))
                            menuCliente();
                        else
                            menuVendedor();
                        return;
                    }
                    else
                    {
                        mvwprintw(win, 3, 1, "Contrasena invalida.     ");
                        set_field_buffer(field[1], 0, "");
                        memset(pass, 0, sizeof(pass));
                        posPass = 0;
                        set_current_field(my_form, field[1]);
                    }
                }
            }
            break;
        default:
            if (current_field(my_form) == field[1])
            {
                if (posPass < (int)sizeof(pass) - 1)
                {
                    pass[posPass++] = (char)ch;
                    form_driver(my_form, '*');
                }
            }
            else
            {
                form_driver(my_form, ch);
            }
            break;
        }
        pos_form_cursor(my_form);
        wrefresh(win);
    }

    curs_set(0);
    unpost_form(my_form);
    free_form(my_form);
    free_field(field[0]);
    free_field(field[1]);
    delwin(win);
    clear();
    refresh();
}

void registrar_usuario(WINDOW *menu_win)
{
    (void)menu_win;
    int mutex = Ingresa_semaforo(KEY_SEM_MUTEX);
    int id_usuario = shmget(KEY_SHM_USUARIO, sizeof(Usuario), IPC_CREAT | 0777);
    Usuario *usuario = (Usuario *)shmat(id_usuario, 0, 0);
    char contrasena_hasheada[SHA256_DIGEST_LENGTH * 2 + 1];
    int registrado = 0;
    FIELD *field[4];
    FORM *my_form;
    int ch, rows, cols;

    field[0] = new_field(1, 20, 6, 14, 0, 0);
    field[1] = new_field(1, 20, 8, 14, 0, 0);
    field[2] = new_field(1, 10, 10, 14, 0, 0);
    field[3] = NULL;

    set_field_back(field[0], A_UNDERLINE);
    field_opts_off(field[0], O_AUTOSKIP);
    set_field_back(field[1], A_UNDERLINE);
    field_opts_off(field[1], O_AUTOSKIP);
    set_field_back(field[2], A_UNDERLINE);
    field_opts_off(field[2], O_AUTOSKIP);

    my_form = new_form(field);
    scale_form(my_form, &rows, &cols);

    WINDOW *win = newwin(rows + 8, cols + 20, 2, 4);
    keypad(win, TRUE);
    set_form_win(my_form, win);
    set_form_sub(my_form, derwin(win, rows, cols, 2, 2));

    box(win, 0, 0);
    print_in_middle(win, 1, 0, cols + 10, "Registrar Usuario", COLOR_PAIR(1));
    post_form(my_form);

    mvwprintw(win, 8, 1, "Usuario:");
    mvwprintw(win, 10, 1, "Contrasena:");
    mvwprintw(win, 12, 1, "Tipo:");
    mvwprintw(win, 13, 1, "(1 = cliente, 2 = vendedor)");
    mvwprintw(win, rows + 5, 1, "Enter: siguiente campo  |  Enter en Tipo: registrar");
    mvwprintw(win, rows + 6, 1, "Flechas: moverse  |  F1: cancelar");

    curs_set(1);
    set_current_field(my_form, field[0]);
    pos_form_cursor(my_form);
    wrefresh(win);

    char pass[64] = {'\0'};
    int posPass = 0;

    while ((ch = wgetch(win)) != KEY_F(1) && !registrado)
    {
        switch (ch)
        {
        case KEY_DOWN:
        case '\t':
            form_driver(my_form, REQ_NEXT_FIELD);
            form_driver(my_form, REQ_END_LINE);
            break;
        case KEY_UP:
            form_driver(my_form, REQ_PREV_FIELD);
            form_driver(my_form, REQ_END_LINE);
            break;
        case KEY_BACKSPACE:
        case 127:
        case 8:
            if (current_field(my_form) == field[1])
            {
                if (posPass > 0)
                {
                    posPass--;
                    pass[posPass] = '\0';
                }
            }
            form_driver(my_form, REQ_DEL_PREV);
            break;
        case 10:
            if (current_field(my_form) != field[2])
            {
                form_driver(my_form, REQ_NEXT_FIELD);
                form_driver(my_form, REQ_END_LINE);
                break;
            }
            form_driver(my_form, REQ_VALIDATION);
            {
                char userbuf[MAX_USERNAME_LENGTH];
                char tipobuf[8];
                strncpy(userbuf, field_buffer(field[0], 0), sizeof(userbuf) - 1);
                userbuf[sizeof(userbuf) - 1] = '\0';
                quitarEspacios(userbuf);
                strncpy(tipobuf, field_buffer(field[2], 0), sizeof(tipobuf) - 1);
                tipobuf[sizeof(tipobuf) - 1] = '\0';
                quitarEspacios(tipobuf);

                if (strlen(userbuf) == 0)
                {
                    mvwprintw(win, 3, 1, "El usuario no puede estar vacio.   ");
                    set_current_field(my_form, field[0]);
                    break;
                }
                if (strcmp(tipobuf, "1") != 0 && strcmp(tipobuf, "2") != 0)
                {
                    mvwprintw(win, 3, 1, "Tipo invalido: escribe 1 o 2.      ");
                    set_field_buffer(field[2], 0, "");
                    set_current_field(my_form, field[2]);
                    break;
                }

                strcpy(usuario->user, userbuf);
                hashear_contrasena(pass, contrasena_hasheada);
                strcpy(usuario->password, contrasena_hasheada);
                strcpy(usuario->tipo, tipobuf);
                up(mutex);

                registrado = 1;
                curs_set(0);
                unpost_form(my_form);
                free_form(my_form);
                free_field(field[0]);
                free_field(field[1]);
                free_field(field[2]);
                delwin(win);
                clear();
                refresh();
                if (!strcmp(usuario->tipo, "1"))
                    menuCliente();
                else
                    menuVendedor();
                return;
            }
            break;
        default:
            if (current_field(my_form) == field[1])
            {
                if (posPass < (int)sizeof(pass) - 1)
                {
                    pass[posPass++] = (char)ch;
                    form_driver(my_form, '*');
                }
            }
            else
            {
                form_driver(my_form, ch);
            }
            break;
        }
        pos_form_cursor(my_form);
        wrefresh(win);
    }

    curs_set(0);
    unpost_form(my_form);
    free_form(my_form);
    free_field(field[0]);
    free_field(field[1]);
    free_field(field[2]);
    delwin(win);
    clear();
    refresh();
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

    int clientes = Ingresa_semaforo(KEY_SEM_CLIENTES);

    int id_usuario = shmget(KEY_SHM_USUARIO, sizeof(Usuario), IPC_CREAT | 0777);
    Usuario *usuario = (Usuario *)shmat(id_usuario, 0, 0);

    int id_carrito = shmget(KEY_SHM_CARRITO, sizeof(Carrito), IPC_CREAT | 0777);
    Carrito *carrito = (Carrito *)shmat(id_carrito, 0, 0);

    up(clientes);

    ITEM **my_items;
    int c;
    MENU *my_menu;
    WINDOW *my_menu_win;
    int n_choices, i;

    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_CYAN, COLOR_BLACK);

    n_choices = ARRAY_SIZE(choices);
    my_items = (ITEM **)calloc(n_choices, sizeof(ITEM *));
    for (i = 0; i < n_choices; ++i)
    {
        my_items[i] = new_item(choices[i], choices[i]);
    }
    set_item_userptr(my_items[0], registrar_usuario);
    set_item_userptr(my_items[1], iniciar_sesion);

    my_menu = new_menu((ITEM **)my_items);

    my_menu_win = newwin(10, 40, 4, 4);
    keypad(my_menu_win, TRUE);
    menu_opts_off(my_menu, O_SHOWDESC);

    set_menu_win(my_menu, my_menu_win);
    set_menu_sub(my_menu, derwin(my_menu_win, 6, 38, 3, 1));
    set_menu_format(my_menu, 5, 1);

    set_menu_mark(my_menu, " * ");

    box(my_menu_win, 0, 0);
    print_in_middle(my_menu_win, 1, 0, 40, "Inicio ESCOMERCE", COLOR_PAIR(1));
    mvwaddch(my_menu_win, 2, 0, ACS_LTEE);
    mvwhline(my_menu_win, 2, 1, ACS_HLINE, 38);
    mvwaddch(my_menu_win, 2, 39, ACS_RTEE);

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
        case 10:
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

    unpost_menu(my_menu);
    free_menu(my_menu);
    for (i = 0; i < n_choices; ++i)
        free_item(my_items[i]);
    endwin();

    shmdt(usuario);
    shmdt(carrito);
    return 0;
}
