#pragma once

#include <functional>
#include <chrono>
#include <memory>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace sml {
  
  class Message {
  public:
    int what_;
    int arg1_;
    int arg2_;
    std::shared_ptr<void> data_;
    
    Message() :what_(0), arg1_(0), arg2_(0) {
    }
    
    virtual ~Message() {
    }
    
    template<typename T>
    Message& dataFrom(T data) {
      data_ = std::static_pointer_cast<void>(std::make_shared<T>(std::move(data)));
      return *this;
    }
    
    template<typename T>
    std::shared_ptr<T> dataAs() {
      return std::static_pointer_cast<T>(data_);
    }
    
  };
  
  class SimpleMessageLoop
  {
    
  private:
    
    enum QueueMessageType { RUNNABLE, MESSAGE };
    
    struct QueueMessage {
      unsigned long long timestamp;
      Message message;
      QueueMessageType type;
    };
    
    std::function<void(Message)> message_handler_;
    std::function<void()> start_handler_;
    std::function<void()> stop_handler_;
    std::function<void()> loop_handler_;
    std::priority_queue <QueueMessage, std::vector<QueueMessage>, std::function<bool(const QueueMessage&, const QueueMessage&)>> queue_;
    std::atomic<bool> is_exit;
    std::mutex mutex_;
    std::condition_variable condition_variable_;
    
    
  public:
    SimpleMessageLoop() :queue_([](const QueueMessage &msg1, const QueueMessage &msg2) {return msg1.timestamp > msg2.timestamp; }), is_exit(false) {
    }
    
    ~SimpleMessageLoop() {
      clearQueue();
      message_handler_ = nullptr;
      start_handler_ = nullptr;
      stop_handler_ = nullptr;
      loop_handler_ = nullptr;
    }
    
    void loop() {
      
      is_exit.store(false);
      if (start_handler_) {
        start_handler_();
      }
      
      while (!is_exit.load()) {
        
        if (loop_handler_) {
          loop_handler_();
        }
        
        std::unique_lock<std::mutex> lk(mutex_);
        if (queue_.empty()) {
          condition_variable_.wait_for(lk, std::chrono::milliseconds(10));
          continue;
        }
        
        auto msg = queue_.top();
        if(msg.timestamp > getCurrentMilliseconds()){
          continue;
        }
        
        queue_.pop();
        lk.unlock();
        
        if (msg.type == MESSAGE) {
          if (message_handler_) {
            message_handler_(msg.message);
          }
        }
        else {
          auto f = msg.message.dataAs<std::function<void()>>().get();
          if (f && *f) {
            (*f)();
          }
        }
      }
      
      if (stop_handler_) {
        stop_handler_();
      }
    }
    
    void stop() {
      is_exit.store(true);
      clearQueue();
    }
    
    void postTask(std::function<void()> f, unsigned long delay_ms = 0) {
      
      Message m;
      m.dataFrom<std::function<void()>>(std::move(f));
      
      QueueMessage qmsg;
      qmsg.message = std::move(m);
      qmsg.timestamp = getCurrentMilliseconds() + delay_ms;
      qmsg.type = RUNNABLE;
      
      {
        std::lock_guard<std::mutex> lk(mutex_);
        if(is_exit.load()){
          return;
        }
        queue_.push(qmsg);
      }
      condition_variable_.notify_one();
      
    }
    
    void postMessage(Message msg, unsigned long delay_ms = 0) {
      
      QueueMessage qmsg;
      qmsg.message = std::move(msg);
      qmsg.timestamp = getCurrentMilliseconds() + delay_ms;
      qmsg.type = MESSAGE;
      
      {
        std::lock_guard<std::mutex> lk(mutex_);
        if(is_exit.load()){
          return;
        }
        queue_.push(qmsg);
      }
      condition_variable_.notify_one();
    }
    
    void onHandle(std::function<void(Message)> f) {
      message_handler_ = f;
    }
    
    void onStop(std::function<void()> f) {
      stop_handler_ = f;
    }
    
    void onStart(std::function<void()> f) {
      start_handler_ = f;
    }
    
    void onLoop(std::function<void()> f) {
      loop_handler_ = f;
    }
    
  private:
    static unsigned long long getCurrentMilliseconds() {
      return std::chrono::duration_cast<std::chrono::milliseconds>
      (std::chrono::system_clock::now().time_since_epoch()).count();
    }
    
    void clearQueue(){
      std::lock_guard<std::mutex> lk(mutex_);
      queue_ = {};
    }
    
  };
  
  
}

