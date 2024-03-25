#ifndef __SYLAR_MUTEX_H__
#define __SYLAR_MUTEX_H__

#include <thread>
#include <functional>
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <atomic>
#include <list>

#include "noncopyable.h"
#include "fiber.h"

namespace sylar {

/**
 * @brief 信号量
 */
class Semaphore : Noncopyable {
public:
    /**
     * @brief 构造函数
     * @param[in] count 信号量值的大小
     */
    Semaphore(uint32_t count = 0);

    /**
     * @brief 析构函数
     */
    ~Semaphore();

    /**
     * @brief 获取信号量
     */
    void wait();

    /**
     * @brief 释放信号量
     */
    void notify();
private:
    sem_t m_semaphore;
};

/**
 * @brief 局部锁的模板实现
 */
template<class T>
struct ScopedLockImpl {
public:
    /**
     * @brief 构造函数
     * @param[in] mutex Mutex
     */
    // 构造加锁
    ScopedLockImpl(T& mutex)    
        :m_mutex(mutex) {
        m_mutex.lock();
        m_locked = true;
    }

    /**
     * @brief 析构函数,自动释放锁
     */
    ~ScopedLockImpl() {
        unlock();
    }

    /**
     * @brief 加锁
     */
    void lock() {
        if(!m_locked) {
            m_mutex.lock();    // lock()独占式锁定操作，当一个线程调用 lock() 函数获取锁时，如果锁已经被其他线程获取，
            //那么该线程将会被阻塞，直到锁被释放为止。lock() 函数用于实现互斥访问，即一次只允许一个线程访问共享资源，
            //其他线程需要等待当前线程释放锁之后才能访问。
            m_locked = true;
        }
    }

    /**
     * @brief 解锁
     */
    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    /// mutex，互斥量
    T& m_mutex;
    /// 是否已上锁
    bool m_locked;
};

/**
 * @brief 局部读锁模板实现
 */
template<class T>
struct ReadScopedLockImpl {
public:
    /**
     * @brief 构造函数
     * @param[in] mutex 读写锁
     */
    ReadScopedLockImpl(T& mutex)
        :m_mutex(mutex) {
        m_mutex.rdlock();
        m_locked = true;
    }

    /**
     * @brief 析构函数,自动释放锁
     */
    ~ReadScopedLockImpl() {
        unlock();
    }

    /**
     * @brief 上读锁
     */
    void lock() {
        if(!m_locked) {
            m_mutex.rdlock();  //读锁，实现了共享式的锁定操作。当一个线程调用 rdlock() 函数获取读取锁时，
            //如果锁已经被其他线程以共享模式获取，那么该线程可以立即获取锁而不被阻塞。多个线程可以同时获取读取锁，
            //因为读取锁是共享的，不会阻塞其他读取操作。读取锁通常用于实现读取操作的并发性，多个线程可以同时读取共享资源，不会互相影响。
            m_locked = true;
        }
    }

    /**
     * @brief 释放锁
     */
    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    /// mutex
    T& m_mutex;
    /// 是否已上锁
    bool m_locked;
};

/**
 * @brief 局部写锁模板实现
 */
template<class T>
struct WriteScopedLockImpl {
public:
    /**
     * @brief 构造函数
     * @param[in] mutex 读写锁
     */
    WriteScopedLockImpl(T& mutex)
        :m_mutex(mutex) {
        m_mutex.wrlock();
        m_locked = true;
    }

    /**
     * @brief 析构函数
     */
    ~WriteScopedLockImpl() {
        unlock();
    }

    /**
     * @brief 上写锁
     */
    void lock() {
        if(!m_locked) {
            m_mutex.wrlock();    // 独占式锁，一次只允许一个线程以写入模式获取锁，并且其他线程需要等待写入模式的锁释放后才能获取锁。
            m_locked = true;
        }
    }

    /**
     * @brief 解锁
     */
    void unlock() {
        if(m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }
private:
    /// Mutex
    T& m_mutex;
    /// 是否已上锁
    bool m_locked;
};

/**
 * @brief 互斥量
 */
class Mutex : Noncopyable {
public: 
    /// 局部锁
    typedef ScopedLockImpl<Mutex> Lock;    // 创建了一个名为 Lock 的类型，它实际上是 ScopedLockImpl<Mutex> 的别名

    /**
     * @brief 构造函数
     */
    Mutex() {
        pthread_mutex_init(&m_mutex, nullptr);
    }

    /**
     * @brief 析构函数
     */
    ~Mutex() {
        pthread_mutex_destroy(&m_mutex);
    }

    /**
     * @brief 加锁
     */
    void lock() {
        pthread_mutex_lock(&m_mutex);
    }

    /**
     * @brief 解锁
     */
    void unlock() {
        pthread_mutex_unlock(&m_mutex);
    }
private:
    /// mutex
    pthread_mutex_t m_mutex;
};

/**
 * @brief 空锁(用于调试)
 */
class NullMutex : Noncopyable{
public:
    /// 局部锁
    typedef ScopedLockImpl<NullMutex> Lock;

    /**
     * @brief 构造函数
     */
    NullMutex() {}

    /**
     * @brief 析构函数
     */
    ~NullMutex() {}

    /**
     * @brief 加锁
     */
    void lock() {}

