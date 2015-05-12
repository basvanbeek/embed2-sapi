/*
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2009 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Bas van Beek <bas@tobin.nl>                                  |
   +----------------------------------------------------------------------+
*/

/* $Id: $ */

#include "php_embed2.h"

#ifndef PHP_EMBED2_MAX_THREADS
#define PHP_EMBED2_MAX_THREADS 100
#endif

#ifndef PHP_EMBED2_MAX_RESOURCES
#define PHP_EMBED2_MAX_RESOURCES 400
#endif

#ifdef PHP_WIN32
	#include <io.h>
#endif
#include <fcntl.h>
#include <ext/standard/info.h>
#include <ext/standard/url.h>
#include <main/php_memory_streams.h>

/* we control char* lifetime of smart_str as we allow it to cross request boundaries */
#define SMART_STR_USE_REALLOC 1
/* we use bigger numbers than default as script output will most likely be bigger than anticipated for smart_str usage */
#define SMART_STR_PREALLOC 2048
#define SMART_STR_START_SIZE 2000
#include <ext/standard/php_smart_str.h>

#define EMBED2_STREAM_WRAPPER "embed2"
#define EMBED2_STREAM_TYPE "embed2stream"

#ifdef ZTS
	#define EMBED2_G(v) TSRMG(embed2_globals_id, zend_embed2_globals*, v)
#else
	#define EMBED2_G(v) (embed2_globals.v)
#endif

/* embed2 module globals */
ZEND_BEGIN_MODULE_GLOBALS(embed2)
	smart_str* output;
	smart_str* log;
ZEND_END_MODULE_GLOBALS(embed2)

enum _php_embed2_output_type {
	OT_STDFD, OT_FILE, OT_CALLBACK, OT_CHAR
};

struct _php_embed2_memory_stream {
	char* ptr;
	size_t len;
	size_t available;
};
typedef struct _php_embed2_memory_stream php_embed2_memory_stream;

/* true global to this module */
static struct _php_embed2_vars {
	zend_bool module_init;
	int argc;
	char** argv;
	#if defined PHP_WRITE_STDOUT && !defined(ZTS)
		int output_fd;
	#else
		FILE* output_fp;
	#endif
	FILE* log_fp;
	enum _php_embed2_output_type output_type;
	enum _php_embed2_output_type log_type;
	void (*fn_log_message)(char* message TSRMLS_DC);
	void (*userlandcallback)(INTERNAL_FUNCTION_PARAMETERS);
	unsigned int expectedthreadcount;
	unsigned int expectedresourcecount;
	char* (*fetchembeddedfile)(char* identifier, size_t* str_length TSRMLS_DC);
	zend_bool free_embeddedfile_on_close;
} php_embed2_vars = {
	0,
	0,
	NULL,
	#if defined PHP_WRITE_STDOUT && !defined(ZTS)
		-1,
	#else
		NULL,
	#endif
	NULL,
	OT_STDFD,
	OT_STDFD,
	NULL,
	NULL,
	1,
	1,
	NULL,
	0
};

const char HARDCODED_INI[] =
	"html_errors=0\n"
	"register_argc_argv=1\n"
	"implicit_flush=1\n"
	"output_buffering=0\n"
	"max_execution_time=0\n"
	"max_input_time=-1\n\0";

ZEND_DECLARE_MODULE_GLOBALS(embed2);

/**
 * EMBED2 SAPI MODULE
 * _______________________________________________________________________________________________________________
 */


static int php_embed2_ub_write(const char *str, uint str_length TSRMLS_DC) {
	char* ptr = (char*)str;
	uint remaining = str_length;
	#if defined PHP_WRITE_STDOUT && !defined ZTS
		long ret;
	#else
		size_t ret;
	#endif
	while (remaining > 0) {
		#if defined PHP_WRITE_STDOUT && !defined ZTS
			/*
			 * write(2) is a thread cancellation point...
			 * and we don't want to cancel right here...
			 * so we only use this if available in non-ZTS mode
			 */
			ret = write(php_embed2_vars.output_fd, ptr, remaining);
			if (ret <= 0) {
				php_handle_aborted_connection();
			}
		#else
			ret = fwrite(ptr, 1, MIN(remaining, 16384), php_embed2_vars.output_fp);
			if (ret == 0) {
				php_handle_aborted_connection();
			}
		#endif
		ptr += ret;
		remaining -= ret;
	}
	return str_length;
}

