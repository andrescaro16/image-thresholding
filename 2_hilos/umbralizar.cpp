/* 
Solo multihilos en lugar de multiprocesos. La razón es que los hilos comparten el mismo espacio de memoria, lo que facilita el acceso y la modificación de
la matriz de píxeles. Por otro lado, los procesos tienen espacios de memoria separados, lo que significa que tendrías que implementar una forma de compartir 
la matriz de píxeles entre los procesos, lo cual puede ser complicado y menos eficiente.

Se crea un número de hilos igual al número de núcleos de la CPU. Divide la matriz en bloques de aproximadamente el mismo tamaño y asigna cada bloque a un hilo 
diferente. Cada hilo umbraliza su bloque de la matriz en paralelo con los otros hilos. Esto debería acelerar el procesamiento de la imagen, especialmente si 
la imagen es grande y la CPU tiene muchos núcleos.

Si la imagen es pequeña, es posible que no se vea una mejora significativa en el rendimiento con multihilos.

*/


#include <iostream>
#include <fstream>
#include <vector>
#include <thread>

using namespace std;

struct Pixel {
    unsigned char blue;
    unsigned char green;
    unsigned char red;
};

#pragma pack(push, 1)
struct BMPHeader {
    char signature[2];
    int fileSize;
    int reserved;
    int dataOffset;
    int headerSize;
    int width;
    int height;
    short planes;
    short bitsPerPixel;
    int compression;
    int dataSize;
    int horizontalResolution;
    int verticalResolution;
    int colors;
    int importantColors;
};
#pragma pack(pop)

vector<vector<Pixel>> leerArchivoBMP(const char* nombreArchivo) {
    ifstream archivo(nombreArchivo, ios::binary);

    if (!archivo) {
        cerr << "No se pudo abrir el archivo BMP" << endl;
        exit(1);
    }

    BMPHeader header;
    archivo.read(reinterpret_cast<char*>(&header), sizeof(BMPHeader));

    if (header.bitsPerPixel != 24) {
        cerr << "El archivo BMP debe tener 24 bits por píxel" << endl;
        exit(1);
    }

    // Mover el puntero al inicio de los datos de píxeles
    archivo.seekg(header.dataOffset, ios::beg);

    vector<vector<Pixel>> matriz(header.height, vector<Pixel>(header.width));

    for (int i = 0; i < header.height; ++i) {
        for (int j = 0; j < header.width; ++j) {
            archivo.read(reinterpret_cast<char*>(&matriz[i][j]), sizeof(Pixel));
        }
        archivo.seekg(header.width % 4, ios::cur);
    }
    archivo.close();
    return matriz;
}

void umbralizar(Pixel& pixel, unsigned char umbral) {
    unsigned char promedio = (pixel.red + pixel.green + pixel.blue) / 3;
    if (promedio < umbral) {
        pixel.red = pixel.green = pixel.blue = 0;
    } else {
        pixel.red = pixel.green = pixel.blue = 255;
    }
}

void umbralizarMatriz(vector<vector<Pixel>>& matriz, unsigned char umbral, int inicio, int fin) {
    for (int i = inicio; i < fin; ++i) {
        for (int j = 0; j < matriz[0].size(); ++j) {
            umbralizar(matriz[i][j], umbral);
        }
    }
}

void guardarMatrizEnBMP(const char* nombreArchivo, const vector<vector<Pixel>>& matriz) {
    ofstream archivo(nombreArchivo, ios::binary);

    if (!archivo) {
        cerr << "No se pudo crear el archivo BMP" << endl;
        exit(1);
    }

    BMPHeader header;
    header.signature[0] = 'B';
    header.signature[1] = 'M';
    header.fileSize = sizeof(BMPHeader) + matriz.size() * ((3 * matriz[0].size()) + (matriz[0].size() % 4)) + 2; // +2 for padding
    header.reserved = 0;
    header.dataOffset = sizeof(BMPHeader);
    header.headerSize = 40;
    header.width = matriz[0].size();
    header.height = matriz.size();
    header.planes = 1;
    header.bitsPerPixel = 24;
    header.compression = 0;
    header.dataSize = matriz.size() * ((3 * matriz[0].size()) + (matriz[0].size() % 4)) + 2; // +2 for padding
    header.horizontalResolution = 0;
    header.verticalResolution = 0;
    header.colors = 0;
    header.importantColors = 0;

    archivo.write(reinterpret_cast<char*>(&header), sizeof(BMPHeader));

    // Escribir la matriz en el archivo BMP
    for (int i = 0; i < matriz.size(); ++i) {
        for (int j = 0; j < matriz[0].size(); ++j) {
            archivo.write(reinterpret_cast<const char*>(&matriz[i][j]), sizeof(Pixel));
        }
        // Rellenar con bytes de 0 para la alineación de 4 bytes
        for (int k = 0; k < matriz[0].size() % 4; ++k) {
            char paddingByte = 0;
            archivo.write(&paddingByte, 1);
        }
    }
    archivo.close();
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        cerr << "Uso: " << argv[0] << " <nombre_del_archivo_entrada.bmp> <nombre_del_archivo_salida.bmp> <umbral>" << endl;
        return 1;
    }
    const char* nombreArchivoLecturaBMP = argv[1];
    const char* nombreArchivoEscrituraBMP = argv[2];
    unsigned char umbral = static_cast<unsigned char>(stoi(argv[3]));

    // Leer el archivo BMP y obtener la matriz de píxeles
    vector<vector<Pixel>> matriz = leerArchivoBMP(nombreArchivoLecturaBMP);

    // Umbralizar la matriz utilizando multihilos
    int numHilos = thread::hardware_concurrency();
    vector<thread> hilos(numHilos);
    int tamanoBloque = matriz.size() / numHilos;
    for (int i = 0; i < numHilos; ++i) {
        int inicio = i * tamanoBloque;
        int fin = (i == numHilos - 1) ? matriz.size() : inicio + tamanoBloque;
        hilos[i] = thread(umbralizarMatriz, ref(matriz), umbral, inicio, fin);
    }
    for (auto& hilo : hilos) {
        hilo.join();
    }

    // Guardar la matriz en un nuevo archivo BMP
    guardarMatrizEnBMP(nombreArchivoEscrituraBMP, matriz);

    return 0;
}
