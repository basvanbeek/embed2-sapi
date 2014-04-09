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

/*
 * Example of using Embed2 SAPI with pthreads and concurrent PHP requests in C
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <sapi/embed2/php_embed2.h>

#define WORKER_THREADS 10

pthread_t mainThread;
pthread_t thread[WORKER_THREADS];

PHP_EMBED2_CALLBACK(myCallback) {
	char *a;
	int alen = 0;
	long worker = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &a, &alen, &worker) == FAILURE) {
		php_error(E_WARNING, "Application: we've encountered an error during callback");
	}

	fprintf(stdout, "Application [%ld]: PHP sent a string parameter: %s\n", worker, a);
	if (return_value_used) {
		char* buf;
		fprintf(stdout, "Application [%ld]: PHP Script wants a return value\n", worker);
		spprintf(&buf, 100, "return value [%ld] from application", worker);
		RETURN_STRING(buf, 0);
	} else {
		fprintf(stdout, "Application [%ld]: call back succeeded... nothing needs to be returned\n", worker);
	}
}

void* fn_workerthread(void* arg) {

	int worker = *(char*)arg;
	fprintf(stdout, "About to execute script [%d]\n", worker);

	TSRMLS_FETCH();
	if (php_embed2_req_init(TSRMLS_C) == FAILURE) {
		fprintf(stdout, "ERROR: unable to start request environment (TSRMLS: %p)\n" TSRMLS_CC);
	} else {

		zval* zval_worker;
		MAKE_STD_ZVAL(zval_worker);
		ZVAL_LONG(zval_worker, worker);
		ZEND_SET_SYMBOL(&EG(symbol_table), "w", zval_worker);

		if (php_embed2_exec_str(
			"<?php\n"
			"usleep(rand(0,10) * 100000);\n"
			"echo \"PHP $w: Test of callback\\n\";\n"
			"echo \"PHP $w: Callback returns: \".php_embed2_callback('param from php code ['.$w.']', $w).\"\\n\";\n"
			"echo \"PHP $w: End of callback test\\n\\n\";\n"
			"?>"
			"this string falls outside of the php script tags\n"
			"<?php\n"
			"echo 'current date: '.date('Y-m-d H:i:s').\"\n\n\";\n"
			"?>",
			"callback_run"
			TSRMLS_CC
		) == FAILURE) {
			fprintf(stdout, "ERROR: unable to execute script (TSRMLS: %p)\n" TSRMLS_CC);
		}
		if (php_embed2_exec_str(
			"<?php\n"
			"echo \"this php code is actually the second script run in the same request for worker [$w]\n\n\""
			"?>",
			"second_script_run"
			TSRMLS_CC
		) == FAILURE) {
			fprintf(stdout, "ERROR: unable to execute script (TSRMLS: %p)\n" TSRMLS_CC);
		}
		php_embed2_req_shutdown();
	}
	return NULL;
}

void* fn_mainthread(void* arg) {
	int i;
	char workerids[WORKER_THREADS];

	/* init engine settings */
	PHP_EMBED2_INIT_CALLBACK(myCallback)
	/* start engine */
	if (php_embed2_init() == FAILURE) {
		fprintf(stdout, "ERROR: unable to start embed2 engine\n");
		return NULL;
	}
	for (i=0;i<WORKER_THREADS;i++) {
		workerids[i] = i;
		if (pthread_create(&thread[i], NULL, fn_workerthread, &(workerids[i])) != 0) {
			fprintf(stdout, "ERROR: unable to start worker thread #%d\n", i);
		}
	}
	for (i=0;i<WORKER_THREADS;i++) {
		if (pthread_join(thread[i], NULL) != 0) {
			fprintf(stdout, "ERROR: unable to join worker thread #%d\n", i);
		}
	}
	/* stop engine */
	php_embed2_shutdown();
	return NULL;
}

int main(int argc, char** argv) {
	/* create main php thread */
	fprintf(stdout, "Start test application\n");
	if (pthread_create(&mainThread, NULL, fn_mainthread, NULL) != 0) {
		fprintf(stdout, "ERROR: unable to start main thread\n");
		return -1;
	}
	/* wait until main thread is done */
	pthread_join(mainThread, NULL);

	fprintf(stdout, "End test application\n");
	return 0;
}
