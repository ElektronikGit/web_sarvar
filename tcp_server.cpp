//
// Created by Davydov Ivan on 10/07/2018.
//

#include "tcp_server.h"



inline void handle_error(const std::string &msg) {
    std::cout << msg << std::endl;
    exit(EXIT_FAILURE);
}

inline void receive_msg(uintptr_t sock_fd) {
    pthread_t thread_id = pthread_self();

    char buffer[128];
    memset(&buffer, '\0', sizeof buffer);

    std::cout << thread_id << " thread receiving a message" << std::endl;

    if (recv(sock_fd, buffer, sizeof buffer, 0) == -1) {
        if (errno != EWOULDBLOCK) {
            handle_error("Receiving data from socket failed");
        }
        return;
    }

    std::cout << thread_id << " thread received message: " << (std::string) buffer << std::endl;

    if (strncmp(buffer, "exit", 4) == 0) {
        auto msg = "bye blyat!\n";
        send(sock_fd, msg, strlen(msg), 0);
        close(sock_fd);
        return;
    }

    std::stringstream ss;
    ss << thread_id;
    ss << " thread echoes ";
    ss << buffer;

    const std::string tmp = ss.str();
    const char* cstr = tmp.c_str();
    send(sock_fd, cstr, strlen(cstr), 0);
}



Queue::Queue() {
    if (pthread_mutex_init(&lock, NULL) < 0) {
        handle_error("Failed to initiate lock");
    }

    if (pthread_mutex_init(&cout_lock, NULL) < 0) {
        handle_error("Failed to initiate cout_lock");
    }

    kq = kqueue();
    if (kq < -1) {
        handle_error("Failed to create kqueue");
    }

    std::cout << "Kqueue created with id " << kq << std::endl;
}

void Queue::Lock() {
    pthread_mutex_lock(&cout_lock);
}

void Queue::Unlock() {
    pthread_mutex_unlock(&cout_lock);
}



void Thread::LockQueue() {
    pthread_mutex_lock(&queue_->lock);
//    std::cout << pthread_self() << " locked queue" << std::endl;
}

int Thread::TryLockQueue() {
    return pthread_mutex_trylock(&queue_->lock);
}

void Thread::UnlockQueue() {
    pthread_mutex_unlock(&queue_->lock);
//    std::cout << pthread_self() << " unlocked queue" << std::endl;
}



void WorkerThread::Work() {
    int events_num;
    struct kevent event;

    queue_->Lock();
    std::cout << pthread_self() << " working thread began to work. kq id is " << queue_->kq << std::endl;
    queue_->Unlock();

    while (true) {
        if (TryLockQueue() == 0) {
//            std::cout << "Queue locked by worker" << std::endl;
            struct timespec t;
            t.tv_nsec = 0;

            events_num = kevent(queue_->kq, NULL, 0, event_list, 128, &t);
            if (events_num < 1) {
//                handle_error("Selecting events failed");
            }
            UnlockQueue();

//            std::cout << "Queue unlocked by worker" << std::endl;

            for (int i = 0; i < events_num; i++) {
                event = event_list[i];

                if (event.flags == EV_EOF) {
                    uintptr_t sock_fd = event.ident;

                    EV_SET(&event, sock_fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);

                    LockQueue();
                    if (kevent(queue_->kq, &event, 1, NULL, 0, NULL) == -1) {
                        handle_error("Deleting event failed");
                    }
                    UnlockQueue();

                    close(sock_fd);
                } else { //if (event.flags == EVFILT_READ)
                    receive_msg(event.ident);
                }
            }
        }
    }
}



ListenerThread::ListenerThread(int port, Queue* queue) : port_(port) {
    queue_ = queue;
    std::cout << "ListenerThread initialised" << std::endl;
}

ListenerThread::~ListenerThread() {
    close(sock_fd_);
}

void ListenerThread::Work() {

    queue_->Lock();
    std::cout << pthread_self() << " listening thread began to work. kq id is " << queue_->kq << std::endl;
    queue_->Unlock();

    sockaddr_in addr;
    memset(&addr, 0, sizeof addr);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    sock_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    int enable = 1;
    if (setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        handle_error("setsockopt(SO_REUSEADDR) failed");
    if (sock_fd_ == -1) {
        handle_error("Failed ot create socket");
    }

    if (bind(sock_fd_, (sockaddr*) &addr, sizeof addr) == -1) {
        handle_error("Failed to bind socket");
    }

    if (listen(sock_fd_, 32) == -1) {
        handle_error("Listening socket failed");
    }

    int new_sock_fd;
    while(1) {
        new_sock_fd = accept(sock_fd_, NULL, NULL);
        if (new_sock_fd < 0) {
            handle_error("Accepting socket failed");
        }

        std::cout << "New file descriptor is " << new_sock_fd << std::endl;

        struct kevent new_conn;

        EV_SET(&new_conn, new_sock_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);

        LockQueue();
        std::cout << "trying to add event to kqueue " << queue_->kq << std::endl;
        if (kevent(queue_->kq, &new_conn, 1, NULL, 0, NULL) == -1) {
            handle_error("Adding new connection to kqueue failed");
        }
        UnlockQueue();
    }
}



inline void* start_worker(void* thread) {
    (static_cast<Thread*>(thread))->Work();

    return (void*)1;
}



TCPServer::TCPServer(int thread_num, int port)
: thread_num_(thread_num)
, port_(port)
, listener_(port_, &queue_)
, workers_(static_cast<u_long>(thread_num))
{
    for (auto& worker : workers_) {
        worker.queue_ = &queue_;
    }

    std::cout << "kq id is " << queue_.kq << std::endl;

    tids_ = std::vector<pthread_t>(thread_num_ + 1);
}

void TCPServer::Start() {
    std::cout << "trying to create listening thread" << std::endl;
    pthread_create(&(tids_[0]), NULL, start_worker, &listener_);
    std::cout << "Listening thread created" << std::endl;

    for (int i = 1; i < thread_num_; ++i) {
        pthread_create(&tids_[i], NULL, start_worker, &workers_[i - 1]);
    }

    while (true) {}
}
