#include "logs.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>

// must be a literal
#define LOG_DIR "logs/"

#define MAX_FILE_LEN 32

static FILE *log_file = NULL;

void log_init(const char *tag)
{
    pid_t pid = getpid();
    
    char filename[32] = { 0 };
    if (tag)
        snprintf(filename, 32, LOG_DIR "%s.%d", tag, pid);
    else
        snprintf(filename, 32, LOG_DIR "logs.%d", pid);

    // get the size of the log dir 
    log_file = fopen(filename, "w");
}

void log_player_init(int num)
{
    char filename[32] = { 0 };
    snprintf(filename, 32, LOG_DIR "player%d.logs", num);

    log_file = fopen(filename, "w");
}

void log_info(const char *fmt_str, ...)
{
    if (log_file)
    {
        va_list va;
        va_start(va, fmt_str);

        fprintf(log_file, "[INFO] ");
        vfprintf(log_file, fmt_str, va);
        fprintf(log_file, "\n");

        va_end(va);

        fflush(log_file);
    }
}


void log_debug(const char *fmt_str, ...)
{
    if (log_file)
    {
        va_list va;
        va_start(va, fmt_str);

        fprintf(log_file, "[DEBUG] ");
        vfprintf(log_file, fmt_str, va);
        fprintf(log_file, "\n");

        va_end(va);

        fflush(log_file);
    }
}

void log_err(const char *fmt_str, ...)
{
    if (log_file)
    {
        va_list va;
        va_start(va, fmt_str);

        fprintf(log_file, "[ERROR] ");
        vfprintf(log_file, fmt_str, va);
        fprintf(log_file, "\n");

        va_end(va);

        fflush(log_file);
    }
}

void log_fini()
{
    if (log_file)
    {
        fclose(log_file);
    }
}