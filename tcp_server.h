//
// Created by Davydov Ivan on 09/06/2018.
//

#ifndef WEB_SERVER_FRAMEWORK_TCP_SERVER_H
#define WEB_SERVER_FRAMEWORK_TCP_SERVER_H

#endif //WEB_SERVER_FRAMEWORK_TCP_SERVER_H

#pragma once

#include <iostream>
#include <netinet/in.h>
#include <sys/event.h>
#include <unistd.h>
#include <pthread.h>
#include <string>
#include <sstream>
#include <vector>

inline void handle_error(const std::string &msg);

inline void receive_msg(uintptr_t sock_fd);

struct Queue {
    Queue();
    pthread_mutex_t lock;
    pthread_mutex_t cout_lock;
    void Lock();
    void Unlock();
    int kq;
};

class Thread {
public:
    void LockQueue();
    int TryLockQueue();
    void UnlockQueue();
    virtual void Work() = 0;
    Queue* queue_;
};

class WorkerThread : public Thread {
public:
    void Work() override;
private:
    struct kevent event_list[128]; //why the length is 32?
};

class ListenerThread : Thread {
public:
    ListenerThread(int port, Queue* queue);
    ~ListenerThread();
    void Work() override;
private:
    int sock_fd_;
    int port_;
};

inline void* start_worker(void* worker);

class TCPServer {
public:
    TCPServer(int thread_num, int port);
    void Start();
private:
    int port_;
    int thread_num_;
    Queue queue_;
    ListenerThread listener_;
    std::vector<WorkerThread> workers_;
    std::vector<pthread_t> tids_;
};