
PHP_ARG_ENABLE(embed2,
  [whether to enable embed2 sapi library],
  [  --enable-embed2[=TYPE]   Enable building of embed2 SAPI library [TYPE=shared-zts]
                           Options: shared, static, shared-zts and static-zts (zts = thread-safe)  
  ], no, no
)

AC_MSG_CHECKING([for embedded SAPI v2 library support])

if test "$PHP_EMBED2" != "no"; then
  case "$PHP_EMBED2" in
    shared)
      PHP_EMBED2_TYPE=shared
      INSTALL_IT="\$(mkinstalldirs) \$(INSTALL_ROOT)\$(prefix)/lib; \$(INSTALL) -m 0755 $SAPI_SHARED \$(INSTALL_ROOT)\$(prefix)/lib"
      PHP_SELECT_SAPI(embed2, shared, php_embed2.c)
      ;;
    static)
      PHP_EMBED2_TYPE=static
      INSTALL_IT="\$(mkinstalldirs) \$(INSTALL_ROOT)\$(prefix)/lib; \$(INSTALL) -m 0644 $SAPI_STATIC \$(INSTALL_ROOT)\$(prefix)/lib"
      PHP_SELECT_SAPI(embed2, static, php_embed2.c)
      ;;
    yes|shared-zts)
      PHP_EMBED2_TYPE=shared-zts
      INSTALL_IT="\$(mkinstalldirs) \$(INSTALL_ROOT)\$(prefix)/lib; \$(INSTALL) -m 0755 $SAPI_SHARED \$(INSTALL_ROOT)\$(prefix)/lib"
      PHP_BUILD_THREAD_SAFE
      PHP_SELECT_SAPI(embed2, shared, php_embed2.c)
      ;;
    static-zts)
      PHP_EMBED2_TYPE=static-zts
      INSTALL_IT="\$(mkinstalldirs) \$(INSTALL_ROOT)\$(prefix)/lib; \$(INSTALL) -m 0644 $SAPI_STATIC \$(INSTALL_ROOT)\$(prefix)/lib"
      PHP_BUILD_THREAD_SAFE
      PHP_SELECT_SAPI(embed2, static, php_embed2.c)
      ;;
    *)
      PHP_EMBED2_TYPE=no
      ;;
  esac
  if test "$PHP_EMBED2_TYPE" != "no"; then
    PHP_INSTALL_HEADERS([sapi/embed2/php_embed2.h])
  fi
  AC_MSG_RESULT([$PHP_EMBED2_TYPE])
else
  AC_MSG_RESULT(no)
fi
