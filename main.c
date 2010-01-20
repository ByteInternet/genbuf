#include "defines.h"

#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "log.h"
#include "types.h"
#include "input.h"
#include "output.h"
#include "setup.h"
#include "options.h"

#include "buffer.h"
#include "reader.h"
#include "logger.h"

/* pthread identifier variables are global for the signal handler to be work */
static pthread_t logthread;
static pthread_t readthread;

static void signal_handler (int signal)
{
	/* Determine action depending on signal */
	switch (signal)
	{
		/* Nice shutdown requested */
		case SIGHUP:
			fprintf(stderr, "I got SIGHUP signal: %d\n", signal);
      break;
		case SIGTERM:
			fprintf(stderr, "I got shutdown signal: %d\n", signal);
			pthread_cancel(readthread);
			break;
   /* Broken pipe, parent process died?*/
		case SIGPIPE:
			//fprintf(stderr, "I got SIGPIPE  signal: %d\n", signal);
			//pthread_cancel(readthread);
      //exit(1);
      sleep(1);
			break;

		/* Ignore other signals */
		default:
			fprintf(stderr, "I ignored signal: %d\n", signal);
			return;
	}
}

static void run (struct reader *rd, struct logger *ld)
{
	/* Ignore signals for rest of threads */
	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	Log(error, "Starting threads!\n");

	/* Create the worker threads */
	SysFatal(pthread_create(&logthread,  NULL, (void*) ld->run, ld), errno, "On logger thread start");
	SysFatal(pthread_create(&readthread, NULL, (void*) rd->run, rd), errno, "On reader thread start");
	
	/* Register signal handlers for nice shutdown */
	signal(SIGHUP, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGPIPE, signal_handler);

	Log(error, "Waiting for readthread to terminate!\n");

	/* Wait for the reader to finish and cleanup */
	SysFatal(pthread_join(readthread, NULL), errno, "While waiting for reader thread to finish");
	rd->cleanup(rd);

	Log(error, "Waiting for logthread to terminate!\n");

	/* Wait for the logger to finish and cleanup */
	SysFatal(pthread_join(logthread, NULL), errno, "While waiting for logger thread to finish");
	ld->cleanup(ld);

	Log(error, "All threads terminated!\n");
}

static int parse_type (char *type)
{
	if (strcasecmp(type, "file") == 0)
		return type_file;
	else if (strcasecmp(type, "udp") == 0)
		return type_udp;
	else if (strcasecmp(type, "tcp") == 0)
		return type_tcp;
	else if (strcasecmp(type, "unix") == 0)
		return type_unix;
	else
		return type_unknown;
}

static void check_for_old_pidfile (char *filename)
{
	FILE *pf = fopen (filename, "r");
	if (pf == NULL)
	{
		/* No existing pidfile */
		return;
	}

	char line[10];
	int len = fread(line, 1, 10, pf);
	if (len == 0)
	{
		perror("Failed to read from pidfile");
		exit(EXIT_FAILURE);
	}

	line[9] = '\0';
	int pid = strtol(line, NULL, 10);
	if (pid > 0)
	{
		if (kill(pid, 0) == -1 && errno == ESRCH)
		{
			fprintf(stderr, "Pidfile(%s) exits, but program(%d) doesn't anymore! Continue...\n", filename, pid);
			return;
		}
		
		fprintf(stderr, "Pidfile(%s) exits, program(%d) still running! Exiting...\n", filename, pid);
		exit(EXIT_FAILURE);
	}
	
	fprintf(stderr, "Pidfile(%s) exits but I couldn't verify PID! Exiting...\n", filename);
	exit(EXIT_FAILURE);
}

