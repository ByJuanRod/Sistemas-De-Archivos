# Creación de Sistema de Archivos Simple

**Estudiante:** Juan Antonio Rodríguez Núñez (10160495)
**Asignatura:** Sistemas Operativos (ICC331)

Este proyecto es una implementación en C de un sistema de archivos simulado que opera dentro de un único archivo binario (`fs-10160495.bin`). El archivo actúa como un disco virtual, gestionando su propio espacio, inodos y bloques de datos sin requerir privilegios de administrador ni interactuar con el sistema de archivos real del sistema operativo.

## Descripción del diseño

El sistema de archivos se estructura dentro de un archivo binario de tamaño fijo (1 MB), dividido lógicamente en bloques de 512 bytes. La arquitectura interna está compuesta por cuatro regiones principales:

1. **Superbloque (512 bytes):** Es el primer bloque del archivo. Almacena la metainformación global del sistema, como un código de formato de validación, el tamaño total del disco, tamaño de los bloques, el total de inodos y bloques disponibles, así como los contadores de inodos y bloques libres actuales.
2. **Tabla de Inodos (4096 bytes):** Contiene un arreglo de estructuras `Inodo`. Cada inodo ocupa 64 bytes y almacena los metadatos de un archivo individual: nombre (hasta 31 caracteres), tamaño en bytes, número del bloque inicial donde residen sus datos, la cantidad de bloques que ocupa y una bandera (`enUso`) para saber si está libre u ocupado. Permite un máximo de 64 archivos.
3. **Bitmap de Bloques (1024 bytes):** Un mapa de bits que rastrea el estado (ocupado/libre) de todos los bloques de datos en el sistema. Cada bit representa un bloque; si el bit es `1`, el bloque está ocupado, si es `0`, está libre.
4. **Bloques de Datos (Resto del archivo):** Es el espacio restante del disco virtual destinado a almacenar el contenido real de los archivos creados por el usuario.

La asignación de espacio se realiza buscando bloques contiguos mediante la función `BuscarBloquesLibres`.

## Comandos implementados

El programa se ejecuta a través de la terminal y soporta las siguientes operaciones:

* **`format`**: 
  Inicializa el sistema de archivos. Crea (o sobrescribe) el archivo binario de 1 MB, llenándolo de ceros y configurando las estructuras iniciales (Superbloque, Tabla de Inodos vacía y Bitmap en ceros). *Nota: Este comando debe ejecutarse antes que cualquier otro.*
  *Uso:* `./sistema_archivos format`

* **`create <nombre> <contenido>`**: 
  Crea un nuevo archivo de texto en el sistema. Busca un inodo libre, verifica si hay bloques de datos contiguos suficientes para el tamaño del contenido, y si todo es correcto, escribe los datos y actualiza las estructuras.
  *Uso:* `./sistema_archivos create "archivo.txt" "Hola Mundo"`

* **`ls`**: 
  Lista todos los archivos actualmente guardados en el sistema de archivos. Muestra el nombre, el tamaño en bytes y el bloque inicial de cada archivo.
  *Uso:* `./sistema_archivos ls`

* **`cat <nombre>`**: 
  Lee y muestra por la salida estándar (terminal) el contenido de un archivo específico almacenado en el disco virtual.
  *Uso:* `./sistema_archivos cat "archivo.txt"`

* **`rm <nombre>`**: 
  Elimina un archivo del sistema. Libera el inodo correspondiente, marca sus bloques como libres (0) en el mapa de bits y sobrescribe con ceros los bloques de datos correspondientes para garantizar un borrado seguro.
  *Uso:* `./sistema_archivos rm "archivo.txt"`

## Limitaciones conocidas

Debido a la naturaleza simple y con fines académicos del sistema, existen algunas limitaciones:
1. **Asignación contigua:** El sistema requiere que los bloques de un archivo sean adyacentes. Esto significa que el disco virtual es susceptible a la **fragmentación externa**. Podría haber suficiente espacio libre total, pero si no está contiguo, la creación del archivo fallará.
2. **Límite de archivos:** La tabla de inodos tiene un tamaño fijo, por lo que el sistema solo puede almacenar un máximo estricto de 64 archivos de forma simultánea.
3. **Largo del nombre:** Los nombres de los archivos están limitados a un máximo de 31 caracteres (más el carácter nulo).
4. **Tipos de archivos:** El sistema está diseñado e implementado fundamentalmente para recibir cadenas de texto por argumentos de terminal, limitando su capacidad práctica para almacenar binarios complejos u otros formatos sin modificaciones en la entrada de datos.