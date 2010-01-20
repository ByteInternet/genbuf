#ifndef GENCACHE_LOG_H
#define GENCACHE_LOG_H

/* Different log levels */
enum log_levels {
	impossible = 0,
	critical   = 1,
	error      = 2,
	warning    = 3,
	info       = 4,
	debug      = 5
};

extern enum log_levels current_log_level;

/* New style logging */
extern void CustomLog (const char *file, const int line, const enum log_levels level,  const char *format, ...);
extern void CustomSystemError (const char *file, const int line, const enum log_levels level, const int fatal, const int err, const char *format, ...);

/* Old style logging */
extern void      Log (enum log_levels level, const char *msg);
extern void     Log2 (enum log_levels level, const char *err, const char *msg);
extern void  Require (int cond);
extern void    Fatal (int cond, const char *err, const char *msg);
extern void SysFatal (int cond, int err, const char *msg);
extern void  NotNull (const void *pointer);
extern void   SysErr (int err, const char *msg);

#endif /* GENCACHE_LOG_H */
