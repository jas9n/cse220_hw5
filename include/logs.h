#ifndef LOGS_H
#define LOGS_H

// for logging to a file

void log_init(const char *tag);
void log_player_init(int num);

void log_info(const char *fmt_str, ...);
void log_debug(const char *fmt_str, ...);
void log_err(const char *fmt_str, ...);

void log_fini();

#endif