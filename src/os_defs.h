#ifndef OS_DEFS_H
#define OS_DEFS_H

// OS macro
#ifdef _AIX
#define OS "IBM AIX"

#elif __ANDROID__
#define OS "Android"

#elif UTS
#define OS "Amdahl UTS"

#elif __amigaos__
#define OS "AmigaOS"

#elif apollo
#define OS "Apollo Domain OS"

#elif __BEOS__
#define OS "BeOS"

#elif __BG__
#define OS "Blue Gene"

#elif __FreeBSD__
#define OS "FreeBSD"

#elif __NetBSD__
#define OS "NetBSD"

#elif __OpenBSD__
#define OS "OpenBSD"

#elif __bsdi__
#define OS "BSD/OS"

#elif __DragonFly__
#define OS "Dragon Fly BSD"

#elif __convex__
#define OS "ConvexOS"

#elif __CYGWIN__
#define OS "Cygwin Environment"

#elif __DGUX__
#define OS "DG/UX"

#elif _SEQUENT_
#define OS "DYNIX/ptx"

#elif __ECOS
#define OS "eCos"

#elif __EMX__
#define OS "EMX Environment"

#elif __GNU__
#define OS "GNU/Hurd"

#elif __FreeBSD_kernel__
#ifdef __GLIBC__
#define OS "GNU/kFreeBSD"
#else
#define OS "Unknown (FreeBSD?)"
#endif // __GLIBC__

#elif __gnu_linux__
#define OS "GNU Linux"

#elif __hiuxmpp
#define OS "HI-UX MPP"

#elif _hpux
#define OS "HP-UX"

#elif __OS400__
#define OS "IBM OS/400"

#elif __INTEGRITY
#define OS "Integrity"

#elif __INTERIX
#define OS "Interix Environment"

#elif __sgi
#define OS "Irix"

#elif __linux__
#define OS "Linux"

#elif __Lynx__
#define OS "LynxOS"

#elif __APPLE__
#ifdef __MACH__
#define OS "MacOS"
#else
#define OS "Unknown (Apple?)"
#endif

#elif __OS9000
#define OS "Microware OS-9"

#elif __minix
#define OS "Minix"

#elif __MORPHOS__
#define OS "MorphOS"

#elif __mpexl
#define OS "MPE/iX"

#elif __MSDOS__
#define OS "MS-DOS"

#elif __TANDEM
#define OS "NonStop"

#elif __nucleus__
#define OS "Nucleus RTOS"

#elif __OS2__
#define OS "OS/2"

#elif __palmos__
#define OS "Palm OS"

#elif EPLAN9
#define OS "Plan 9"

#elif pyr
#define OS "Pyramid DC/OSx"

#elif __QNX__
#define OS "QNX"

#elif sinux
#define OS "Reliant UNIX"

#elif M_XENIX
#define OS "SCO OpenServer"

#elif __sun
#if defined(sun) || defined(__sun)
# if defined(__SVR4) || defined(__svr4__)
#define OS "Solaris"
# else
#define OS "SunOS"
# endif
#endif

#elif __VOS__
#define OS "Stratus VOS"

#elif __sysv__
#define OS "SVR4 Environment"

#elif __SYLLABLE__
#define OS "Syllable"

#elif __SYMBIAN32__
#define OS "Symbian OS"

#elif __osf__
#define OS "Tru64 (OSF/1)"

#elif __ultrix__
#define OS "Ultrix"

#elif _UNICOS
#define OS "UNICOS"

#elif __unix__
#define OS "Unix"

#elif _UNIXWARE7
#define OS "UnixWare"

#elif _UWIN
#define OS "U/Win Environment"

#elif __VMS
#define OS "VMS"

#elif __VXWORKS__
#define OS "VxWorks"

#elif _WIN16
#define OS "Windows 16 bit"

#elif  _WIN32
#define OS "Windows 32 bit"

#elif  _WIN64
#define OS "Windows 64 bit"

#elif _WIN32_WCE
#define OS "Windows CE"

#elif _WINDU_SOURCE
#define OS "Wind/U Environment"

#elif __MVS__
#define OS "z/OS"

#else
#define OS "Unknown"

#endif // OS macro chain

// new line
#ifdef _WIN32
#define PLATFORM_NL "\r\n"
#else
#define PLATFORM_NL "\n"
#endif

#endif // header