    /**
     * @brief 解锁
     */
    void unlock() {}
};

/**
 * @brief 读写互斥量
 */
class RWMutex : Noncopyable{
public:

    /// 局部读锁
    typedef ReadScopedLockImpl<RWMutex> ReadLock;

    /// 局部写锁
    typedef WriteScopedLockImpl<RWMutex> WriteLock;

    /**
     * @brief 构造函数
     */
    RWMutex() {
        pthread_rwlock_init(&m_lock, nullptr);
    }
    
    /**
     * @brief 析构函数
     */
    ~RWMutex() {
        pthread_rwlock_destroy(&m_lock);
    }

    /**
     * @brief 上读锁
     */
    void rdlock() {
        pthread_rwlock_rdlock(&m_lock);
    }

    /**
     * @brief 上写锁
     */
    void wrlock() {
        pthread_rwlock_wrlock(&m_lock);
    }

    /**
     * @brief 解锁
     */
    void unlock() {
        pthread_rwlock_unlock(&m_lock);
    }
private:
    /// 读写锁
    pthread_rwlock_t m_lock;
};

/**
 * @brief 空读写锁(用于调试)
 */
class NullRWMutex : Noncopyable {
public:
    /// 局部读锁
    typedef ReadScopedLockImpl<NullMutex> ReadLock;
    /// 局部写锁
    typedef WriteScopedLockImpl<NullMutex> WriteLock;

    /**
     * @brief 构造函数
     */
    NullRWMutex() {}
    /**
     * @brief 析构函数
     */
    ~NullRWMutex() {}

    /**
     * @brief 上读锁
     */
    void rdlock() {}

    /**
     * @brief 上写锁
     */
    void wrlock() {}
    /**
     * @brief 解锁
     */
    void unlock() {}
};

/**
 * @brief 自旋锁，用于线程同步的锁机制。它的特点是在尝试获取锁时不会立即进入睡眠状态，而是使用循环方式不断尝试获取锁。
 *因此，当锁被其他线程持有时，当前线程会处于忙等待状态，不会进入睡眠，直到获取到锁为止。
 *忙等待： 当一个线程尝试获取自旋锁时，如果锁已经被其他线程持有，则该线程会一直处于忙等待状态，不断尝试获取锁，直到成功为止。
 *无睡眠： 自旋锁的获取操作不会使得线程进入睡眠状态，因此不会发生上下文切换，避免了进程切换的开销。
 *短暂持有： 自旋锁通常用于临界区较短的情况，以避免长时间的忙等待造成的性能损失。
 */
class Spinlock : Noncopyable {
public:
    /// 局部锁
    typedef ScopedLockImpl<Spinlock> Lock;

    /**
     * @brief 构造函数
     */
    Spinlock() {
        pthread_spin_init(&m_mutex, 0);
    }

    /**
     * @brief 析构函数
     */
    ~Spinlock() {
        pthread_spin_destroy(&m_mutex);
    }

    /**
     * @brief 上锁
     */
    void lock() {
        pthread_spin_lock(&m_mutex);
    }

    /**
     * @brief 解锁
     */
    void unlock() {
        pthread_spin_unlock(&m_mutex);
    }
private:
    /// 自旋锁
    pthread_spinlock_t m_mutex;
};

/**
 * @brief 原子锁
 */
class CASLock : Noncopyable {
public:
    /// 局部锁
    typedef ScopedLockImpl<CASLock> Lock;

    /**
     * @brief 构造函数
     */
    CASLock() {
        m_mutex.clear();    //将原子标志 m_mutex 清除，以确保锁处于未锁定状态，以确保对象在创建后是可用的，而不是处于被锁定的状态。
    }

    /**
     * @brief 析构函数
     */
    ~CASLock() {
    }

    /**
     * @brief 上锁，std::atomic_flag_test_and_set_explicit 是一个原子操作，
     *它在执行期间会将原子标志 m_mutex 设置为 true，并返回之前的值。
     *因此，只有当 m_mutex 之前的值为 false（即锁未被占用）时，才能成功获取锁。
     */
    void lock() {
        while(std::atomic_flag_test_and_set_explicit(&m_mutex, std::memory_order_acquire));    
    }

    /**
     * @brief 解锁
     */
    void unlock() {
        std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
    }
private:
    /// 原子状态
    volatile std::atomic_flag m_mutex;    //编译器会将对该变量的读写操作看作具有未知的副作用，不会对m_mutex进行优化。
};

class Scheduler;
class FiberSemaphore : Noncopyable {
public:
    typedef Spinlock MutexType;

    FiberSemaphore(size_t initial_concurrency = 0);    // 指定初始的并发数
    ~FiberSemaphore();

    bool tryWait();    // 尝试等待信号量。如果信号量可用，则立即返回 true；否则返回 false。
    void wait();    // 等待信号量。如果信号量不可用，则当前线程将被阻塞，直到信号量可用为止。
    void notify();    //通知信号量。唤醒等待的线程，使得它们可以继续执行。

    size_t getConcurrency() const { return m_concurrency;}
    void reset() { m_concurrency = 0;}
private:
    MutexType m_mutex;
    std::list<std::pair<Scheduler*, Fiber::ptr> > m_waiters;    //存储等待信号量的调度器和协程指针的列表。
    size_t m_concurrency;    //并发数
};



}

#endif
