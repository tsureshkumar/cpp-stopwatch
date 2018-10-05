AC_INIT(stopwatch, [0.1.0],[tsureshkumar2000@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CHECK_HEADERS(curses.h,,,)
AC_CONFIG_HEADERS([config.h])
AC_CHECK_LIB(ncurses,initscr ,[], [])
AC_PROG_CXX
AC_PROG_INSTALL
AC_SUBST(CURSES)
AC_CONFIG_FILES([
    Makefile
])
AC_OUTPUT

