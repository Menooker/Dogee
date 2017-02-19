#ifndef __DOGEE_H_ 
#define __DOGEE_H_ 

#include <stdint.h>
#define DOGEE_DBG

#ifndef _MSC_VER
#define SUPPORT_TLS
#endif

#ifdef __x86_64__
#define DOGEE_ON_X64
#endif

#ifdef _WIN64
#define DOGEE_ON_X64
#endif

#ifndef SUPPORT_TLS
#define THREAD_LOCAL __declspec( thread )
#else
#define THREAD_LOCAL thread_local
#endif
typedef uint32_t ObjectKey;
typedef uint32_t FieldKey;
typedef uint64_t LongKey;

#define DOGEE_MAX_SHARED_KEY_TRIES  200

#define MAKE64(a,b) (unsigned long long)( ((unsigned long long)a)<<32 | (unsigned long long)b)

#define DOGEE_USE_MEMCACHED
//some globally shared declarations

#endif 