static int php_embed2_ub_write_smart_str(const char* str, uint str_length TSRMLS_DC) {
	smart_str* sstr = EMBED2_G(output);
	if (sstr != NULL) {
		smart_str_appendl(sstr, str, str_length);
	}
	return str_length;
}

static void php_embed2_flush(void *server_context) {
	int ret;
	#if defined PHP_WRITE_STDOUT && !defined ZTS
		ret = fsync(php_embed2_vars.output_fd);
	#else
		ret = fflush(php_embed2_vars.output_fp);
	#endif
	if (ret==EOF) {
		php_handle_aborted_connection();
	}
}

static int php_embed2_deactivate(TSRMLS_D) {
	if (php_embed2_vars.output_type == OT_STDFD || php_embed2_vars.output_type == OT_FILE) {
		php_embed2_flush(NULL);
	}
	return SUCCESS;
}

static void php_embed2_send_header(sapi_header_struct *sapi_header, void *server_context TSRMLS_DC) { }

static char* php_embed2_read_cookies(TSRMLS_D) {
	return NULL;
}

static void php_embed2_register_variables(zval *track_vars_array TSRMLS_DC) {
	php_import_environment_variables(track_vars_array TSRMLS_CC);
}

static void php_embed2_log_message(char *message) {
	#ifdef PHP_WIN32
		fprintf (php_embed2_vars.log_fp, "%s\r\n", message);
	#else
		fprintf (php_embed2_vars.log_fp, "%s\n", message);
	#endif
}

static void php_embed2_log_message_smart_str(char *message) {
	smart_str* sstr;
	TSRMLS_FETCH();
	sstr = EMBED2_G(log);
	smart_str_appendl(sstr, message, strlen(message));
	#ifdef PHP_WIN32
		smart_str_appendl(sstr, "\r\n", 2);
	#else
		smart_str_appendc(sstr, '\n');
	#endif
}

#ifdef ZTS
static void php_embed2_log_message_wrapper(char *message) {
	TSRMLS_FETCH();
	php_embed2_vars.fn_log_message(message TSRMLS_CC);
}
#endif

static php_embed2_memory_stream* memstream_open(char* buf, size_t len TSRMLS_DC) {
	php_embed2_memory_stream* ret = malloc(sizeof(php_embed2_memory_stream));
	ret->len = ret->available = len;
	ret->ptr = buf;
	return ret;
}

static size_t memstream_reader(void *handle, char *buf, size_t len TSRMLS_DC) {
	size_t servesize;
	php_embed2_memory_stream* stream = (php_embed2_memory_stream*)handle;
	if (stream->available < 1) {
		return 0;
	}
	if (len > stream->available) {
		servesize = stream->available;
	} else {
		servesize = len;
	}
	memcpy(buf, stream->ptr, servesize);
	stream->ptr += servesize;
	stream->available -= servesize;
	return servesize;
}

static void memstream_closer(void *handle TSRMLS_DC) {
	free(handle);
}
#if ZEND_MODULE_API_NO >= 20090115
	static size_t memstream_fsizer(void *handle TSRMLS_DC) {
		return ((php_embed2_memory_stream*)(handle))->len - ((php_embed2_memory_stream*)(handle))->available;
	}
#else
	static long memstream_fteller(void *handle TSRMLS_DC) {
		return (long)(((php_embed2_memory_stream*)(handle))->len - ((php_embed2_memory_stream*)(handle))->available);
	}
#endif

static void php_embed2_globals_ctor(zend_embed2_globals* embed2_globals TSRMLS_DC) {
	if (php_embed2_vars.output_type == OT_CHAR) {
		embed2_globals->output = malloc(sizeof(smart_str));
		embed2_globals->output->c = NULL;
		embed2_globals->output->a = embed2_globals->output->len = 0;
	}
	if (php_embed2_vars.log_type == OT_CHAR) {
		embed2_globals->log = malloc(sizeof(smart_str));
		embed2_globals->log->c = NULL;
		embed2_globals->log->a = embed2_globals->log->len = 0;
	}
}

