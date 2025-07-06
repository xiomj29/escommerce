# ESCOMERCE - Sistema de Comercio Electrónico en C - Proyecto EXPOESCOM 2024

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![Linux](https://img.shields.io/badge/Fedora-294172?style=for-the-badge&logo=fedora&logoColor=white)
![GitHub](https://img.shields.io/badge/GitHub-100000?style=for-the-badge&logo=github&logoColor=white)

## Descripción
Sistema cliente-servidor de comercio electrónico implementado en C que utiliza:
- Memoria compartida para IPC
- Semáforos para sincronización
- Ncurses para interfaz de terminal
- SHA-256 para hashing de contraseñas

## Instalación en Fedora
```bash
sudo dnf install gcc ncurses-devel openssl-devel git
git clone https://github.com/tu-usuario/escommerce.git
cd escommerce
```

## Compilación

Servidor:
```bash
gcc Servidor4.c -o servidor -lpthread
```

Cliente:
```bash
gcc Cliente4.c -o cliente -lform -lncurses -lcrypto
```

## Uso

Terminal 1 (Servidor):
```bash
./servidor
```

Terminal 2 (Cliente):
```bash
./cliente
```

##Estructura
```
.
├── Servidor4.c
├── Cliente4.c
├── usuarios.txt
├── inventario.txt
└── ticket_*.txt
```

## Licencia

Este proyecto está bajo la Licencia MIT - consulta el archivo [LICENSE](LICENSE) para más detalles.

