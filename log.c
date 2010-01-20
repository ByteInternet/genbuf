#include "defines.h"
#include "log.h"

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#define TIMESPEC_LEN	64

enum log_levels current_log_level = impossible;

/* To shut up the compiler, we don't consider this a bug (see 'man strftime')*/
static int my_strftime(char *buf, int buflen, const char *format, const struct tm *thetime)
{
	return strftime(buf, buflen, format, thetime);
}

/* Report something */
void CustomLog (const char *file, const int line, enum log_levels level, const char *format, ...)
{
	char timespec[TIMESPEC_LEN];
	struct tm *localized;
	time_t now;
	va_list ap;

	/* Check if we shoud log this */
	if (level > current_log_level)
		return;
	
	/* Resolve the localized time */
	time(&now);
	localized = localtime(&now);
	my_strftime(timespec, TIMESPEC_LEN, "%c", localized);

	/* Lock the error log stream */
	flockfile(stderr);

	/* Print the date,program,program-location header of the log */
	fprintf(stderr, ">>> %s genbuf {%s:%d}: ", timespec, file, line);

	/* Print the given formatted string */
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);

	/* Print trailer */
	fprintf(stderr, "\n");

	/* We're done, unlock the error log stream */
	funlockfile(stderr);
}

/* Report a system error */
void CustomSystemError (const char *file, const int line, enum log_levels level, int fatal, int err, const char *format, ...)
{
	char timespec[TIMESPEC_LEN];
	struct tm *localized;
	time_t now;
	va_list ap;

	/* Check if we shoud log this */
	if (level > current_log_level)
		return;
	
	/* Resolve the localized time */
	time(&now);
	localized = localtime(&now);
	my_strftime(timespec, TIMESPEC_LEN, "%c", localized);

	/* Lock the error log stream */
	flockfile(stderr);

	/* Print the date,program,program-location header of the log */
	fprintf(stderr, ">>> %s genbuf {%s:%d}: ", timespec, file, line);

	/* Print the given formatted string */
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);

	/* Print trailer */
	fprintf(stderr, ": \"%s\"\n", strerror(err));

	/* We're done, unlock the error log stream */
	funlockfile(stderr);

	/* Terminate if necessary */
	if (fatal)
		abort();
}

void Log2 (enum log_levels level, const char *err, const char *msg)
{
	char timespec[TIMESPEC_LEN];
	struct tm *localized;
	time_t now;

	/* Check if we shoud log this */
	if (level > current_log_level)
		return;
	
	/* Resolve the localized time */
	time(&now);
	localized = localtime(&now);
	my_strftime(timespec, TIMESPEC_LEN, "%c", localized);

	/* If not specified, specify log message*/
	if (msg == NULL)
		msg = "Log";
	
	/* Print it */
	fprintf(stderr, ">>> %s genbuf: %s: %s\n", timespec, msg, err);
}

void Log (enum log_levels level, const char *msg)
{
	Log2(level, msg, NULL);
}

void Require (int cond)
{
	if (! cond)
	{
		Log2(critical, "Failed!", "Requirement");
		abort();
	}
}

void Fatal (int cond, const char *err, const char *msg)
{
	if (cond)
	{
		if (msg == NULL)
			msg = "Fatal";
		
		Log2(critical, err, msg);

		abort();
	}
}

void SysFatal (int cond, int err, const char *msg)
{
	if (cond)
	{
		if (msg == NULL)
			msg = "SysFatal";

		Log2(critical, strerror(err), msg);

		abort();
	}
}

void NotNull (const void *pointer)
{
	Fatal (pointer == NULL, "Failed!", "Not-NULL check");
}

void SysErr (int err, const char *msg)
{
	if (msg == NULL)
		msg = "System error";
	
	Log2(error, strerror(errno), msg);
}
