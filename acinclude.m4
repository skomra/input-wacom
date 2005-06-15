dnl Macros for configuring the Linux Wacom package
dnl
AC_DEFUN([AC_WCM_SET_PATHS],[
if test "x$prefix" = xNONE
then WCM_PREFIX=$ac_default_prefix
else WCM_PREFIX=$prefix
fi
if test "x$exec_prefix" = xNONE
then WCM_EXECDIR=$WCM_PREFIX
else WCM_EXECDIR=$exec_prefix
fi
])
AC_DEFUN([AC_WCM_CHECK_ENVIRON],[
dnl Variables for various checks
WCM_ARCH=unknown
WCM_KSTACK=-mpreferred-stack-boundary=2
WCM_KERNEL=unknown
WCM_KERNEL_VER=2.4
WCM_ISLINUX=no
WCM_ENV_KERNEL=no
WCM_OPTION_MODVER=no
WCM_KERNEL_WACOM_DEFAULT=no
WCM_ENV_XF86=no
WCM_ENV_XORGSDK=no
WCM_LINUX_INPUT=
WCM_PATCH_WACDUMP=
WCM_PATCH_WACOMDRV=
WCM_ENV_GTK=no
WCM_ENV_TCL=no
WCM_ENV_TK=no
WCM_XIDUMP_DEFAULT=yes
WCM_ENV_XLIB=no
WCM_XLIBDIR_DEFAULT=/usr/X11R6/lib
WCM_TCLTKDIR_DEFAULT=/usr
XF86SUBDIR=xc/programs/Xserver/hw/xfree86/common
WCM_LINUXWACOMDIR=`pwd`
WCM_ENV_NCURSES=no
dnl Check architecture
AC_MSG_CHECKING(for processor type)
WCM_ARCH=`uname -m`
AC_MSG_RESULT($WCM_ARCH)
dnl
dnl Check kernel type
AC_MSG_CHECKING(for kernel type)
WCM_KERNEL=`uname -s`
AC_MSG_RESULT($WCM_KERNEL)
dnl
dnl Check for linux
AC_MSG_CHECKING(for linux-based kernel)
islinux=`echo $WCM_KERNEL | grep -i linux | wc -l`
if test $islinux != 0; then
	WCM_ISLINUX=yes
fi
AC_MSG_RESULT($WCM_ISLINUX)
if test x$WCM_ISLINUX != xyes; then
	echo "***"
	echo "*** WARNING:"
	echo "*** Linux kernel not detected; linux-specific features will not"
	echo "*** be built including USB support in XFree86 and kernel drivers."
	echo "***"
fi
dnl
dnl Check for linux kernel override
AC_ARG_WITH(linux,
AS_HELP_STRING([--with-linux], [Override linux kernel check]),
[ WCM_ISLINUX=$withval ])
dnl
dnl Handle linux specific features
if test x$WCM_ISLINUX = xyes; then
	WCM_KERNEL_WACOM_DEFAULT=yes
	WCM_LINUX_INPUT="-DLINUX_INPUT"
	AC_DEFINE(WCM_ENABLE_LINUXINPUT,1,[Enable the Linux Input subsystem])
else
	WCM_PATCH_WACDUMP="(no USB) $WCM_PATCH_WACDUMP"
	WCM_PATCH_WACOMDRV="(no USB) $WCM_PATCH_WACOMDRV"
fi
])
dnl
dnl
dnl
AC_DEFUN([AC_WCM_CHECK_KERNELSOURCE],[
dnl Check for kernel build environment
AC_ARG_WITH(kernel,
AS_HELP_STRING([--with-kernel=dir], [Specify kernel source directory]),
[
	WCM_KERNELDIR="$withval"
	AC_MSG_CHECKING(for valid kernel source tree)
	if test -f "$WCM_KERNELDIR/include/linux/input.h"; then
		AC_MSG_RESULT(ok)
		WCM_ENV_KERNEL=yes
	else
		AC_MSG_RESULT(missing input.h)
		AC_MSG_ERROR("Unable to find $WCM_KERNELDIR/include/linux/input.h")
		WCM_ENV_KERNEL=no
	fi
],
[
	dnl guess directory
	AC_MSG_CHECKING(for kernel sources)
	WCM_KERNELDIR="/usr/src/linux /usr/src/linux-`uname -r` /usr/src/linux-2.4 /usr/src/linux-2.6 /lib/modules/`uname -r`/build"

	for i in $WCM_KERNELDIR; do
		if test -f "$i/include/linux/input.h"; then
			WCM_ENV_KERNEL=yes
			WCM_KERNELDIR=$i
			break
		fi
	done

	if test "x$WCM_ENV_KERNEL" = "xyes"; then
		WCM_ENV_KERNEL=yes
		AC_MSG_RESULT($WCM_KERNELDIR)
	else
		AC_MSG_RESULT(not found)
		WCM_KERNELDIR=""
		WCM_ENV_KERNEL=no
		echo "***"
		echo "*** WARNING:"
		echo "*** Unable to guess kernel source directory"
		echo "*** Looked at /usr/src/linux"
		echo "*** Looked at /usr/src/linux-`uname -r`"
		echo "*** Looked at /usr/src/linux-2.4"
		echo "*** Looked at /usr/src/linux-2.6"
		echo "*** Looked at /lib/modules/`uname -r`/build"
		echo "*** Kernel modules will not be built"
		echo "***"
	fi
])])
dnl
AC_DEFUN([AC_WCM_CHECK_MODVER],[
dnl Guess modversioning
if test x$WCM_ENV_KERNEL = xyes; then
	AC_MSG_CHECKING(for kernel module versioning)
	if test -f "$WCM_KERNELDIR/include/linux/version.h"; then
		WCM_OPTION_MODVER=yes
		AC_MSG_RESULT(yes)
		moduts=`grep UTS_RELEASE $WCM_KERNELDIR/include/linux/version.h`
		ISVER=`echo $moduts | grep -c "2.4"` 
		if test "$ISVER" -gt 0; then
			MINOR=`echo $moduts | cut -f 1 -d- | cut -f3 -d. | cut -f1 -d\" | sed 's/\([[0-9]]*\).*/\1/'`
			if test $MINOR -ge 22; then
				WCM_KERNEL_VER="2.4.22"
			else
				WCM_KERNEL_VER="2.4"
			fi
		else
			ISVER=`echo $moduts | grep -c "2.6"` 
			if test "$ISVER" -gt 0; then
				MINOR=`echo $moduts | cut -f 1 -d- | cut -f3 -d. | cut -f1 -d\" | sed 's/\([[0-9]]*\).*/\1/'`
				if test $MINOR -ge 11; then
					WCM_KERNEL_VER="2.6.11"
				elif test $MINOR -eq 10; then
					WCM_KERNEL_VER="2.6.10"
				elif test $MINOR -eq 9; then
					WCM_KERNEL_VER="2.6.9"
				elif test $MINOR -eq 8; then
					WCM_KERNEL_VER="2.6.8"
				elif test $MINOR -eq 7; then
					WCM_KERNEL_VER="2.6.7"
				elif test $MINOR -eq 6; then
					WCM_KERNEL_VER="2.6.6"
				elif test $MINOR -eq 5; then
					WCM_KERNEL_VER="2.6.5"
				elif test $MINOR -eq 4; then
					WCM_KERNEL_VER="2.6.4"
				elif test $MINOR -eq 3; then
					WCM_KERNEL_VER="2.6.3"
				elif test $MINOR -eq 2; then
					WCM_KERNEL_VER="2.6.2"
				else
					WCM_KERNEL_VER="2.6"
				fi
			else
				echo "***"
				echo "*** WARNING:"
				echo "*** $moduts is not supported by this pachage"
				echo "*** Kernel modules will not be built"
				echo "***"
				WCM_OPTION_MODVER=no
				AC_MSG_RESULT(no)
				WCM_ENV_KERNEL=no
			fi
		fi
	else
		echo "***"
		echo "*** WARNING:"
		echo "*** version.h is not in $WCM_KERNELDIR/include/linux"
		echo "*** Kernel modules will not be built"
		echo "***"
		WCM_KERNELDIR=""
		WCM_ENV_KERNEL=no
		WCM_OPTION_MODVER=no
		AC_MSG_RESULT(no)
		
	fi
fi
])
dnl
AC_DEFUN([AC_WCM_CHECK_XORG_SDK],[
dnl Check for Xorg sdk environment
AC_ARG_WITH(xorg-sdk,
AS_HELP_STRING([--with-xorg-sdk=dir], [Specify Xorg SDK directory]),
[ WCM_XORGSDK="$withval"; ])
if test -n "$WCM_XORGSDK"; then
	AC_MSG_CHECKING(for valid Xorg SDK)
	if test -f $WCM_XORGSDK/include/xf86Version.h; then
		WCM_ENV_XORGSDK=yes
		AC_MSG_RESULT(ok)
	elif test -f $WCM_XORGSDK/xc/include/xf86Version.h; then
		WCM_ENV_XORGSDK=yes
		$WCM_XORGSDK=$WCM_XORGSDK/xc
		AC_MSG_RESULT(ok)
	else
		AC_MSG_RESULT("xf86Version.h missing")
		AC_MSG_ERROR("Unable to find xf86Version.h under $WCM_XORGSDK/include and WCM_XORGSDK/xc/include")
	fi
	WCM_XORGSDK=`(cd $WCM_XORGSDK; pwd)`
fi
AM_CONDITIONAL(WCM_ENV_XORGSDK, [test x$WCM_ENV_XORGSDK = xyes])
])
AC_DEFUN([AC_WCM_CHECK_XFREE86SOURCE],[
dnl Check for XFree86 build environment
if test -d x-includes; then
	WCM_XF86DIR=x-includes
fi
AC_ARG_WITH(xf86,
AS_HELP_STRING([--with-xf86=dir], [Specify XF86 build directory]),
[ WCM_XF86DIR="$withval"; ])
if test -n "$WCM_XF86DIR"; then
	AC_MSG_CHECKING(for valid XFree86 build environment)
	if test -f $WCM_XF86DIR/$XF86SUBDIR/xf86Version.h; then
		WCM_ENV_XF86=yes
		AC_MSG_RESULT(ok)
	else
		AC_MSG_RESULT("xf86Version.h missing")
		AC_MSG_ERROR("Unable to find $WCM_XF86DIR/$XF86SUBDIR/xf86Version.h")
	fi
	WCM_XF86DIR=`(cd $WCM_XF86DIR; pwd)`
fi
AM_CONDITIONAL(WCM_ENV_XF86, [test x$WCM_ENV_XF86 = xyes])
])
AC_DEFUN([AC_WCM_CHECK_GTK],[
dnl Check for GTK development environment
AC_ARG_WITH(gtk,
AS_HELP_STRING([--with-gtk={1.2|2.0|yes|no}],[Uses GTK 1.2 or 2.0 API]),
[WCM_USE_GTK=$withval],[WCM_USE_GTK=yes])

if test "$WCM_USE_GTK" == "yes" || test "$WCM_USE_GTK" == "1.2"; then
	AC_CHECK_PROG(gtk12config,gtk-config,yes,no)
fi
if test "$WCM_USE_GTK" == "yes" || test "$WCM_USE_GTK" == "2.0"; then
	AC_CHECK_PROG(pkgconfig,pkg-config,yes,no)
	if test x$pkgconfig == xyes; then
		AC_MSG_CHECKING(pkg-config for gtk+-2.0)
		gtk20config=`pkg-config --exists gtk+-2.0 && echo yes`
		if test "$gtk20config" == "yes"; then
			AC_MSG_RESULT(yes)
		else
			AC_MSG_RESULT(no)
		fi
	fi
fi

dnl Default to best GTK available
if test "$WCM_USE_GTK" == "yes"; then
	if test "$gtk20config" == "yes"; then
		WCM_USE_GTK=2.0
	elif test "$gtk12config" == "yes"; then
		WCM_USE_GTK=1.2
	else
		echo "***"; echo "*** WARNING:"
		echo "*** unable to find any gtk development environment; are the "
		echo "*** development packages installed?  gtk will not be used."
		echo "***"
		WCM_USE_GTK=no
	fi
fi

dnl Handle GTK 1.2
if test "$WCM_USE_GTK" == "1.2"; then
	if test "$gtk12config" != "yes"; then
		echo "***"; echo "*** WARNING:"
		echo "*** unable to find gtk-config in path; are the development"
		echo "*** packages installed?  gtk will not be used."
		echo "***"
	else
		AC_MSG_CHECKING(for GTK version)
		gtk12ver=`gtk-config --version`
		if test $? != 0; then
			AC_MSG_RESULT(unknown)
			AC_MSG_ERROR(gtk-config failed)
		fi
		AC_MSG_RESULT($gtk12ver)
		WCM_ENV_GTK=$gtk12ver
		AC_DEFINE(WCM_ENABLE_GTK12,1,Use GTK 1.2 API)
		CFLAGS="$CFLAGS `gtk-config --cflags`"
		LIBS="$LIBS `gtk-config --libs`"
	fi
fi

dnl Handle GTK 2.0
if test "$WCM_USE_GTK" == "2.0"; then
	if test "$pkgconfig" != "yes"; then
		echo "***"; echo "*** WARNING:"
		echo "*** unable to find pkg-config in path; gtk 2.0 requires"
		echo "*** pkg-config to locate the proper development environment."
		echo "*** gtk will not be used."
		echo "***"
	elif test "$gtk20config" != "yes"; then
		echo "***"; echo "*** WARNING:"
		echo "*** unable to find gtk 2.0 registered with pkg-config;"
		echo "*** are the development packages installed?"
		echo "*** pkg-config is not very smart; if gtk has dependencies"
		echo "*** that are not installed, you might still get this error."
		echo "*** Try using pkg-config --debug gtk+-2.0  to see what it is"
		echo "*** complaining about.  Misconfigured systems may choke"
		echo "*** looking for gnome-config; if this is the case, you will"
		echo "*** need to install the Gnome development libraries even"
		echo "*** though we will not use them."
		echo "***"
	else
		AC_MSG_CHECKING(for GTK version)
		gtk20ver=`pkg-config --modversion gtk+-2.0`
		if test $? != 0; then
			AC_MSG_RESULT(unknown)
			AC_MSG_ERROR(pkg-config failed)
		fi
		AC_MSG_RESULT($gtk20ver)
		WCM_ENV_GTK=$gtk20ver
		AC_DEFINE(WCM_ENABLE_GTK20,1,Use GTK 2.0 API)
		CFLAGS="$CFLAGS `pkg-config --cflags gtk+-2.0`"
		LIBS="$LIBS `pkg-config --libs gtk+-2.0`"
	fi
fi
])
AC_DEFUN([AC_WCM_CHECK_XLIB],[
dnl Check for XLib development environment
WCM_XLIBDIR=
AC_ARG_WITH(xlib,
AS_HELP_STRING([--with-xlib=dir], [uses a specified X11R6 directory]),
[WCM_XLIBDIR=$withval])

dnl handle default case
AC_MSG_CHECKING(for X lib directory)
if test "$WCM_XLIBDIR" == "" || test "$WCM_XLIBDIR" == "yes"; then
	if test -d $WCM_XLIBDIR_DEFAULT/X11; then
		WCM_ENV_XLIB=yes
		WCM_XLIBDIR=$WCM_XLIBDIR_DEFAULT
		AC_MSG_RESULT(found)
	else
		AC_MSG_RESULT(not found, tried $WCM_XLIBDIR_DEFAULT/X11)
		WCM_ENV_XLIB=no
	fi
elif test -d $WCM_XLIBDIR; then
	WCM_ENV_XLIB=yes
	AC_MSG_RESULT(found)
else
	AC_MSG_RESULT(not found, tried $WCM_XLIBDIR)
	WCM_ENV_XLIB=no
fi
])
AC_DEFUN([AC_WCM_CHECK_TCL],[
dnl Check for TCL development environment
WCM_TCLDIR=
AC_ARG_WITH(tcl, 
AS_HELP_STRING([--with-tcl=dir], [uses a specified tcl directory  ]),
[ WCM_TCLDIR=$withval ])

dnl get tcl version
AC_PATH_PROG([TCLSH],[tclsh],[no])
if test "x$TCLSH" != "xno"; then
	AC_MSG_CHECKING([for tcl version])
	version=$(echo ["puts [set tcl_version]"] | $TCLSH)
	AC_MSG_RESULT([$version])
fi

dnl handle default case
if test "$WCM_TCLDIR" = "yes" || test "$WCM_TCLDIR" == ""; then
	AC_MSG_CHECKING([for tcl header files])
	dir="$WCM_TCLTKDIR_DEFAULT/include";
	for i in "" tcl/ "tcl$version/"; do
		if test "x$WCM_ENV_TCL" != "xyes"; then
			if test -f "$dir/$i/tcl.h"; then
				AC_MSG_RESULT([$dir/$i])
				WCM_ENV_TCL=yes
				WCM_TCLDIR="$dir/$i"
				CFLAGS="$CFLAGS -I$WCM_TCLDIR"
			fi
		fi
	done
	if test "x$WCM_ENV_TCL" != "xyes"; then
		AC_MSG_RESULT([not found; tried $WCM_TCLTKDIR_DEFAULT/include/tcl.h])		
		echo "***"; echo "*** WARNING:"
		echo "*** The tcl development environment does not appear to"
		echo "*** be installed. The header file tcl.h does not appear"
		echo "*** in the include path. Do you have the tcl rpm or"
		echo "*** equivalent package properly installed?  Some build"
		echo "*** features will be unavailable."
		echo "***"
	fi

dnl handle specified case
elif test "$WCM_TCLDIR" != "no"; then
	AC_MSG_CHECKING([for tcl header files])
	if test -f "$WCM_TCLDIR/include/tcl.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TCL=yes
		if test "$WCM_TCLDIR" != "/usr"; then
			CFLAGS="$CFLAGS -I$WCM_TCLDIR/include"
		fi
	elif test -f "$WCM_TCLDIR/tcl.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TCL=yes
		if test "$WCM_TCLDIR" != "/usr"; then
			CFLAGS="$CFLAGS -I$WCM_TCLDIR"
		fi
	else
		AC_MSG_RESULT([not found; tried $WCM_TCLDIR/include/tcl.h and $WCM_TCLDIR/tcl.h])
		echo "***"; echo "*** WARNING:"
		echo "*** The tcl development environment does not appear to"
		echo "*** be installed. The header file tcl.h does not appear"
		echo "*** in the include path. Do you have the tcl rpm or"
		echo "*** equivalent package properly installed?  Some build"
		echo "*** features will be unavailable."
		echo "***"
	fi
fi
])
AC_DEFUN([AC_WCM_CHECK_TK],[
dnl Check for TK development environment
WCM_TKDIR=
AC_ARG_WITH(tk,
AS_HELP_STRING([--with-tk=dir], [uses a specified tk directory  ]), 
[ WCM_TKDIR=$withval ])

dnl handle default case
if test "$WCM_TKDIR" = "yes" || test "$WCM_TKDIR" == ""; then
	AC_MSG_CHECKING([for tk header files])
	if test -f "$WCM_TCLTKDIR_DEFAULT/include/tk.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TK=yes
		WCM_TKDIR="$WCM_TCLTKDIR_DEFAULT"
	elif test -f "$WCM_TCLDIR/include/tk.h" || test -f "$WCM_TCLDIR/tk.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TK=yes
		WCM_TKDIR="$WCM_TCLDIR"
	else
		AC_MSG_RESULT([not found; tried $WCM_TCLTKDIR_DEFAULT/include/tk.h and $WCM_TCLDIR/include/tk.h])
		echo "***"; echo "*** WARNING:"
		echo "*** The tk development environment does not appear to"
		echo "*** be installed. The header file tk.h does not appear"
		echo "*** in the include path. Do you have the tk rpm or"
		echo "*** equivalent package properly installed?  Some build"
		echo "*** features will be unavailable."
		echo "***"
	fi
dnl handle specified case
elif test "$WCM_TKDIR" != "no"; then
	AC_MSG_CHECKING([for tk header files])
	if test -f "$WCM_TKDIR/include/tk.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TK=yes
		if test "$WCM_TCLDIR" != "$WCM_TKDIR" && "$WCM_TKDIR" != "/usr"; then
			CFLAGS="$CFLAGS -I$WCM_TKDIR/include"
		fi
	elif test -f "$WCM_TKDIR/tk.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TK=yes
		if test "$WCM_TCLDIR" != "$WCM_TKDIR" && test "$WCM_TKDIR" != "/usr"; then
			CFLAGS="$CFLAGS -I$WCM_TKDIR"
		fi
	else
		AC_MSG_RESULT(not found; tried $WCM_TKDIR/include/tk.h and $WCM_TKDIR/tk.h)
		echo "***"; echo "*** WARNING:"
		echo "*** The tk library does not appear to be installed."
		echo "*** Do you have the tk rpm or equivalent package properly"
		echo "*** installed?  Some build features will be unavailable."
		echo "***"
	fi
fi
])
AC_DEFUN([AC_WCM_CHECK_NCURSES],[
dnl Check for ncurses development environment
AC_CHECK_HEADER(ncurses.h, [WCM_ENV_NCURSES=yes])
if test x$WCM_ENV_NCURSES != xyes; then
	echo "***"; echo "*** WARNING:"
	echo "*** The ncurses development library does not appear to be installed."
	echo "*** The header file ncurses.h does not appear in the include path."
	echo "*** Do you have the ncurses-devel rpm or equivalent package"
	echo "*** properly installed?  Some build features will be unavailable."
	echo "***"
	AC_DEFINE(WCM_ENABLE_NCURSES,0,[ncurses header files available])
else
	AC_DEFINE(WCM_ENABLE_NCURSES,1,[ncurses header files available])
fi
])
