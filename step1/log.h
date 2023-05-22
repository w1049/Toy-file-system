#include <err.h>
#include <stdio.h>

static FILE *log_file;

static inline void log_init(const char *fname) {
    log_file = fopen(fname, "w");
    if (log_file == NULL) err(1, "fopen %s", fname);
}

static inline void log_close(void) { fclose(log_file); }

#define Log(format, ...)                                         \
    do {                                                         \
        fprintf(log_file, "[INFO] " format "\n", ##__VA_ARGS__); \
        fflush(log_file);                                        \
    } while (0)

#define Warn(format, ...)                                        \
    do {                                                         \
        fprintf(log_file, "[WARN] " format "\n", ##__VA_ARGS__); \
        fflush(log_file);                                        \
    } while (0)

#define Error(format, ...)                                        \
    do {                                                          \
        fprintf(log_file, "[ERROR] " format "\n", ##__VA_ARGS__); \
        fflush(log_file);                                         \
    } while (0)

#ifdef DEBUG
#define Debug(format, ...)                                        \
    do {                                                          \
        fprintf(log_file, "[DEBUG] " format "\n", ##__VA_ARGS__); \
        fflush(log_file);                                         \
    } while (0)
#else
#define Debug(format, ...)
#endif
