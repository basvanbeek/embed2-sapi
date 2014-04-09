
set BOOST_LIB_PATH="C:\sources\boost_1_38_0\lib"
set BOOST_INCLUDE_PATH="C:\sources\boost_1_38_0"

set PHP_LIB_PATH="C:\PHPDEV\php-5.2.9\Release_TS"
set PHP_INCLUDE_PATH="C:\PHPDEV\php-5.2.9"


CL /Od /I %BOOST_INCLUDE_PATH% /I "%PHP_INCLUDE_PATH%\main" /I "%PHP_INCLUDE_PATH%\Zend" /I "%PHP_INCLUDE_PATH%\TSRM" /I %PHP_INCLUDE_PATH% /D "WIN32" /D "PHP_WIN32" /D "ZEND_WIN32" /D "_CONSOLE" /D "_USE_32BIT_TIME_T" /D "_WIN32_WINNT=0x0501" /D "_SCL_SECURE_NO_WARNINGS" /D "ZTS" /Gm /EHsc /RTC1 /MDd /W3 /nologo /c /ZI /TP "..\embed2_cpp_test.cpp"


CL embed2_cpp_test.obj /link /OUT:"embed2_threading_test.exe" /INCREMENTAL /NOLOGO /LIBPATH:"%BOOST_LIB_PATH%" /LIBPATH:"%PHP_LIB_PATH%" /SUBSYSTEM:CONSOLE /DYNAMICBASE /NXCOMPAT /MACHINE:X86 php5embed2.lib  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib