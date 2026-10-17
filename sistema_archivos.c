#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define ID_JUAN              "10160495"
#define NOMBRE_DISCO         "fs-" ID_JUAN ".bin"

#define TAMANO_DISCO         1048576
#define TAMANO_BLOQUE        512

#define TAMANO_SUPERBLOQUE   512
#define TAMANO_TABLA_INODOS  4096
#define TAMANO_INODO         64
#define TAMANO_BITMAP        1024

#define MAX_ARCHIVOS         (TAMANO_TABLA_INODOS / TAMANO_INODO)
#define MAX_LARGO_NOMBRE     32

#define INICIO_SUPERBLOQUE   0
#define INICIO_TABLA_INODOS  (INICIO_SUPERBLOQUE + TAMANO_SUPERBLOQUE)
#define INICIO_BITMAP        (INICIO_TABLA_INODOS + TAMANO_TABLA_INODOS)
#define INICIO_DATOS         (INICIO_BITMAP + TAMANO_BITMAP)

#define TOTAL_BLOQUES_DATOS  ((TAMANO_DISCO - INICIO_DATOS) / TAMANO_BLOQUE)
#define PRIMER_BLOQUE_DATOS  (INICIO_DATOS / TAMANO_BLOQUE)

#define FORMATO               0x4A75616E

/* 
Nombre: Superbloque
Objetivo: Almacenar informacion general del sistema de archivos.
Estructura:
   formato: Representa el formato del archivo
   tamanoDisco: Representa el tamaño total que posee el disco.
   tamanoBloque: Representa el tamaño que posee cada bloque en bytes.
   totalInodos: Representa la cantidad maxima de archivos que se pueden tener.
   totalBloques: Representa la cantidad de bloques de datos.
   inodosLibres: Representa la cantidad de inodos libres que existen en el superbloque (cantidad de archivos que se pueden crear).
   bloquesLibres: Representan los bloques de datos que no se han utilizado.
   relleno: Representa el espacio que se toma para completar los 512 bytes.
 */
typedef struct {
    uint32_t formato;
    uint32_t tamanoDisco;
    uint32_t tamanoBloque;
    uint32_t totalInodos;
    uint32_t totalBloques;
    uint32_t inodosLibres;
    uint32_t bloques_libres;       /* bloques de datos sin usar */
    char relleno[TAMANO_SUPERBLOQUE - 7 * 4];
} Superbloque;

/* 
Nombre: Inodo
Objetivo: Almacenar la información que describe un archivo.
Estructura:
   nombre: Representa el nombre del archivo.
   tamano: Representa el tamaño del contenido en bytes.
   bloqueInicial: Representa el primer bloque donde se encuentra el contenido.
   cantidadBloques: Representa la cantidad de bloques seguidos que ocupa el inodo.
   enUso: Representa el estado en el que se encuentra el inodo (1 para ocupado, 0 para libre). 
   relleno: Representa el espacio que se toma para completar los bytes.
 */
typedef struct {
    char nombre[MAX_LARGO_NOMBRE];
    uint32_t tamano;
    uint32_t bloqueInicial;
    uint32_t cantidadBloques;
    uint8_t en_uso; 
    char relleno[TAMANO_INODO - MAX_LARGO_NOMBRE - 3 * 4 - 1];
} Inodo;

static FILE *disco;
static Superbloque superbloque;
static Inodo tablaInodos[MAX_ARCHIVOS];
static uint8_t bitmap[TAMANO_BITMAP];

/*
   Nombre: EstaOcupadoBloque
   Argumentos:
      (int) numeroBloque: Representa el bloque que se va a buscar en el bitmap
   Objetivo: Determinar si en el bitmap se encuentra ocupado el bloque de datos.
   Retorno: (int)
      Retorna 1 si el bloque de datos dentro del bitmap esta ocupado.
      Retorna 0 si el bloque de datos no se encuentra ocupado.
*/
static int EstaOcupadoBloque(int numeroBloque) {
    int byte = numeroBloque / 8;
    int bit  = numeroBloque % 8;
    return (bitmap[byte] >> bit) & 1;
}

