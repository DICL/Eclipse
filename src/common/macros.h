#ifndef __MACROS_H__
#define __MACROS_H__

#ifndef __GNUC__
#error "Required GCC"
#endif

#ifdef _DEBUG
#define WEAK __attribute__((weak))
#else
#define WEAK
#endif

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#define OK 0
#define FAIL -1

#ifndef DPSIZE
#define DPSIZE (2 << 13) //! 4096 Bytes
#endif

#ifndef DATAFILE
#define DATAFILE "/scratch/youngmoon01/garbage2.bin"
#endif

#ifndef NSERVERS
#define NSERVERS 39
#endif

#ifndef ALPHA
#define ALPHA 0.03f
#endif

#ifndef PATH_LENGTH
#define PATH_LENGTH 128 
#endif 

#define EXIT_IF(x,m) if ((x) == -1) {log (M_ERR, "SOMENODE", (m));}

#endif