static void php_embed2_globals_dtor(zend_embed2_globals* embed2_globals TSRMLS_DC) {
	if (php_embed2_vars.output_type == OT_CHAR) {
		if (embed2_globals->output != NULL) {
			free(embed2_globals->output->c);
			embed2_globals->output->c = NULL;
			embed2_globals->output->len = embed2_globals->output->a = 0;
		}
		free(embed2_globals->output);
	}
	if (php_embed2_vars.log_type == OT_CHAR) {
		if (embed2_globals->log != NULL) {
			free(embed2_globals->log->c);
			embed2_globals->log->c = NULL;
			embed2_globals->log->len = embed2_globals->log->a = 0;
		}
		free(embed2_globals->log);
	}
}


sapi_module_struct php_embed2_module = {
	EMBED2_NAME,						/* name */
	"PHP Embedded Library v2",			/* pretty name */

	NULL,								/* startup */
	php_module_shutdown_wrapper,		/* shutdown */

	NULL,								/* activate */
	php_embed2_deactivate,				/* deactivate */

	php_embed2_ub_write,				/* unbuffered write */
	php_embed2_flush,					/* flush */
	NULL,								/* get uid */
	NULL,								/* getenv */

	php_error,							/* error handler */

	NULL,								/* header handler */
	NULL,								/* send headers handler */
	php_embed2_send_header,				/* send header handler */

	NULL,								/* read POST data */
	php_embed2_read_cookies,			/* read Cookies */

	php_embed2_register_variables,		/* register server variables */
	php_embed2_log_message,				/* Log message */
	NULL,								/* Get request time */

	STANDARD_SAPI_MODULE_PROPERTIES
};


/**
 * EMBED2 MEMORY STREAM WRAPPER
 * _______________________________________________________________________________________________________________
 */

size_t phpmemstream_reader(php_stream* stream, char* buf, size_t count TSRMLS_DC) {
	return memstream_reader(stream->abstract, buf, count TSRMLS_CC);
}

int phpmemstream_closer(php_stream* stream, int close_handle TSRMLS_DC) {
	if (php_embed2_vars.free_embeddedfile_on_close == 1) {
		php_embed2_memory_stream* handle = (php_embed2_memory_stream*) stream->abstract;
		free(handle->ptr - handle->len + handle->available);
	}
	memstream_closer(stream->abstract TSRMLS_CC);
	return 0;
}

static php_stream_ops php_embed2_stream_ops = {
	NULL,
	phpmemstream_reader,
	phpmemstream_closer,
	NULL,
	EMBED2_STREAM_TYPE,
	NULL,
	NULL,
	NULL,
	NULL
};

php_stream* phpmemstream_open(php_stream_wrapper* wrapper, char* filename, char* mode, int options, char** opened_path, php_stream_context* context STREAMS_DC TSRMLS_DC) {
	size_t str_len;
	char* str;
	char* filepath;
	php_url* url;

	if (php_embed2_vars.fetchembeddedfile == NULL) {
		return NULL;
	}

	url = php_url_parse(filename);
	if (!url) {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC, "Invalid URL, must be in the form: embed2://filename");
		return NULL;
	}
	if (!url->host || url->host[0] == 0 || strcasecmp("embed2", url->scheme) != 0) {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC, "Invalid URL, must be in the form: embed2://filename");
		php_url_free(url);
		return NULL;
	}
	if (!url->path) {
		filepath = url->host;
	} else {
		filepath = (char*)malloc(2048);
		snprintf(filepath, 2048, "%s%s", url->host, url->path);
	}
	str = php_embed2_vars.fetchembeddedfile(filepath, &str_len TSRMLS_CC);
	if (str == NULL || str_len == 0) {
		php_stream_wrapper_log_error(wrapper, options TSRMLS_CC, "Unable to retrieve embedded file: %s", filepath);
		if (url->path) {
			free(filepath);
		}
		php_url_free(url);
		return NULL;
	}
	if (url->path) {
		free(filepath);
	}
	php_url_free(url);
	return php_stream_alloc(&php_embed2_stream_ops, memstream_open(str, str_len TSRMLS_CC), 0, mode);
}

static php_stream_wrapper_ops php_embed2_stream_wrapper_ops = {
	phpmemstream_open,		/* stream opener */
	NULL,					/* stream closer */
	NULL,					/* stream_stat */
	NULL,					/* url_stat */
	NULL,					/* dir_opener */
	EMBED2_STREAM_WRAPPER,	/* scheme */
	NULL,					/* unlink */
	#if PHP_MAJOR_VERSION >= 5
		NULL,				/* rename */
		NULL,				/* mkdir */
		NULL,				/* rmdir */
	#endif
};

