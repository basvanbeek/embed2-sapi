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

EMBED2 SAPI README 

Why another embedded library?

The original embedded library is fine if your goal is to add limited PHP scripting support
to your application when the amount of PHP script runs remains small and you do not
need concurrent script runs (in multi-threaded applications). If you need to use embedded
PHP in a multi-threaded environment, need higher speeds when dealing with multiple script
runs or you wish to manipulate INI settings which fall under the category PHP_INI_SYSTEM
or PHP_INI_PERDIR you will need the EMBED2 SAPI module.


How do I run concurrent scripts in multi-threaded mode?

Typically you request the php environment to startup at program startup by invoking
php_embed2_init(). In each thread function request a thread safe resource identifier
by invoking TSRMLS_FETCH(). If you call other functions from
within the thread function try to use the TSRMLS_xx macros as much as possible.
When you need to run a script invoke the php request environment by calling 
php_embed2_req_init(), execute a script with the php_embed2_exec_* functions and when 
done with the request shut it down by using php_embed2_req_shutdown(). You are allowed
to run multiple php_embed2_exec_* functions in the same request space if you need the 
script globals to cross the function calls. On application shutdown you must have shut
down the php environment using on of the php_embed2_shutdown* functions.


I'm not using threads in my application, why should I use this library?

Efficiency! With the embed2 library you can have multiple separate requests / script runs
without the need of initializing and shutting down the entire PHP environment. The original
embedded library functions the same way as a CLI environment as for each script run the 
following happens: 
	- php environment startup
	- php modules startup
	- php request startup
	- script execution
	- php request shutdown
	- php modules shutdown
	- php environment shutdown
When running a lot of scripts during the lifetime of your application this is not optimal. 
With the embed2 library you will only have to startup the php environment and php modules 
once and for each script run only the following happens:
	- php request startup
	- script execution
	- php request shutdown


What else is different?

The embed2 library allows you to:
	- select an alternate php.ini configuration file
	- select where php output and error output should go (stdout, file, callback or char*) 
	- easy install userland callback function without needing to write an extension module
	- execute php scripts from char* using internal memory_stream handler for zend
	- use embed2:// stream combined with callback function to implement embedded/on-the-fly-generated script retrieval
	
Possible Future enhancements?
	- easy setup of ini arguments before engine start
	- per thread configuration of easy callback function (so it can be changed on the fly even within request space)
	- ...
	- your idea?