/*
   Nombre: MarcarBloqueOcupado
   Argumento: 
      (int) numeroBloque: Representa el bloque que se va a marcar como ocupado.
   Objetivo: Marcar en el bitmap un bloque como ocupado.
   Retorno: -
*/
static void MarcarBloqueOcupado(int numeroBloque) {
    bitmap[numeroBloque / 8] |= (1 << (numeroBloque % 8));
}

/*
   Nombre: MarcarBloqueLibre
   Argumentos:
      (int) numeroBloque: Representa el bloque se va a marcar como libre.
   Objetivo: Marcar en el bitmap un bloque como libre.
   Retorno: -
*/
static void MarcarBloqueLibre(int numeroBloque) {
    bitmap[numeroBloque / 8] &= ~(1 << (numeroBloque % 8));
}


/*
   Nombre: GuardarEstructuras
   Argumentos: -
   Objetivo: Guardar en el archivo binario el superbloque, la tabla de inodos y el bitmap.
   Retorno: -
*/
static void GuardarEstructuras(void) {
    fseek(disco, INICIO_SUPERBLOQUE, SEEK_SET);
    fwrite(&superbloque, sizeof(superbloque), 1, disco);

    fseek(disco, INICIO_TABLA_INODOS, SEEK_SET);
    fwrite(tablaInodos, sizeof(tablaInodos), 1, disco);

    fseek(disco, INICIO_BITMAP, SEEK_SET);
    fwrite(bitmap, sizeof(bitmap), 1, disco);

    fflush(disco);
}

/*
   Nombre: AbrirDisco
   Argumentos: -
   Objetivo:
   Retorno: (int) 
      Retorna 1 si se logró abrir el disco correctamente.
      Retorna 0 si no se logró abrir el disco.
*/
static int AbrirDisco(void) {
    disco = fopen(NOMBRE_DISCO, "r+b");
    if (disco == NULL) {
        printf("Error: no existe %s. Ejecute primero: format\n", NOMBRE_DISCO);
        return 0;
    }

    fseek(disco, INICIO_SUPERBLOQUE, SEEK_SET);
    fread(&superbloque, sizeof(superbloque), 1, disco);

    if (superbloque.formato != FORMATO) {
        printf("Error: %s no tiene un formato valido.\n", NOMBRE_DISCO);
        return 0;
    }

    fseek(disco, INICIO_TABLA_INODOS, SEEK_SET);
    fread(tablaInodos, sizeof(tablaInodos), 1, disco);

    fseek(disco, INICIO_BITMAP, SEEK_SET);
    fread(bitmap, sizeof(bitmap), 1, disco);

    return 1;
}

/*
   Nombre: BuscarArchivo
   Argumentos:
      (char*) nombre: Representa el nombre del archivo que se va a buscar.
   Objetivo: 
   Retorno: (int)
      Retorna la posición en el inodo en caso de que se encuentre un archivo.
      Retorna -1 si no se cuenta el archivo indicado.
*/
static int BuscarArchivo(const char *nombre) {
    for (int i = 0; i < MAX_ARCHIVOS; i++) {
        if (tablaInodos[i].en_uso && strcmp(tablaInodos[i].nombre, nombre) == 0)
            return i;
    }
    return -1;
}

/*
   Nombre: BuscarInodoLibre
   Argumentos: -
   Objetivo: Obtener la posición del primer inodo libre en la tabla.
   Retorno: (int)
      Retorna la posición del primer inodo libre si encuentra un espacio disponible.
      Retorna -1 si la tabla esta llena.
*/
static int BuscarInodoLibre(void) {
    for (int i = 0; i < MAX_ARCHIVOS; i++) {
        if (!tablaInodos[i].en_uso)
            return i;
    }
    return -1;
}

