#ifndef __DOGEE_WRAPPING_H_
#define __DOGEE_WRAPPING_H_


#ifdef _WIN32
#include <Windows.h>
#include <WinBase.h>
#define BD_LOCK CRITICAL_SECTION
#define BD_RWLOCK SRWLOCK

#define UaInitLock(a) InitializeCriticalSection(a)
#define UaKillLock(a) DeleteCriticalSection(a)
#define UaEnterLock(a) EnterCriticalSection(a)
#define UaLeaveLock(a) LeaveCriticalSection(a)

#define UaInitRWLock(a) InitializeSRWLock(a)
#define UaEnterWriteRWLock(a) AcquireSRWLockExclusive(a)
#define UaLeaveWriteRWLock(a) ReleaseSRWLockExclusive(a)
#define UaEnterReadRWLock(a) AcquireSRWLockShared(a)
#define UaTryEnterWriteRWLock(a) TryAcquireSRWLockExclusive(a)
#define UaTryEnterReadRWLock(a) TryAcquireSRWLockShared(a)
#define UaLeaveReadRWLock(a) ReleaseSRWLockShared(a)
#define UaKillRWLock(a)
#else
#include <unistd.h>
#include <semaphore.h>
#define BD_LOCK pthread_spinlock_t
#define BD_RWLOCK pthread_rwlock_t

#define UaInitLock(a) pthread_spin_init(a,PTHREAD_PROCESS_PRIVATE)
#define UaKillLock(a) pthread_spin_destroy(a)
#define UaEnterLock(a) pthread_spin_lock(a)
#define UaLeaveLock(a) pthread_spin_unlock(a)

#define UaInitRWLock(a) pthread_rwlock_init(a,NULL)
#define UaEnterWriteRWLock(a) pthread_rwlock_wrlock(a)
#define UaLeaveWriteRWLock(a) pthread_rwlock_unlock(a)
#define UaEnterReadRWLock(a) pthread_rwlock_rdlock(a)
#define UaTryEnterWriteRWLock(a) pthread_rwlock_trywrlock(a)
#define UaTryEnterReadRWLock(a) pthread_rwlock_tryrdlock(a)
#define UaLeaveReadRWLock(a) pthread_rwlock_unlock(a)
#define UaKillRWLock(a) pthread_rwlock_destroy(a)
#endif

#endif