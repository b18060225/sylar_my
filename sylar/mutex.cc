#include "mutex.h"
#include "macro.h"
#include "scheduler.h"

namespace sylar {

Semaphore::Semaphore(uint32_t count) {
    if(sem_init(&m_semaphore, 0, count)) {    //调用 sem_init() 函数初始化信号量
        throw std::logic_error("sem_init error");
    }
}

Semaphore::~Semaphore() {
    sem_destroy(&m_semaphore);
}

void Semaphore::wait() {    //等待信号量
    if(sem_wait(&m_semaphore)) {
        throw std::logic_error("sem_wait error");
    }
}

void Semaphore::notify() {
    if(sem_post(&m_semaphore)) {    //调用 sem_post() 函数增加信号量的计数
        throw std::logic_error("sem_post error");
    }
}

FiberSemaphore::FiberSemaphore(size_t initial_concurrency)
    :m_concurrency(initial_concurrency) {
}

FiberSemaphore::~FiberSemaphore() {
    SYLAR_ASSERT(m_waiters.empty());    // 确保在销毁时等待队列为空。
}

bool FiberSemaphore::tryWait() {
    SYLAR_ASSERT(Scheduler::GetThis());    // 首先获取互斥量的锁
    {
        MutexType::Lock lock(m_mutex);
        if(m_concurrency > 0u) {
            --m_concurrency;
            return true;
        }
        return false;
    }
}

void FiberSemaphore::wait() {
    SYLAR_ASSERT(Scheduler::GetThis());
    {
        MutexType::Lock lock(m_mutex);
        if(m_concurrency > 0u) {
            --m_concurrency;
            return;
        }
        m_waiters.push_back(std::make_pair(Scheduler::GetThis(), Fiber::GetThis())); // 如果大于0则减少并发度并直接返回；否则将当前协程加入等待队列，然后切换到其他协程执行。
    }
    Fiber::YieldToHold();    // 切换协程
}

void FiberSemaphore::notify() {
    MutexType::Lock lock(m_mutex);
    if(!m_waiters.empty()) {
        auto next = m_waiters.front();    //等待队列不为空，则取出下一个协程
        m_waiters.pop_front();
        next.first->schedule(next.second);
    } else {
        ++m_concurrency;    // 增加并发度
    }
}

}