/*
   Nombre: BuscarBloquesLibres
   Argumentos: 
      (int) cantidad: Representa la cantidad de bloques libres que se desea obtener.
   Objetivo: Obtener la cantidad de bloques libres consecutivos.
   Retorno: (int)
      Retorna el número del primer bloque libre encontrado que cumple con la cantidad necesaria.
      Retorna -1 si no hay espacio suficiente.
*/
static int BuscarBloquesLibres(int cantidad) {
    int seguidos = 0;
    for (int i = 0; i < TOTAL_BLOQUES_DATOS; i++) {
        if (EstaOcupadoBloque(i))
            seguidos = 0;
        else
            seguidos++;

        if (seguidos == cantidad)
            return i - cantidad + 1;
    }
    return -1;
}

/*
   Nombre: ComandoFormat
   Argumentos: -
   Objetivo: Crear un archivo vacio de 1 MB e inicializar las estructuras de superbloque, tabla de inodos y el bitmap
   Retorno: -
*/
static void ComandoFormat(void) {
    disco = fopen(NOMBRE_DISCO, "w+b");
    if (disco == NULL) {
        printf("Error: no se pudo crear %s\n", NOMBRE_DISCO);
        return;
    }

    char bloqueVacio[TAMANO_BLOQUE] = {0};
    for (int i = 0; i < TAMANO_DISCO / TAMANO_BLOQUE; i++)
        fwrite(bloqueVacio, TAMANO_BLOQUE, 1, disco);

    memset(&superbloque, 0, sizeof(superbloque));
    memset(tablaInodos, 0, sizeof(tablaInodos));
    memset(bitmap, 0, sizeof(bitmap));

    superbloque.formato = FORMATO;
    superbloque.tamanoDisco = TAMANO_DISCO;
    superbloque.tamanoBloque = TAMANO_BLOQUE;
    superbloque.totalInodos = MAX_ARCHIVOS;
    superbloque.totalBloques = TOTAL_BLOQUES_DATOS;
    superbloque.inodosLibres = MAX_ARCHIVOS;
    superbloque.bloques_libres  = TOTAL_BLOQUES_DATOS;

    GuardarEstructuras();
    fclose(disco);
    printf("Sistema de archivos creado: %s (1 MB)\n", NOMBRE_DISCO);
}

/*
   Nombre: ComandoCreate
   Argumentos:
      (char*) nombre: Representa el nombre del archivo que se va a crear.
      (char*) contenido: Representa el contenido del archivo de texto.
   Objetivo: Crear un archivo de texto y realizar las validaciones y escrituras necesarias.
   Retorno: -
*/
static void ComandoCreate(const char *nombre, const char *contenido) {
    int tamanoContenido = strlen(contenido);

    if (strlen(nombre) >= MAX_LARGO_NOMBRE) {
        printf("Error: el nombre debe tener maximo %d caracteres.\n", MAX_LARGO_NOMBRE - 1);
        return;
    }
    if (BuscarArchivo(nombre) != -1) {
        printf("Error: el archivo '%s' ya existe.\n", nombre);
        return;
    }

    int posicionInodo = BuscarInodoLibre();
    if (posicionInodo == -1) {
        printf("Error: no quedan inodos libres.\n");
        return;
    }

    int bloquesNecesarios = (tamanoContenido + TAMANO_BLOQUE - 1) / TAMANO_BLOQUE;
    if (bloquesNecesarios == 0)
        bloquesNecesarios = 1;

    int primerBloque = BuscarBloquesLibres(bloquesNecesarios);
    if (primerBloque == -1) {
        printf("Error: no hay espacio suficiente en el disco.\n");
        return;
    }

    for (int i = 0; i < bloquesNecesarios; i++)
        MarcarBloqueOcupado(primerBloque + i);

    long posicion_en_disco = INICIO_DATOS + (long)primerBloque * TAMANO_BLOQUE;
    fseek(disco, posicion_en_disco, SEEK_SET);
    fwrite(contenido, 1, tamanoContenido, disco);

    Inodo *inodo = &tablaInodos[posicionInodo];
    memset(inodo, 0, sizeof(Inodo));
    strcpy(inodo->nombre, nombre);
    inodo->tamano           = tamanoContenido;
    inodo->bloqueInicial   = PRIMER_BLOQUE_DATOS + primerBloque;
    inodo->cantidadBloques = bloquesNecesarios;
    inodo->en_uso           = 1;

    superbloque.inodosLibres--;
    superbloque.bloques_libres -= bloquesNecesarios;
    GuardarEstructuras();

    printf("Archivo creado en bloque %u\n", inodo->bloqueInicial);
}

