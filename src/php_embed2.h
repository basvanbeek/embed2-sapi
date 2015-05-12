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

#ifndef _PHP_EMBED2_H_
#define _PHP_EMBED2_H_

#include <main/php.h>
#include <main/SAPI.h>
#include <main/php_main.h>
#include <main/php_variables.h>
#include <main/php_ini.h>
#include <zend_ini.h>

#define EMBED2_NAME    "Embed2 PHP SAPI Library"
#define EMBED2_VERSION "1.1"

#ifndef PHP_WIN32
	#define EMBED2_SAPI_API SAPI_API
#else
	#define EMBED2_SAPI_API
#endif

#ifndef PHP_WIN32
	#define EMBED2_SAPI_API SAPI_API
#else
	#define EMBED2_SAPI_API
#endif

/**
 * EASY WRAPPER FUNCTIONS
 */

#define PHP_EMBED2_REQ_START() \
	php_embed2_req_init(TSRMLS_C); \
	zend_first_try

#define PHP_EMBED2_REQ_START_EX(argc,argv,name) \
	php_embed2_req_init_with_args(argc, argv, name TSRMLS_CC); \
	zend_first_try

#define PHP_EMBED2_REQ_END() \
	zend_end_try(); \
	php_embed2_req_shutdown();


#define PHP_EMBED2_SAFE_REQUEST_WRAPPER(code) \
	php_embed2_req_init(TSRMLS_CC); \
	zend_first_try \
		code; \
	zend_end_try(); \
	php_embed2_req_shutdown();

#define PHP_EMBED2_SAFE_REQUEST_WRAPPER_EX(argc,argv,name,func) \
	php_embed2_req_init_with_args(argc, argv, name TSRMLS_CC); \
	zend_first_try \
		func; \
	zend_end_try(); \
	php_embed2_req_shutdown;

#define PHP_EMBED2_CALLBACK(name) void name(INTERNAL_FUNCTION_PARAMETERS)
#define PHP_EMBED2_INIT_CALLBACK(name) php_embed2_set_userland_callback(name);


/**
 * PHP EMBED2 API FUNCTIONS
 */

BEGIN_EXTERN_C()
/* pre startup initialization */

EMBED2_SAPI_API int php_embed2_set_input_callback(char* (*fetchembeddedfile)(char* identifier, size_t* str_length TSRMLS_DC), zend_bool free_pchar_on_close);

/* functions to set output mode */
EMBED2_SAPI_API int php_embed2_set_output_stdout(void);
EMBED2_SAPI_API int php_embed2_set_output_file(const char* filepath);
EMBED2_SAPI_API int php_embed2_set_output_callback(int (*fnwrite)(const char *str, unsigned int str_length TSRMLS_DC));
/* use in combination with php_embed2_get_output */
EMBED2_SAPI_API int php_embed2_set_output_pchar(void);

EMBED2_SAPI_API int php_embed2_set_log_stderr(void);
EMBED2_SAPI_API int php_embed2_set_log_file(const char* filepath);
EMBED2_SAPI_API int php_embed2_set_log_callback(void (*fnwrite)(char *message TSRMLS_DC));
/* use in combination with php_embed2_get_log */
EMBED2_SAPI_API int php_embed2_set_log_pchar(void);

EMBED2_SAPI_API int php_embed2_set_ini_path(char* ini_path);
/* set default arguments in case one uses php_embed2_reg_init over php_embed2_reg_init_with_args */
EMBED2_SAPI_API int php_embed2_set_default_arguments(int argc, char** argv);
EMBED2_SAPI_API int php_embed2_expected_threadcount(unsigned int count);
EMBED2_SAPI_API int php_embed2_expected_resourcecount(unsigned int count);

/*
 *  simple application callback for php userland code...
 *  for more advanced interaction it's better to write your own extension module
 */
EMBED2_SAPI_API int php_embed2_set_userland_callback(void (*ulcb)(INTERNAL_FUNCTION_PARAMETERS));

/* startup PHP environment */
EMBED2_SAPI_API int php_embed2_init();

/* start request environment using argc/argv from php_embed2_set_default_arguments if provided */
EMBED2_SAPI_API int php_embed2_req_init(TSRMLS_D);
/* start request environment with it's own argc/argv variables */
EMBED2_SAPI_API int php_embed2_req_init_with_args(int argc, char** argv, char* name TSRMLS_DC);

/* php code execution functions */
EMBED2_SAPI_API int php_embed2_exec_fp  (FILE* fp, char* runname TSRMLS_DC);
EMBED2_SAPI_API int php_embed2_exec_path(char* filepath TSRMLS_DC);
EMBED2_SAPI_API int php_embed2_exec_zhnd(zend_file_handle* script TSRMLS_DC);
EMBED2_SAPI_API int php_embed2_exec_str (char* script, char* runname TSRMLS_DC);

/* wrapper around zend_eval_string call */
EMBED2_SAPI_API int php_embed2_exec_cmd (char* command, zval* return_value, char* runname TSRMLS_DC);

/* shutdown request environment */
EMBED2_SAPI_API void php_embed2_req_shutdown();

/* shutdown PHP environment */
EMBED2_SAPI_API void php_embed2_shutdown(void);
EMBED2_SAPI_API void php_embed2_shutdown_ex(TSRMLS_D);

/* functions used in output_smart_str modes... these functions can cross the php request environment */
EMBED2_SAPI_API char* php_embed2_get_output(size_t* len TSRMLS_DC);
EMBED2_SAPI_API char* php_embed2_get_log(size_t* len TSRMLS_DC);


/* forward declarations of the embed2 modules */
extern EMBED_SAPI_API sapi_module_struct php_embed2_module;
extern  zend_module_entry  embed2_module_entry;
#define phpext_embed2_ptr  &embed2_module_entry

END_EXTERN_C()

#endif /* _PHP_EMBED2_H_ */
