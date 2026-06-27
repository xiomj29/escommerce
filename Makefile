# Makefile para ESCOMERCE (cliente/servidor con memoria compartida y semaforos)

CC      = gcc
CFLAGS  = -Wall -O2
DEPS    = escomerce.h

SRV_LIBS = -lpthread
CLI_LIBS = -lform -lmenu -lncurses -lcrypto

# Llaves IPC usadas por la app (ver escomerce.h). Se usan para 'ipcclean'.
IPC_KEYS = 4001 4002 4003 4004 4005 4010 4011 4012 4020 4021 4022 4030 4031 4032

.PHONY: all clean ipcclean dataclean help

all: servidor cliente

servidor: Servidor4.c $(DEPS)
	$(CC) $(CFLAGS) Servidor4.c -o servidor $(SRV_LIBS)

cliente: Cliente4.c $(DEPS)
	$(CC) $(CFLAGS) Cliente4.c -o cliente $(CLI_LIBS)

# Elimina los binarios
clean:
	rm -f servidor cliente

# Elimina los segmentos de memoria compartida y semaforos de la app.
# Util si el servidor se cerro mal y quedaron objetos IPC colgados.
ipcclean:
	@for k in $(IPC_KEYS); do \
		hk=$$(printf '0x%08x' $$k); \
		ipcrm -M $$hk 2>/dev/null || true; \
		ipcrm -S $$hk 2>/dev/null || true; \
	done
	@rm -f /tmp/server.lock
	@echo "IPC de ESCOMERCE limpiado."

# Borra los archivos de datos generados (CUIDADO: pierdes inventario/usuarios/tickets)
dataclean:
	rm -f usuarios.txt inventario.txt tickets.txt ticket_*.txt

help:
	@echo "make            - compila servidor y cliente"
	@echo "make servidor   - compila solo el servidor"
	@echo "make cliente    - compila solo el cliente"
	@echo "make clean      - borra los binarios"
	@echo "make ipcclean   - libera la memoria compartida/semaforos colgados"
	@echo "make dataclean  - borra los archivos de datos (.txt)"
	@echo ""
	@echo "Uso: en una terminal './servidor', en otra './cliente'"