/*
   Nombre: ComandoLS
   Argumentos: -
   Objetivo: Crear una salida que liste los archivos (nombre, tamaño y el bloque).
   Retorno: -
*/
static void ComandoLS(void) {
    printf("%-32s %-14s %s\n", "Nombre", "Tamano", "Bloque");

    for (int i = 0; i < MAX_ARCHIVOS; i++) {
        if (!tablaInodos[i].en_uso)
            continue;

        char tamanoTexto[32];
        sprintf(tamanoTexto, "%u bytes", tablaInodos[i].tamano);
        printf("%-32s %-14s %u\n", tablaInodos[i].nombre, tamanoTexto,
               tablaInodos[i].bloqueInicial);
    }
}

/*
   Nombre: ComandoCat
   Argumentos:
      (char*) nombre: Representa el nombre del archivo que se va a visualizar.
   Objetivo: Mostrar en la terminal el contenidodel archivo seleccionado.
   Retorno: -
*/
static void ComandoCat(const char *nombre) {
    int posicionInodo = BuscarArchivo(nombre);
    if (posicionInodo == -1) {
        printf("Error: el archivo '%s' no existe.\n", nombre);
        return;
    }

    Inodo *inodo = &tablaInodos[posicionInodo];

    char contenido[inodo->tamano + 1];
    fseek(disco, (long)inodo->bloqueInicial * TAMANO_BLOQUE, SEEK_SET);
    fread(contenido, 1, inodo->tamano, disco);
    contenido[inodo->tamano] = '\0';

    printf("%s\n", contenido);
}

/*
   Nombre: ComandoRM
   Argumentos:
      (char*) nombre: Representa el nombre del fichero que se va a eliminar.
   Objetivo: Liberar los bloques en el bitmap y simular la eliminación.
   Retorno: -
*/
static void ComandoRM(const char *nombre) {
    int posicionInodo = BuscarArchivo(nombre);
    if (posicionInodo == -1) {
        printf("Error: el archivo '%s' no existe.\n", nombre);
        return;
    }

    Inodo *inodo = &tablaInodos[posicionInodo];
    int primerBloque = inodo->bloqueInicial - PRIMER_BLOQUE_DATOS;
    char bloqueVacio[TAMANO_BLOQUE] = {0};

    for (uint32_t i = 0; i < inodo->cantidadBloques; i++) {
        MarcarBloqueLibre(primerBloque + i);
        fseek(disco, INICIO_DATOS + (long)(primerBloque + i) * TAMANO_BLOQUE, SEEK_SET);
        fwrite(bloqueVacio, TAMANO_BLOQUE, 1, disco);
    }

    superbloque.bloques_libres += inodo->cantidadBloques;
    superbloque.inodosLibres++;
    memset(inodo, 0, sizeof(Inodo));

    GuardarEstructuras();
    printf("Archivo '%s' eliminado\n", nombre);
}

int main(int argc, char *argv[]) {
   printf("Juan Rodriguez - 10160495\n");

    if (argc < 2) {
        printf("Uso: %s format | create <nombre> <contenido> | ls | cat <nombre> | rm <nombre>\n",
               argv[0]);
        return 1;
    }

    char *comando = argv[1];

    if (strcmp(comando, "format") == 0) {
        ComandoFormat();
        return 0;
    }

    if (!AbrirDisco())
        return 1;

    if (strcmp(comando, "create") == 0 && argc >= 4)
        ComandoCreate(argv[2], argv[3]);
    else if (strcmp(comando, "ls") == 0)
        ComandoLS();
    else if (strcmp(comando, "cat") == 0 && argc >= 3)
        ComandoCat(argv[2]);
    else if (strcmp(comando, "rm") == 0 && argc >= 3)
        ComandoRM(argv[2]);
    else
        printf("Comando no reconocido o faltan argumentos.\n");

    fclose(disco);
    return 0;
}
