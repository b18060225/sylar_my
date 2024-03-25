#include "thread.h"
#include "log.h"
#include "util.h"

namespace sylar {

static thread_local Thread* t_thread = nullptr;    //为什么类型设置为静态？唯一，同名的类的不同实例共享一个
static thread_local std::string t_thread_name = "UNKNOW";

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

Thread* Thread::GetThis() {
    return t_thread;
}

const std::string& Thread::GetName() {
    return t_thread_name;
}

void Thread::SetName(const std::string& name) {
    if(name.empty()) {
        return;
    }
    if(t_thread) {
        t_thread->m_name = name;
    }
    t_thread_name = name;
}

Thread::Thread(std::function<void()> cb, const std::string& name)
    :m_cb(cb)
    ,m_name(name) {
    if(name.empty()) {
        m_name = "UNKNOW";
    }
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);    //创建一个新的线程
    if(rt) {
        SYLAR_LOG_ERROR(g_logger) << "pthread_create thread fail, rt=" << rt
            << " name=" << name;
        throw std::logic_error("pthread_create error");
    }
    m_semaphore.wait();    // 等待新线程启动完成。这样做是为了确保新线程已经启动并且设置了线程的属性，然后主线程才继续执行。
}

Thread::~Thread() {
    if(m_thread) {
        pthread_detach(m_thread);//将线程标记为分离状态，这样在线程结束时，系统会自动回收其资源，而不需要其他线程调用 pthread_join() 来等待其结束。
        
    }
}

void Thread::join() {
    if(m_thread) {
        int rt = pthread_join(m_thread, nullptr);    //等待线程执行完成，并释放线程资源
        if(rt) {
            SYLAR_LOG_ERROR(g_logger) << "pthread_join thread fail, rt=" << rt
                << " name=" << m_name;
            throw std::logic_error("pthread_join error");
        }
        m_thread = 0;    //将成员变量 m_thread 置为 0，表示线程已经执行完成。
    }
}

void* Thread::run(void* arg) {
    Thread* thread = (Thread*)arg;
    t_thread = thread;
    t_thread_name = thread->m_name;
    thread->m_id = sylar::GetThreadId();
    pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

    std::function<void()> cb;
    cb.swap(thread->m_cb);    //进行交换。这样做是为了避免在执行线程函数时，其它线程对 m_cb 的修改带来的影响。

    thread->m_semaphore.notify();    // 通知等待的线程，表示当前线程已经启动

    cb();    // 执行线程操作
    return 0;
}

}
