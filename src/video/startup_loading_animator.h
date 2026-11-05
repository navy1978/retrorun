#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

// Native presenter only: stop/join before the core takes over video resources.
class StartupLoadingAnimator {
    std::mutex mutex_;
    std::condition_variable changed_;
    std::thread worker_;
    bool stopped_=true;
    std::atomic<bool> failed_{false};
    std::atomic<unsigned> frames_{0};
public:
    ~StartupLoadingAnimator(){stop();}
    void start(std::function<void()> draw,std::chrono::milliseconds interval=std::chrono::milliseconds(100)){
        stop();
        {std::lock_guard<std::mutex> lock(mutex_);stopped_=false;}
        failed_=false;frames_=0;
        worker_=std::thread([this,draw=std::move(draw),interval]{
            std::unique_lock<std::mutex> lock(mutex_);
            while(!changed_.wait_for(lock,interval,[this]{return stopped_;})){
                lock.unlock();
                try{draw();++frames_;}catch(...){failed_=true;return;}
                lock.lock();
            }
        });
    }
    void stop(){
        {std::lock_guard<std::mutex> lock(mutex_);stopped_=true;}
        changed_.notify_all();
        if(worker_.joinable())worker_.join();
    }
    bool failed()const{return failed_.load();}
    unsigned frames()const{return frames_.load();}
};
