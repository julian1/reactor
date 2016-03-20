
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include <logger.h>

typedef struct Logger Logger;
struct Logger
{
    FILE              *logout;
    Logger_log_level  log_level;
};

Logger *logger_create(FILE *logout, Logger_log_level level)
{
    Logger *l = malloc(sizeof(Logger));
    memset(l, 0, sizeof(Logger));
    l->logout = logout;
    l->log_level = level;
    return l;
}

void logger_destroy(Logger *l)
{
    memset(l, 0, sizeof(Logger));
    free(l);
}

void logger_log(Logger *l, Logger_log_level level, const char *format, ...)
{
    if(level >= l->log_level) {
        va_list args;
        va_start (args, format);

        // fprintf(d->logout, "%s: ", getTimexceptfdstamp());
        const char *s_level = NULL;
        switch(level) {
          case LOG_DEBUG: s_level = "DEBUG"; break;
          case LOG_INFO:  s_level = "INFO"; break;
          case LOG_WARN:  s_level = "WARN"; break;
          case LOG_FATAL: s_level = "FATAL"; break;
          default: assert(0);
        };
        fprintf(l->logout, "%s: ", s_level);
        vfprintf(l->logout, format, args);
        va_end (args);
        fprintf(l->logout, "\n");
        fflush(l->logout);
    }
}

