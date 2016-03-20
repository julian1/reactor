#pragma once

#include <stdio.h>

typedef struct Logger Logger;

// ALL < DEBUG < INFO < WARN < ERROR < FATAL < OFF.
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_FATAL,
    LOG_NONE
} Logger_log_level;

Logger *logger_create(FILE *logout, Logger_log_level level);

void logger_destroy(Logger *);

void logger_log(Logger *l, Logger_log_level level, const char *format, ...);

