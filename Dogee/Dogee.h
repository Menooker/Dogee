#ifndef __DOGEE_H_ 
#define __DOGEE_H_ 

#include <stdint.h>
#define DOGEE_DBG
#ifndef SUPPORT_TLS
#define THREAD_LOCAL __declspec( thread )
#else
#define THREAD_LOCAL thread_local
#endif
typedef uint32_t ObjectKey;
typedef uint32_t FieldKey;
typedef uint64_t LongKey;

#define MAKE64(a,b) (unsigned long long)( ((unsigned long long)a)<<32 | (unsigned long long)b)

#define DOGEE_USE_MEMCACHED
//some globally shared declarations

#endif 