int main (int argc, char **argv)
{
	struct buffer *buffer;
	struct reader *rd;
	struct logger *ld;
	struct output_handler *outhandler;
	struct input_handler *inhandler;
	enum io_types out_type = type_file;
	enum io_types in_type = type_file;
	char *out_res = NULL;
	char *in_res = NULL;
	char *pidfile = NULL;
	int c, retval;

	static struct option long_options[] =
	{
		{"verbose",     no_argument,       NULL, 'v'},
		{"help",        no_argument,       NULL, 'h'},
		{"in",          required_argument, NULL, 'i'},
		{"out",         required_argument, NULL, 'o'},
		{"src",         required_argument, NULL, 's'},
		{"source",      required_argument, NULL, 's'},
		{"dst",         required_argument, NULL, 'd'},
		{"dest",        required_argument, NULL, 'd'},
		{"destination", required_argument, NULL, 'd'},
		{"backlog",     required_argument, NULL, 'b'},
		{"pidfile",     required_argument, NULL, 'p'},
		{ NULL,         0,                 NULL,  0 }
	};
	int option_index = 0;

	Log(critical, "Genbuf starting!\n");
	retval = EXIT_SUCCESS;

	buffer = buffer_init();
	rd     = reader_init(buffer);
	ld     = logger_init(buffer);
	current_log_level = impossible;

	/* While we're busy */
	while(TRUE)
	{
		/* Get option */
		c = getopt_long (argc, argv, "vhi:o:s:d:b:p:", long_options, &option_index);

		/* Detect the end of the options is reached */
		if (c == -1)
			break;

		/* Sneaky redefine of long options to their short counterparts */
		if (c == 0)
			c = long_options[option_index].val;
	
		/* Determine which option the user supplied */
		switch(c)
		{
			case 'v':
				/* Increase verbose level */
				if (current_log_level < debug)
					current_log_level++;
				break;

			case 'i':
				/* Set the input type for the next source */
				in_type = parse_type(optarg);
				if (in_type == type_unknown)
				{
					fprintf(stderr, "Unknown in-type: %s\n", optarg);
					retval = EXIT_FAILURE;
					goto clean_exit;
				}
				break;

			case 'o':
				/* Set the output type for the next destination */
				out_type = parse_type(optarg);
				if (out_type == type_unknown)
				{
					fprintf(stderr, "Unknown out-type: %s\n", optarg);
					retval = EXIT_FAILURE;
					goto clean_exit;
				}
				break;

			case 's':
				/* Add a source */
				in_res = strdup(optarg);
				if (in_res == NULL)
				{
					perror("String duplication failed");
					retval = EXIT_FAILURE;
					goto clean_exit;
				}

				inhandler = create_input_handler(in_type, in_res);

				/* Register with reader */
				rd->add_source(rd, inhandler);
				break;

			case 'd':
				/* Set the destination */
				if (out_type == type_unknown)
				{
					fprintf(stderr, "No output type specified for destination!\n");
					retval = EXIT_FAILURE;
					goto clean_exit;
				}
				
				if (out_res != NULL)
				{
					fprintf(stderr, "Destination already set!\n");
					retval = EXIT_FAILURE;
					goto clean_exit;
				}
				
				out_res = strdup(optarg);
				if (out_res == NULL)
				{
					perror("String duplication failed");
					retval = EXIT_FAILURE;
					goto clean_exit;
				}
				
				outhandler = create_output_handler(out_type, out_res);

				/* Register with logger */
				ld->set_destination(ld, outhandler);
				break;

			case 'b':
				/* Set backlog file */
				ld->backlog_file = strdup(optarg);
				if (ld->backlog_file == NULL)
				{
					perror("String duplication failed");
					retval = EXIT_FAILURE;
					goto clean_exit;
				}
				break;

			case 'p':
				/* User requested pidfile creation */
				pidfile = strdup(optarg);
				if (pidfile == NULL)
				{
					perror("String duplication failed");
					retval = EXIT_FAILURE;
					goto clean_exit;
				}

				check_for_old_pidfile(pidfile);

				FILE *pidfd = fopen(pidfile, "w");
				if (pidfd == NULL)
				{
					perror("Pidfile open/creation failed");
					retval = EXIT_FAILURE;
					goto clean_exit;
				}
				
				if (fprintf(pidfd, "%d", getpid()) < 0)
				{
					perror("Failed to write to pidfile");
					retval = EXIT_FAILURE;
					goto clean_exit;
				}
				
				if (fclose(pidfd))
				{
					perror("Pidfile close failed");
					retval = EXIT_FAILURE;
					goto clean_exit;
				}
				break;

			case 'h':
				/* Print a helpfull message */
				fprintf(
					stderr,
						"%s:\n\t"
						"-h(elp)\n"
						"\t-v(erbose)\n"
						"\t-p(idfile) <file>\n"
						"\t[-in  <type> -s((ou)rc(e))      <res>]+\n"
						"\t[-out <type> -d((e)st(ination)) <res>]\n"
						"\n"
						"\t<type>=file/udp/tcp/unix\n"
						"\t<res>=filename/host:port\n",
					argv[0]);
				retval = EXIT_FAILURE;
				goto clean_exit;
				
			case '?':
				/* getopt_long already printed an error message. */
				retval = EXIT_FAILURE;
				goto clean_exit;
				
			default:
				/* Print error */
				fprintf(stderr, "Error(%c)!\n", c);
				retval = EXIT_FAILURE;
				goto clean_exit;
		}
	}

	/* Run main program loop */
	run(rd, ld);

	/* Do some cleanups */
	free(in_res);
	free(out_res);

	/* Cleanup some last things */
	buffer_cleanup(buffer);

	/* All went well, exit */
	Log(critical, "Program finished succesfully..");

clean_exit:
	if (pidfile != NULL)
	{
		if (unlink(pidfile) == -1)
		{
			perror("Failed to remove pidfile");
		}
	}
	
	return retval;
}
