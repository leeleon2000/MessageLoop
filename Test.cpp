
#include "SimpleMessageLoop.h"


int main()
{
  {
    using namespace sml;
    SimpleMessageLoop main_loop;
    SimpleMessageLoop thread_loop;
    Message msg;
    
    thread_loop.onStop([&main_loop](){
      main_loop.stop();
    });
    
    thread_loop.onHandle([&](Message msg) {
      const int data = *msg.dataAs<int>();
      printf("thread get data:%d\n", data);
      if(data >= 11){
        thread_loop.stop();
      }else{
        main_loop.postMessage(msg.dataFrom<int>(data + 1));
      }
    });
    
    std::thread thread1([&] {
      Message m;
      m.what_ = 0;
      m.dataFrom<int>(0);
      main_loop.postMessage(m, 1000);
      thread_loop.loop();
    });
    
    main_loop.onHandle([&thread_loop](Message msg){
      const int data = *msg.dataAs<int>();
      printf("main get data:%d\n", data);
      thread_loop.postMessage(msg.dataFrom<int>(data + 1));
    });
    
    main_loop.loop();
    thread1.join();
  }
  return 0;
}

