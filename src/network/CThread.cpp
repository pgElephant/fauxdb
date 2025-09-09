/*-------------------------------------------------------------------------
 *
 * CThread.cpp
 *      Thread management utilities for FauxDB.
 *      Part of the FauxDB MongoDB-compatible database server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */


#include "CThread.hpp"

#include <chrono>
#include <stdexcept>
#include <thread>

using namespace std;
using namespace std::chrono;

using namespace FauxDB;

atomic<size_t> CThread::activeThreadCount_(0);
size_t CThread::maxThreads_(100);
mutex CThread::threadPoolMutex_;

CThread::CThread()
{
    running_ = false;
}

CThread::~CThread()
{
    stop();
}

bool CThread::start(function<void()> function)
{
    if (running_)
        return false;
    try
    {
        threadFunction_ = function;
        thread_ = thread(
            [this]()
            {
                if (threadFunction_)
                    threadFunction_();
            });
        running_ = true;
        return true;
    }
    catch (...)
    {
        running_ = false;
        return false;
    }
}

void CThread::stop()
{
    if (running_ && thread_.joinable())
    {
        running_ = false;
        thread_.join();
    }
}

void CThread::join()
{
    if (thread_.joinable())
        thread_.join();
}

bool CThread::isRunning() const noexcept
{
    return running_;
}

thread::id CThread::getId() const noexcept
{
    return thread_.get_id();
}

bool CThread::isJoinable() const noexcept
{
    return thread_.joinable();
}

void CThread::detach()
{
    if (thread_.joinable())
        thread_.detach();
}

void CThread::setMaxThreads(size_t maxThreads)
{
    maxThreads_ = maxThreads;
}

size_t CThread::getActiveThreadCount()
{
    return activeThreadCount_.load();
}

void CThread::shutdownAll()
{
}
