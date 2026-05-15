#ifndef THREADTASK_H
#define THREADTASK_H

#include <thread>

class ThreadTask
{
public:
    virtual ~ThreadTask();

public:
    virtual void start();
    virtual void stop();

private:
    std::jthread thread;
};

#endif // THREADTASK_H