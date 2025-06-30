# stdio_lib

Basic implementation of the stdio library. It works for Linux and Windows and provides
several functions:

SO_FILE *so_fopen(const char *pathname, const char *mode)

int so_fclose(SO_FILE *stream)

int so_fflush(SO_FILE *stream)

int so_fgetc(SO_FILE *stream)

int so_fputc(int c, SO_FILE *stream)

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)

int so_fseek(SO_FILE *stream, long offset, int whence)

SO_FILE *so_popen(const char *command, const char *type)

int so_pclose(SO_FILE *stream)

# Building the library

`make` => generates library

`make clean` => deletes library and object files

