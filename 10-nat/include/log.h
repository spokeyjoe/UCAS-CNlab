#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include <errno.h>
#include <string.h>

// #define LOG_DEBUG

enum log_level { DEBUG = 0, INFO, WARNING, ERROR };

static enum log_level this_log_level = DEBUG;

static const char *log_level_str[] = { "DEBUG", "INFO", "WARNING", "ERROR" };

#ifdef LOG_DEBUG
	#define log_it(fmt, level_str, ...) \
		fprintf(stderr, "[%s:%u] %s: " fmt  "\n", __FILE__, __LINE__, \
				level_str, ##__VA_ARGS__);
#else
	#define log_it(fmt, level_str, ...) \
		fprintf(stderr, "%s: " fmt "\n", level_str, ##__VA_ARGS__);
#endif

#define log(level, fmt, ...) \
	do { \
		if (level < this_log_level) \
			break; \
		log_it(fmt, log_level_str[level], ##__VA_ARGS__); \
	} while (0)

static inline void logaddr(char *name, u32 addr) {
    log(DEBUG, "%s: %d.%d.%d.%d\n", name, addr>>24, (addr<<8)>>24, (addr<<16)>>24, (addr<<24)>>24);	
}

#endif
