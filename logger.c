
#include <logger.h>

Logger *logger_create(FILE *logout, Logger_log_level level);

void logger_destroy(Logger *);