static php_stream_wrapper php_embed2_stream_wrapper = {
		&php_embed2_stream_wrapper_ops,
		NULL,	/* abstract */
		0 		/* is_url */
};


/**
 * EMBED2 BRIDGE MODULE
 * _______________________________________________________________________________________________________________
 */

PHP_FUNCTION(php_embed2_has_callback) {
	if (php_embed2_vars.userlandcallback != NULL) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}
}

PHP_FUNCTION(php_embed2_callback) {
	if (php_embed2_vars.userlandcallback != NULL) {
		php_embed2_vars.userlandcallback(INTERNAL_FUNCTION_PARAM_PASSTHRU);
	} else {
		php_error(E_ERROR, "callback function was not defined by application");
	}
}

PHP_MINIT_FUNCTION(embed2) {
	if (php_register_url_stream_wrapper(EMBED2_STREAM_WRAPPER, &php_embed2_stream_wrapper TSRMLS_CC) == FAILURE) {
		return FAILURE;
	}
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(embed2) {
	if (php_unregister_url_stream_wrapper(EMBED2_STREAM_WRAPPER TSRMLS_CC) == FAILURE) {
		return FAILURE;
	}
	return SUCCESS;
}

PHP_RINIT_FUNCTION(embed2) {
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(embed2) {
	return SUCCESS;
}

PHP_MINFO_FUNCTION(embed2) {
	php_info_print_table_start();
	php_info_print_table_colspan_header(2, EMBED2_NAME);
	php_info_print_table_row(2, "version", EMBED2_VERSION);
	php_info_print_table_row(2, "installed functions", "php_embed2_callback, php_embed2_has_callback");
	php_info_print_table_end();
}

static zend_function_entry embed2_functions[] = {
	PHP_FE(php_embed2_has_callback, NULL)
	PHP_FE(php_embed2_callback, NULL)
	{NULL, NULL, NULL}
};

zend_module_entry embed2_module_entry  = {
	STANDARD_MODULE_HEADER,
	"PHP Embedded Library v2 Bridge",	/* pretty name */
	embed2_functions,					/* module functions list */
	PHP_MINIT(embed2),					/* module init */
	PHP_MSHUTDOWN(embed2),				/* module shutdown */
	PHP_RINIT(embed2),					/* module request init */
	PHP_RSHUTDOWN(embed2),				/* module request shutdown */
	PHP_MINFO(embed2),					/* module phpinfo table */
	EMBED2_VERSION,						/* module version */
	STANDARD_MODULE_PROPERTIES
};


/**
 * EMBED2 EXTERNAL API
 * _______________________________________________________________________________________________________________
 */

EMBED2_SAPI_API int php_embed2_set_input_callback(char* (*fetchembeddedfile)(char* identifier, size_t* str_length TSRMLS_DC), zend_bool free_on_close) {
	if (php_embed2_vars.module_init || fetchembeddedfile == NULL) {
		return FAILURE;
	}
	php_embed2_vars.fetchembeddedfile = fetchembeddedfile;
	if (free_on_close > 0) {
		php_embed2_vars.free_embeddedfile_on_close = 1;
	} else {
		php_embed2_vars.free_embeddedfile_on_close = 0;
	}
	return SUCCESS;
}

EMBED2_SAPI_API int php_embed2_set_output_stdout(void) {
	if (php_embed2_vars.module_init) {
		return FAILURE;
	}
	#if defined PHP_WRITE_STDOUT && !defined(ZTS)
		php_embed2_vars.output_fd = STDOUT_FILENO;
	#else
		php_embed2_vars.output_fp = stdout;
	#endif
	php_embed2_vars.output_type = OT_STDFD;
	php_embed2_module.ub_write  = php_embed2_ub_write;
	php_embed2_module.flush     = php_embed2_flush;
	return SUCCESS;
}

EMBED2_SAPI_API int php_embed2_set_output_file(const char* filepath) {
	if (php_embed2_vars.module_init || filepath == NULL) {
		return FAILURE;
	}
	#if defined PHP_WRITE_STDOUT && !defined(ZTS)
		/* use write */
		php_embed2_vars.output_fd = open(filepath,  O_RDWR | O_CREAT | O_APPEND | O_SYNC);
		if (php_embed2_vars.output_fd < 0) {
			return FAILURE;
		}
	#else
		/* use fwrite */
		php_embed2_vars.output_fp = fopen(filepath, "a");
		if (php_embed2_vars.output_fp == NULL) {
			return FAILURE;
		}
	#endif
	php_embed2_vars.output_type = OT_FILE;
	php_embed2_module.ub_write  = php_embed2_ub_write;
	php_embed2_module.flush     = php_embed2_flush;
	return SUCCESS;
}

EMBED2_SAPI_API int php_embed2_set_output_callback(int (*fnwrite)(const char *str, unsigned int str_length TSRMLS_DC)) {
	if (php_embed2_vars.module_init || fnwrite == NULL) {
		return FAILURE;
	}
	php_embed2_vars.output_type = OT_CALLBACK;
	php_embed2_module.ub_write  = fnwrite;
	php_embed2_module.flush     = NULL;
	return SUCCESS;
}

EMBED2_SAPI_API int php_embed2_set_output_pchar(void) {
	if (php_embed2_vars.module_init) {
		return FAILURE;
	}
	php_embed2_vars.output_type = OT_CHAR;
	php_embed2_module.ub_write  = php_embed2_ub_write_smart_str;
	php_embed2_module.flush     = NULL;
	return SUCCESS;
}


EMBED2_SAPI_API int php_embed2_set_log_stderr(void) {
	if (php_embed2_vars.module_init) {
		return FAILURE;
	}
	php_embed2_vars.log_fp = stderr;
	php_embed2_vars.log_type = OT_STDFD;
	php_embed2_module.log_message = php_embed2_log_message;
	return SUCCESS;

}
EMBED2_SAPI_API int php_embed2_set_log_file(const char* filepath) {
	if (php_embed2_vars.module_init || filepath == NULL) {
		return FAILURE;
	}
	php_embed2_vars.log_fp = fopen(filepath, "a");
	if (php_embed2_vars.log_fp == NULL) {
		return FAILURE;
	}
	php_embed2_vars.log_type = OT_FILE;
	php_embed2_module.log_message = php_embed2_log_message;
	return SUCCESS;

}

EMBED2_SAPI_API int php_embed2_set_log_callback(void (*fnwrite)(char *message TSRMLS_DC)) {
	if (php_embed2_vars.module_init || fnwrite == NULL) {
		return FAILURE;
	}
	php_embed2_vars.log_type = OT_CALLBACK;
	#ifdef ZTS
		php_embed2_module.log_message = php_embed2_log_message_wrapper;
		php_embed2_vars.fn_log_message = fnwrite;
	#else
		php_embed2_module.log_message = fnwrite;
	#endif
	return SUCCESS;
}

EMBED2_SAPI_API int php_embed2_set_log_pchar(void) {
	if (php_embed2_vars.module_init) {
		return FAILURE;
	}
	php_embed2_vars.log_type = OT_CHAR;
	php_embed2_module.log_message= php_embed2_log_message_smart_str;
	return SUCCESS;
}

EMBED2_SAPI_API int php_embed2_set_ini_path(char* ini_path) {
	if (php_embed2_vars.module_init || ini_path == NULL) {
		return FAILURE;
	}
	php_embed2_module.php_ini_path_override = malloc(strlen(ini_path) + 1);
	memcpy(php_embed2_module.php_ini_path_override, ini_path, strlen(ini_path) + 1);
	return SUCCESS;
}


EMBED2_SAPI_API int php_embed2_set_default_arguments(int argc, char** argv) {
	if (php_embed2_vars.module_init || argc < 1 || argv == NULL || *argv == NULL) {
		return FAILURE;
	}
	/* make startup options default for each thread */
	php_embed2_vars.argc = argc;
	php_embed2_vars.argv = argv;
	return SUCCESS;
}

EMBED2_SAPI_API int php_embed2_expected_threadcount(unsigned int count) {
	if (php_embed2_vars.module_init || count < 1 || count > PHP_EMBED2_MAX_THREADS) {
		return FAILURE;
	}
	php_embed2_vars.expectedthreadcount = count;
	return SUCCESS;
}

EMBED2_SAPI_API int php_embed2_expected_resourcecount(unsigned int count) {
	if (php_embed2_vars.module_init || count < 1 || count > PHP_EMBED2_MAX_RESOURCES) {
		return FAILURE;
	}
	php_embed2_vars.expectedresourcecount = count;
	return SUCCESS;
}

EMBED2_SAPI_API int php_embed2_set_userland_callback(void (*ulcb)(INTERNAL_FUNCTION_PARAMETERS)) {
	if (php_embed2_vars.module_init) {
		return FAILURE;
	}
	php_embed2_vars.userlandcallback = ulcb;
	return SUCCESS;
}

EMBED2_SAPI_API int php_embed2_init(void) {

	if (php_embed2_vars.output_type == OT_STDFD) {
		/* make sure these are set */
		#if defined PHP_WRITE_STDOUT && !defined(ZTS)
			php_embed2_vars.output_fd = STDOUT_FILENO;
		#else
			php_embed2_vars.output_fp = stdout;
		#endif
	}

	if (php_embed2_vars.log_type == OT_STDFD) {
		php_embed2_vars.log_fp = stderr;
	}

	#ifdef HAVE_SIGNAL_H
		#if defined(SIGPIPE) && defined(SIG_IGN)
			/*
				ignore SIGPIPE in standalone mode so that sockets created via fsockopen()
				don't kill PHP if the remote site closes it. In apache|apxs mode apache
				does that for us!
				thies@thieso.net 20000419
			*/
			signal(SIGPIPE, SIG_IGN);
		#endif
	#endif

	#ifdef PHP_WIN32
		_fmode = _O_BINARY;						/* sets default for file streams to binary */
		setmode(_fileno(stdin), O_BINARY);		/* set stdio mode to binary */
		setmode(_fileno(stdout), O_BINARY);		/* set stdio mode to binary */
		setmode(_fileno(stderr), O_BINARY);		/* set stdio mode to binary */
	#endif

	if (php_embed2_vars.argv) {
		php_embed2_module.executable_location = php_embed2_vars.argv[0];
	}

	#ifdef ZTS
		tsrm_startup(php_embed2_vars.expectedthreadcount, php_embed2_vars.expectedresourcecount, 0, NULL);
	#endif
	sapi_startup(&php_embed2_module);

	php_embed2_module.ini_entries = malloc(sizeof(HARDCODED_INI));
	memcpy(php_embed2_module.ini_entries, HARDCODED_INI, sizeof(HARDCODED_INI));

	if (php_module_startup(&php_embed2_module, &embed2_module_entry, 1)==FAILURE) {
		sapi_shutdown();
		#ifdef ZTS
			tsrm_shutdown();
		#endif
		return FAILURE;
	}

	#ifdef ZTS
		ts_allocate_id(
				&embed2_globals_id,
				sizeof(zend_embed2_globals),
				(ts_allocate_ctor) php_embed2_globals_ctor,
				(ts_allocate_dtor) php_embed2_globals_dtor
		);
	#else
		php_embed2_globals_ctor(&embed2_globals TSRMLS_CC);
	#endif

	php_embed2_vars.module_init = 1;
	return SUCCESS;
}

EMBED2_SAPI_API int php_embed2_req_init(TSRMLS_D) {
	return php_embed2_req_init_with_args(php_embed2_vars.argc, php_embed2_vars.argv, "embed2" TSRMLS_CC);
}

EMBED2_SAPI_API int php_embed2_req_init_with_args(int argc, char** argv, char* name TSRMLS_DC) {
	if (argc > 0) {
		SG(request_info).argc = argc;
		SG(request_info).argv = argv;
	}
	SG(options) |= SAPI_OPTION_NO_CHDIR;

	if (php_request_startup(TSRMLS_C) == FAILURE) {
		return FAILURE;
	}

	SG(headers_sent) = 1;
	SG(request_info).no_headers = 1;

	php_register_variable("PHP_SELF", name, NULL TSRMLS_CC);
	return SUCCESS;
}

EMBED2_SAPI_API int php_embed2_exec_fp(FILE* fp, char* runname TSRMLS_DC) {
	zend_file_handle zhnd;
	zhnd.type = ZEND_HANDLE_FP;
	zhnd.filename = runname;
	zhnd.opened_path = NULL;
	zhnd.free_filename = 0;
	return php_execute_script(&zhnd TSRMLS_CC);
}

EMBED2_SAPI_API int php_embed2_exec_path(char* filepath TSRMLS_DC) {
	zend_file_handle zhnd;
	zhnd.type = ZEND_HANDLE_FP;
	zhnd.filename = filepath;
	zhnd.opened_path = NULL;
	zhnd.free_filename = 0;
	if (!(zhnd.handle.fp = fopen(zhnd.filename, "rb"))) {
		return FAILURE;
	}
	return php_execute_script(&zhnd TSRMLS_CC);
}

EMBED2_SAPI_API int php_embed2_exec_zhnd(zend_file_handle* zhnd TSRMLS_DC) {
	return php_execute_script(zhnd TSRMLS_CC);
}

EMBED2_SAPI_API int php_embed2_exec_str(char* script, char* runname TSRMLS_DC) {
	zend_file_handle zhnd;
	zhnd.type = ZEND_HANDLE_STREAM;
	zhnd.filename = runname;
	zhnd.opened_path = NULL;
	zhnd.free_filename = 0;
	zhnd.handle.stream.handle  = memstream_open(script, strlen(script) TSRMLS_CC);
	zhnd.handle.stream.reader  = memstream_reader;
	zhnd.handle.stream.closer  = memstream_closer;
	#if ZEND_MODULE_API_NO >= 20090115
		zhnd.handle.stream.fsizer = memstream_fsizer;
		zhnd.handle.stream.isatty = 0;
	#else
		zhnd.handle.stream.fteller = memstream_fteller;
		zhnd.handle.stream.interactive = 0;
	#endif
	return php_execute_script(&zhnd TSRMLS_CC);
}

EMBED2_SAPI_API int php_embed2_exec_cmd(char* command, zval* return_value, char* runname TSRMLS_DC) {
	if (command == NULL) {
		return FAILURE;
	}
	if (runname == NULL) {
		return zend_eval_string(command, return_value, "embed2" TSRMLS_CC);
	} else {
		return zend_eval_string(command, return_value, runname TSRMLS_CC);
	}
}

EMBED2_SAPI_API void php_embed2_req_shutdown() {
	php_request_shutdown(NULL);
}

EMBED2_SAPI_API void php_embed2_shutdown() {
	TSRMLS_FETCH();
	php_embed2_shutdown_ex(TSRMLS_C);
}

EMBED2_SAPI_API void php_embed2_shutdown_ex(TSRMLS_D) {
	#ifndef ZTS
		php_embed2_globals_dtor(&embed2_globals TSRMLS_CC);
	#endif
	php_module_shutdown(TSRMLS_C);
	sapi_shutdown();
	#ifdef ZTS
		tsrm_shutdown();
	#endif

	if (php_embed2_module.ini_entries) {
		free(php_embed2_module.ini_entries);
		php_embed2_module.ini_entries = NULL;
	}
	if (php_embed2_module.php_ini_path_override) {
		free(php_embed2_module.php_ini_path_override);
		php_embed2_module.php_ini_path_override = NULL;
	}
	if (php_embed2_vars.output_type == OT_FILE) {
		#if defined PHP_WRITE_STDOUT && !defined(ZTS)
			close(php_embed2_vars.output_fd);
		#else
			fclose(php_embed2_vars.output_fp);
		#endif
	}
	if (php_embed2_vars.log_type == OT_FILE) {
		fclose(php_embed2_vars.log_fp);
	}
	php_embed2_vars.module_init = 0;
}

EMBED2_SAPI_API char* php_embed2_get_output(size_t* len TSRMLS_DC) {
	char* result;
	smart_str* str = EMBED2_G(output);
	if (str == NULL || str->c == NULL) {
		if (len != NULL) {
			*len = str->len;
		}
		return NULL;
	}
	smart_str_0(str);
	if (len != NULL) {
		*len = str->len;
	}
	result = str->c;
	str->c = NULL;
	str->len = str->a = 0;
	return result;
}

EMBED2_SAPI_API char* php_embed2_get_log(size_t* len TSRMLS_DC) {
	char* result;
	smart_str* str = EMBED2_G(log);
	if (str == NULL || str->c == NULL) {
		if (len != NULL) {
			*len = str->len;
		}
		return NULL;
	}
	smart_str_0(str);
	if (len != NULL) {
		*len = str->len;
	}
	result = str->c;
	str->c = NULL;
	str->len = str->a = 0;
	return result;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
