#ifndef LOCK_H_INCLUDED
#define LOCK_H_INCLUDED

#include <shared_mutex>

class RWLock
{
private:
    std::shared_mutex m_mutex;
public:
    RWLock(const RWLock&) = delete;
    RWLock& operator=(const RWLock&) = delete;
    explicit RWLock(bool /*writeFirst*/ = true) {}
    virtual ~RWLock() = default;
    void readLock()
    {
        m_mutex.lock_shared();
    }
    void readUnlock()
    {
        m_mutex.unlock_shared();
    }
    void writeLock()
    {
        m_mutex.lock();
    }
    void writeUnlock()
    {
        m_mutex.unlock();
    }
};

#endif //LOCK_H_INCLUDED
