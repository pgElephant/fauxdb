#pragma once
#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
using namespace std;
namespace FauxDB
{

class CThread
{
  public:
    CThread();
    ~CThread();
    bool start(function<void()> function);
    void stop();
    bool isRunning() const noexcept;
    void join();
    void detach();
    thread::id getId() const noexcept;
    bool isJoinable() const noexcept;
    static void setMaxThreads(size_t maxThreads);
    static size_t getActiveThreadCount();
    static void shutdownAll();

  private:
    thread thread_;
    atomic<bool> running_;
    function<void()> threadFunction_;
    static atomic<size_t> activeThreadCount_;
    static size_t maxThreads_;
    static mutex threadPoolMutex_;
};
} /* namespace FauxDB */
