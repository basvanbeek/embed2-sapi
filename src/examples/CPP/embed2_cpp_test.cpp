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
 * Example of using Embed2 SAPI with boost::thread library and concurrent PHP requests in C++
 *
 */

#ifdef WIN_32
#include "stdafx.h"
#endif
#include <boost/thread.hpp>
#include <sapi/embed2/php_embed2.h>

#define WORKER_THREADS 10

PHP_EMBED2_CALLBACK(myCallback) {
	char *a;
	int alen = 0;
	long worker = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &a, &alen, &worker) == FAILURE) {
		php_error(E_WARNING, "Application: we've encountered an error during callback");
	}

	std::cout << "Application [" << worker << "]: PHP sent a string parameter: " << a << std::endl;
	if (return_value_used) {
		char* buf;
		std::cout << "Application [" << worker << "]: PHP Script wants a return value" << std::endl;
		spprintf(&buf, 100, "return value [%ld] from application", worker);
		RETURN_STRING(buf, 0);
	} else {
		std::cout << "Application [" << worker << "]: call back succeeded... nothing needs to be returned" << std::endl;
	}
}

void threadfunc(int workerid) {
	// worker thread
	TSRMLS_FETCH();

	std::cout << "About to execute script [" << workerid << "]" << std::endl;

	if (php_embed2_req_init(TSRMLS_C) == FAILURE) {
		std::cout << "ERROR: unable to start request environment (TSRMLS: " << TSRMLS_C << ")" << std::endl;
	} else {
		zval* zval_worker;

		MAKE_STD_ZVAL(zval_worker);
		ZVAL_LONG(zval_worker, workerid);
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
			std::cout << "ERROR: unable to execute script (TSRMLS: " << TSRMLS_C << ")" << std::endl;
		}
		if (php_embed2_exec_str(
			"<?php\n"
			"echo \"this php code is actually the second script run in the same request for worker [$w]\n\n\""
			"?>",
			"second_script_run"
			TSRMLS_CC
		) == FAILURE) {
			std::cout << "ERROR: unable to execute script (TSRMLS: " << TSRMLS_C << ")" << std::endl;
		}
		php_embed2_req_shutdown();
	}
}

int main(int argc, char* argv[]) {
	boost::thread *threads[WORKER_THREADS];
	int i;

	std::cout << "Application start" << std::endl;

	PHP_EMBED2_INIT_CALLBACK(myCallback);

	if (php_embed2_init() == FAILURE) {
		std::cerr << "ERROR: unable to start php embed2 engine" << std::endl;
		return -1;
	}

	for (i = 0; i < WORKER_THREADS; i++) {
		threads[i] = new boost::thread(threadfunc, i);
	}
	for (i = 0; i < WORKER_THREADS; i++) {
		threads[i]->join();
	}

	php_embed2_shutdown();

	std::cout << "Application end" << std::endl;

	return 0;
}

