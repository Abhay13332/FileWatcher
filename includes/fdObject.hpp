#ifndef FOROBJECT_HPP
#define FOROBJECT_HPP
#include <concepts>
#include <string>
#include <unistd.h>
#include <utility> 
namespace FileWatcherSystem{
    namespace epollFlags {
        class EpollModFlags;
    }
    class EpollMan;
    template<typename T>
    concept hasEpollSatisfyFunc=requires (T obj) {
        {obj.getEpollInfo()}->std::same_as<std::pair<int,epollFlags::EpollModFlags>>;
        
    };
    template<typename Derived>
    class EpollSatisfy{
        protected:
        EpollSatisfy(){
            static_assert(hasEpollSatisfyFunc<Derived>,"class do not implement getEpollInfo" ); 
        }
        public:
        EpollSatisfy(EpollSatisfy&)=delete;
        EpollSatisfy &operator=(EpollSatisfy&)=delete;
        EpollSatisfy(EpollSatisfy&&)=default;
        EpollSatisfy &operator=(EpollSatisfy&&)=default;


        
    };
    template<typename T>
    concept satistfyRdhup=requires (T obj) {
        {obj.isWriteComplete()}->std::same_as<bool>;
     
    };
    template<typename Derived>
    class EpollSatisfyRDH:public EpollSatisfy<Derived>{
        public:
        bool isEpollRdHup=false;
        EpollSatisfyRDH():EpollSatisfy<Derived>(){
            
        }
        EpollSatisfyRDH(EpollSatisfyRDH&&)=default;
        EpollSatisfyRDH &operator=(EpollSatisfyRDH&&)=default;
        bool getRDHupStatus()noexcept{
            return isEpollRdHup;
        }
        void setRDHupStatus(bool val){
            isEpollRdHup=val;
        }
    };
    enum class writeLevel: int8_t{
   PARTIAL,
   COMPLETE
};
    class Controller{
        protected:
        Controller()=default;
        virtual ~Controller();
        public:
        virtual std::string read()=0;
        virtual void write(std::string )=0;
    };
inline FileWatcherSystem::Controller::~Controller() = default;
class FileDesc{
    int fd=-1;
    public:
    FileDesc()=default;
    FileDesc(int fd)noexcept:fd(fd){}
    FileDesc(FileDesc&)=delete;
    FileDesc(FileDesc&&oth)noexcept:fd(oth.fd){
        oth.fd=-1;
    }
    FileDesc& operator=(FileDesc&)=delete;
    FileDesc& operator=(FileDesc&& oth)noexcept{
        if(this!=&oth){
            if (fd != -1) {
                close(fd); 
            }
            fd=oth.fd;
            oth.fd=-1;
        }
        return* this;
    };
    [[nodiscard]] explicit operator int()const{
        return fd;
    }
    [[nodiscard]] int get(){
        return fd;
    }
    ~FileDesc()noexcept{
        if (fd!=-1) {
         close(fd);
        }
    }


};


} 
#endif