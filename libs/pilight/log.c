/*
	Copyright (C) 2013 CurlyMo

	This file is part of pilight.

    pilight is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software
	Foundation, either version 3 of the License, or (at your option) any later
	version.

    pilight is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with pilight. If not, see	<http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <syslog.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>

#include "../../pilight.h"
#include "common.h"
#include "gc.h"
#include "log.h"

FILE *lf=NULL;

char *logfile = NULL;
char *logpath = NULL;

int log_gc(void) {
	if(shelllog == 1) {
		logmarkup();
		fprintf(stderr, "%sDEBUG: garbage collected log library\n", debug_log);
	}
	if(lf) {
		if(fclose(lf) != 0) {
			return 0;
		}
		else {
			lf = NULL;
		}
	}
	if(logfile) {
		sfree((void *)&logfile);
	}
	if(logpath) {
		sfree((void *)&logpath);
	}
	return 1;
}

void logprintf(int prio, const char *format_str, ...) {
	int save_errno = errno;
	va_list ap;

	if(logfile == NULL && filelog == 0 && shelllog == 0)
		return;

	if(loglevel >= prio) {
		if(lf == NULL && filelog == 1) {
			if((lf = fopen(logfile, "a+")) == NULL) {
				logprintf(LOG_WARNING, "could not open logfile %s", logfile);
			}
		}

		if(filelog == 1 && lf != NULL && loglevel < LOG_DEBUG) {
			logmarkup();
			fputs(debug_log, lf);
			va_start(ap, format_str);
			if(prio==LOG_WARNING)
				fprintf(lf,"WARNING: ");
			if(prio==LOG_ERR)
				fprintf(lf,"ERROR: ");
			if(prio==LOG_INFO)
				fprintf(lf, "INFO: ");
			if(prio==LOG_NOTICE)
				fprintf(lf, "NOTICE: ");
			vfprintf(lf, format_str, ap);
			fputc('\n',lf);
			fflush(lf);
			va_end(ap);
		}

		if(shelllog == 1 || prio == LOG_ERR) {
			logmarkup();
			fputs(debug_log, stderr);
			va_start(ap, format_str);
			if(prio==LOG_WARNING)
				fprintf(stderr, "WARNING: ");
			if(prio==LOG_ERR)
				fprintf(stderr, "ERROR: ");
			if(prio==LOG_INFO)
				fprintf(stderr, "INFO: ");
			if(prio==LOG_NOTICE)
				fprintf(stderr, "NOTICE: ");
			if(prio==LOG_DEBUG)
				fprintf(stderr, "DEBUG: ");
			vfprintf(stderr, format_str, ap);
			fputc('\n', stderr);
			fflush(stderr);
			va_end(ap);
		}
	}
	errno = save_errno;
}

void logperror(int prio, const char *s) {
	// int save_errno = errno;
	// if(logging == 0)
		// return;

	// if(s != NULL) {
		// logprintf(prio, "%s: %s", s, strerror(errno));
	// } else {
		// logprintf(prio, "%s", strerror(errno));
	// }
	// errno = save_errno;
}

void log_file_enable(void) {
	filelog = 1;
}

void log_file_disable(void) {
	filelog = 0;
}

void log_shell_enable(void) {
	shelllog = 1;
}

void log_shell_disable(void) {
	shelllog = 0;
}

void log_file_set(char *log) {
	struct stat s;
	char *filename = basename(log);
	size_t i = (strlen(log)-strlen(filename));
	logpath = realloc(logpath, i+1);
	memset(logpath, '\0', i+1);
	strncpy(logpath, log, i);
	
	if(strcmp(filename, log) != 0) {
		int err = stat(logpath, &s);
		if(err == -1) {
			if(ENOENT == errno) {
				logprintf(LOG_ERR, "the log file folder does not exist", optarg);
				sfree((void *)&logpath);
				exit(EXIT_FAILURE);
			} else {
				logprintf(LOG_ERR, "failed to run stat on log folder", optarg);
				sfree((void *)&logpath);
				exit(EXIT_FAILURE);
			}
		} else {
			if(S_ISDIR(s.st_mode)) {
				logfile = realloc(logfile, strlen(log)+1);
				strcpy(logfile, log);
			} else {
				logprintf(LOG_ERR, "the log file folder does not exist", optarg);
				sfree((void *)&logpath);
				exit(EXIT_FAILURE);
			}
		}
	} else {
		logfile = realloc(logfile, strlen(log)+1);
		strcpy(logfile, log);
	}

	if(lf == NULL && filelog == 1) {
		if((lf = fopen(logfile, "w")) == NULL) {
			logprintf(LOG_WARNING, "could not open logfile %s", logfile);
		} else {
			if(fclose(lf) == 0) {
				lf = NULL;
			}
		}
	}
	sfree((void *)&logpath);
}

void log_level_set(int level) {
	loglevel = level;
}

int log_level_get(void) {
	return loglevel;
}
