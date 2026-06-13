#ifndef UTIL_HPP
#define UTIL_HPP
#include<exception>
#include <functional>
#include <iostream>
#include <unistd.h>
#ifndef NDEBUG
    // In Debug builds, execute the function or statement directly
    #define DEBUG_RUN(...) do { __VA_ARGS__ } while(0)
#else
    // In Release builds, replace it with nothing (completely removed)
    #define DEBUG_RUN(...) do { } while(0)
#endif

namespace FileWatcherSystem {
namespace debug {

    template <typename... Args>
    static void print( Args&&... args) {
        DEBUG_RUN(
        ((std::cout << args), ...);
        std::cout << std::endl;// NOLINT(performance-avoid-endl)
        );
    }
  
   


}
class NonBlockReadError:public std::exception{
     public:
    const char* what() const noexcept override {
        return "operation would block or is not supported by the filesystem";
    }
};
class ScopeGuard{
    std::move_only_function<void()> action;
    bool dismissed = false;

  public:
   ScopeGuard(std::move_only_function<void()> cleanup):action(std::move(cleanup)){}
   void disable(){
    dismissed = true;

   }
   void enable(){
    dismissed=false;
   }
   ~ScopeGuard(){
      if (!dismissed ) {
            action(); // Runs ONLY if dismiss() was never hit (i.e., on error/early return)
        }
   }
};

template<class... Ts> struct Overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

}

#endif