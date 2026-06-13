#ifndef SOCKET_HPP
#define SOCKET_HPP
#include <algorithm>
#include <array>
#include <expected>
#include <functional>
#include <iostream>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cerrno>
#include<stdexcept>
#include <sys/types.h>
#include <unistd.h>
#include<epoll.hpp>
#include<fdObject.hpp>
#include<util.hpp>
namespace FileWatcherSystem {
namespace socketIO{
    class SocketModFlags{    
        
        int val;
        public:
        static int getValue(const SocketModFlags& obj){return obj.val;}
           SocketModFlags(int val):val(val){}
           SocketModFlags operator|(const SocketModFlags& oth)const noexcept{
               return val|oth.val;
            }
           explicit operator int()const {
               return val;
           }
           bool operator==(const SocketModFlags& oth) const{
                return val==oth.val;
           }
        
        };
        using csSMF=const SocketModFlags;
        csSMF tcpSocket=SOCK_STREAM;
        csSMF udpSocket=SOCK_DGRAM;
        csSMF rawSocket=SOCK_RAW;
        csSMF nonBlocking=SOCK_NONBLOCK;
        csSMF closeOnExec=SOCK_CLOEXEC;

        csSMF localCommunication=AF_UNIX;
        csSMF ipv4Communication=AF_INET;
        csSMF ipv6Communication=AF_INET6;
}

class SocketClient:public  EpollSatisfyRDH<SocketClient>{
  FileDesc clientfd = -1;
  struct sockaddr_in clientAddr;
  friend class SkSubscriberController; 
public:
   SocketClient(int clientfd,sockaddr_in clientAddr):EpollSatisfyRDH<SocketClient>(),clientfd(clientfd),clientAddr(clientAddr){}

  std::pair<int,epollFlags::EpollModFlags> getEpollInfo(){
      return {clientfd.get(),epollFlags::forEdgeTriggered};
  }
  
  std::string ReadDataBuff;

  bool read(int pos=-1){
       std::array<char, 4096> buffer;
       ssize_t bytesRead;
       while(( bytesRead=::read(clientfd.get(), buffer.data(),buffer.size()))>0){
        ReadDataBuff.append(buffer.data(),bytesRead);
       };
       if(bytesRead==-1 && (errno!=EWOULDBLOCK && errno!=EAGAIN)){
           switch (errno) {
            case EINTR:
                // System call was interrupted by a signal before any data was read.
                // Depending on your architecture, you might want to retry instead of throwing.
                throw std::runtime_error("Read interrupted by signal: " + std::string(std::to_string(errno)));

            case ECONNRESET:
                // The remote peer forced a connection close (e.g., crashed or sent a RST packet).
                throw std::runtime_error("Connection reset by peer: " + std::string(std::to_string(errno)));

            case ETIMEDOUT:
                // Transmission timeout occurred on the underlying network connection.
                throw std::runtime_error("Connection timed out: " + std::string(std::to_string(errno)));

            case EBADF:
                // clientfd is not a valid file descriptor or is not open for reading.
                throw std::runtime_error("Invalid or closed file descriptor: " + std::string(std::to_string(errno)));

            case EFAULT:
                // buffer.data() points outside your accessible address space.
                throw std::runtime_error("Buffer memory fault: " + std::string(std::to_string(errno)));

            case EINVAL:
                // The file descriptor is unsuitable for reading.
                throw std::runtime_error("Invalid argument for read operation: " + std::string(std::to_string(errno)));

            case EIO:
                // Low-level I/O error occurred.
                throw std::runtime_error("Physical I/O error: " + std::string(std::to_string(errno)));

            default:
                // Fallback for any other system error.
                throw std::runtime_error("Fatal read error: " + std::string(std::to_string(errno)));
        }
       }
       if(ReadDataBuff.size()>0 ){

           return true;
       }
       return false;

  }
  std::string WriteDataBuffremain;
    bool isWriteComplete(){
     if(WriteDataBuffremain.size()==0) return true;
     return false;

  }
  std::expected<void , NonBlockReadError>write(){
    debug::print(WriteDataBuffremain);
    if(WriteDataBuffremain.length()==0){
        return {};
    }
    int pos=0;
    ssize_t bytesWrite=0;
    std:std::string_view contentView(WriteDataBuffremain);
    while(pos+bytesWrite<WriteDataBuffremain.size() && (bytesWrite=::write(clientfd.get(), contentView.substr(pos).data(), contentView.substr(pos).size()))>0 ){
        pos+=bytesWrite;
    }
    if(bytesWrite==-1 && (errno==EWOULDBLOCK || errno==EAGAIN) && pos!=WriteDataBuffremain.length()){
        WriteDataBuffremain=WriteDataBuffremain.substr(pos);
        return std::unexpected(NonBlockReadError());
    }
    if(bytesWrite==-1){
        switch (errno) {
        // Recoverable or signal-based write interruptions
        case EINTR:        // The write was interrupted by a signal before any data was written
            throw std::runtime_error("Write interrupted by signal: " + std::string(std::to_string(errno)));

        // Connection failure states
        case EPIPE:        // The reading side of the socket has closed (causes a SIGPIPE signal unless masked)
        case ECONNRESET:   // The connection was forcibly reset by the remote peer
            throw std::runtime_error("Broken pipe or connection reset: " + std::string(std::to_string(errno)));

        case ETIMEDOUT:    // The underlying network connection timed out during transmission
            throw std::runtime_error("Write operation timed out: " + std::string(std::to_string(errno)));

        // Resource and boundary failures
        case ENOSPC:       // The device containing the file system or socket buffer has no room
            throw std::runtime_error("No space left on device/buffer: " + std::string(std::to_string(errno)));

        // Invalid file descriptor or state errors
        case EBADF:        // clientfd is not a valid open file descriptor for writing
            throw std::runtime_error("Invalid or closed write file descriptor: " + std::string(std::to_string(errno)));

        case EINVAL:       // File descriptor is unsuitable for writing or invalid alignment
            throw std::runtime_error("Invalid argument for write operation: " + std::string(std::to_string(errno)));

        case EFAULT:       // Buffer points outside your accessible address space
            throw std::runtime_error("Memory fault in write buffer pointer: " + std::string(std::to_string(errno)));

        // Catch-all for other rare system errors
        default:
            throw std::runtime_error("Fatal write error: " + std::string(std::to_string(errno)));
    }
    }
    WriteDataBuffremain.clear();
    return {};

  };
  std::expected<void , NonBlockReadError> write(const std::string& content){
    std::expected<void , NonBlockReadError> result=write();
    if(!result.has_value()){
         WriteDataBuffremain.append(content);
         return std::unexpected(NonBlockReadError());
    }
    int pos=0;
    ssize_t bytesWrite=0;
    std:std::string_view contentView(content);
    while((bytesWrite=::write(clientfd.get(), contentView.substr(pos).data(), contentView.substr(pos).size()))>0 && pos+bytesWrite<content.size()){
        pos+=bytesWrite;
    }
    if(bytesWrite==-1 && (errno==EWOULDBLOCK &&errno==EAGAIN) && pos!=content.length()){
        WriteDataBuffremain=content.substr(pos);
         return std::unexpected(NonBlockReadError());

    }
       if(bytesWrite==-1){
        switch (errno) {
        // Recoverable or signal-based write interruptions
        case EINTR:        // The write was interrupted by a signal before any data was written
            throw std::runtime_error("Write interrupted by signal: " + std::string(std::to_string(errno)));

        // Connection failure states
        case EPIPE:        // The reading side of the socket has closed (causes a SIGPIPE signal unless masked)
        case ECONNRESET:   // The connection was forcibly reset by the remote peer
            throw std::runtime_error("Broken pipe or connection reset: " + std::string(std::to_string(errno)));

        case ETIMEDOUT:    // The underlying network connection timed out during transmission
            throw std::runtime_error("Write operation timed out: " + std::string(std::to_string(errno)));

        // Resource and boundary failures
        case ENOSPC:       // The device containing the file system or socket buffer has no room
            throw std::runtime_error("No space left on device/buffer: " + std::string(std::to_string(errno)));

        // Invalid file descriptor or state errors
        case EBADF:        // clientfd is not a valid open file descriptor for writing
            throw std::runtime_error("Invalid or closed write file descriptor: " + std::string(std::to_string(errno)));

        case EINVAL:       // File descriptor is unsuitable for writing or invalid alignment
            throw std::runtime_error("Invalid argument for write operation: " + std::string(std::to_string(errno)));

        case EFAULT:       // Buffer points outside your accessible address space
            throw std::runtime_error("Memory fault in write buffer pointer: " + std::string(std::to_string(errno)));

        // Catch-all for other rare system errors
        default:
            throw std::runtime_error("Fatal write error: " + std::string(std::to_string(errno)));
    }
    }
    WriteDataBuffremain.clear();
    return {};
     
  }



  SocketClient(SocketClient &) = delete;
  SocketClient &operator=(SocketClient &) = delete;
  SocketClient(SocketClient &&) = default;
  SocketClient &operator=(SocketClient &&) = default;
  int getFd() { return clientfd.get(); }
};
class NBTcpSocket;
struct EpollSocketDeleter {
    void operator()(EpollEvent* event) const {
        if (event) {
            auto clientData =getDataEpollEvent<SocketClient>(event);
            delete clientData;
            delete event; 
        }
    }
};
class SkSubscriberController:public Controller{
     
     std::vector<std::unique_ptr<EpollEvent,EpollSocketDeleter>> subs;
     NBTcpSocket& socket;
    public:
      SkSubscriberController(NBTcpSocket& socket,int defaultClientCount=10):Controller(),socket(socket){
        subs.reserve(defaultClientCount);
      }
      ~SkSubscriberController()override=default;
      
      auto getClientView(){
         return   subs | std::views::transform([](const std::unique_ptr<EpollEvent,EpollSocketDeleter>& cl) {
             return std::pair<SocketClient*,EpollEvent*>(getDataEpollEvent<SocketClient>(cl.get()),cl.get());
        });
      }
      void addsub(EpollEvent* event){
            subs.emplace_back(event);
      };
      void deletesub(EpollEvent* event){
        subs.erase(std::remove_if(subs.begin(),subs.end(),[event](const std::unique_ptr<EpollEvent,EpollSocketDeleter>& cl){
            if(cl->getFd()==event->getFd()){
                return true;
            }
         return false;
        }),subs.end());
      }

      
      void writeToclients(std::move_only_function<void(SocketClient*,EpollEvent*)> whenBlock){
         for(auto [cl,event]:getClientView()){
           if(!cl->write()){
              whenBlock(cl,event); 
           }; 
        }
      }
      std::string read()override{
            std::runtime_error("should not read from subs");
            return "";
      }
      void write(std::string data)override{
       

           for(auto [cl,event]:getClientView()){
            cl->WriteDataBuffremain+=data;
          }
        //  client->WriteDataBuffremain+=data;
      }
};
class NBTcpSocket:public EpollSatisfy<NBTcpSocket>{
    friend class SkSubscriberController;
    FileDesc socketFd;
    sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[4096];

    
public:
    NBTcpSocket(int port,int maxClients=10):EpollSatisfy<NBTcpSocket>(){
        using namespace socketIO;
        socketFd=socket(SocketModFlags::getValue(ipv4Communication),SocketModFlags::getValue(tcpSocket|nonBlocking), 0);
        if(socketFd.get()==-1){
            switch(errno) {
                case EACCES:
                    throw std::runtime_error("Permission denied");
                case EAFNOSUPPORT:
                    throw std::runtime_error("The specified address family is not supported");
                case EINVAL:
                    throw std::runtime_error("Invalid argument passed to socket creation");
                case EMFILE:
                    throw std::runtime_error("The process has reached its limit of open file descriptors");
                case ENFILE:
                    throw std::runtime_error("The system-wide limit on the total number of open files has been reached");
                case ENOBUFS:
                    throw std::runtime_error("Insufficient buffer space available");
                case ENOMEM:
                    throw std::runtime_error("Insufficient memory available");
                default:
                    throw std::runtime_error("Unknown error occurred while creating socket: " + std::to_string(errno));
               }
        }
        int opt = 1;
        if(0>setsockopt(socketFd.get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))){
            switch (errno) {
        case EBADF:
            throw std::runtime_error("setsockopt failed (EBADF): Invalid file descriptor.");
        case ENOTSOCK:
            throw std::runtime_error("setsockopt failed (ENOTSOCK): Descriptor is not a socket.");
        case EINVAL:
            throw std::runtime_error("setsockopt failed (EINVAL): Invalid option length or level.");
        case EFAULT:
            throw std::runtime_error("setsockopt failed (EFAULT): Invalid memory address space for opt.");
        case ENOPROTOOPT:
            throw std::runtime_error("setsockopt failed (ENOPROTOOPT): Unknown or unsupported protocol option.");
        case ENOMEM:
            throw std::runtime_error("setsockopt failed (ENOMEM): Insufficient system memory.");
        default:
            throw std::runtime_error("setsockopt failed with unexpected OS error: " + std::string(std::to_string(errno)));
          }
        }
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);
        if (bind(socketFd.get(), (struct sockaddr *)&address, sizeof(address)) < 0) {
                switch (errno) {
        case EADDRINUSE:
            throw std::runtime_error("bind failed (EADDRINUSE): Local address/port already in use.");
        case EACCES:
            throw std::runtime_error("bind failed (EACCES): Access denied. Port requires root privileges.");
        case EADDRNOTAVAIL:
            throw std::runtime_error("bind failed (EADDRNOTAVAIL): Specified interface address is not available.");
        case EBADF:
            throw std::runtime_error("bind failed (EBADF): The socketFd is not a valid descriptor.");
        case ENOTSOCK:
            throw std::runtime_error("bind failed (ENOTSOCK): The descriptor socketFd is not a socket.");
        case EINVAL:
            throw std::runtime_error("bind failed (EINVAL): Socket is already bound to an address.");
        case EFAULT:
            throw std::runtime_error("bind failed (EFAULT): Address points outside your accessible address space.");
        case ENAMETOOLONG:
            throw std::runtime_error("bind failed (ENAMETOOLONG): Unix domain socket path name is too long.");
        case ENOENT:
            throw std::runtime_error("bind failed (ENOENT): Component of path prefix does not exist.");
        case ENOTDIR:
            throw std::runtime_error("bind failed (ENOTDIR): Component of path prefix is not a directory.");
        case EROFS:
            throw std::runtime_error("bind failed (EROFS): Socket inode would reside on a read-only file system.");
        default:
            throw std::runtime_error("bind failed with unexpected OS error: " + std::string(std::to_string(errno)));
    }
        }
        if(listen(socketFd.get(),maxClients)<0){
             switch (errno) {
        case EADDRINUSE:
            throw std::runtime_error("listen failed (EADDRINUSE): Another socket is already listening on the same port.");
        case EBADF:
            throw std::runtime_error("listen failed (EBADF): The socketFd is not a valid descriptor.");
        case ENOTSOCK:
            throw std::runtime_error("listen failed (ENOTSOCK): The descriptor socketFd is not a socket.");
        case EOPNOTSUPP:
            throw std::runtime_error("listen failed (EOPNOTSUPP): The socket type does not support the listen operation.");
        case EINVAL:
            throw std::runtime_error("listen failed (EINVAL): The socket is already connected or invalid.");
        case ENOBUFS:
            throw std::runtime_error("listen failed (ENOBUFS): Insufficient system resources available to queue connections.");
        default:
            throw std::runtime_error("listen failed with unexpected OS error: " + std::string(std::to_string(errno)));
    }
        }
        printf("Server listening on port %d\n", port);



    };
    NBTcpSocket(NBTcpSocket&)=delete;
    NBTcpSocket& operator=(NBTcpSocket&)=delete;
    NBTcpSocket(NBTcpSocket&&)=default;
    NBTcpSocket& operator=(NBTcpSocket&&)=default;
    std::pair<int,epollFlags::EpollModFlags> getEpollInfo(){
      return {socketFd.get(),epollFlags::forEdgeTriggered};
  }

    std::expected<SocketClient,NonBlockReadError> getClient(){
         struct sockaddr_in clientAddr;
         
         socklen_t clientLen = sizeof(clientAddr);
         int clientfd=accept4(socketFd.get(),(struct sockaddr *)&clientAddr, &clientLen, static_cast<int>(socketIO::nonBlocking|socketIO::closeOnExec));
         
         if(clientfd==-1 && (errno==EWOULDBLOCK || errno==EAGAIN)){
             return std::unexpected(NonBlockReadError());
            }
           
        if (clientfd == -1) {
    switch (errno) {
        // Interrupted or aborted connections
        case EINTR:        // System call was interrupted by a signal before a connection arrived
        case ECONNABORTED: // A connection was aborted by the client before being accepted
            throw std::runtime_error("Connection interrupted or aborted: " + std::string(std::to_string(errno)));

        // Process or system resource limits
        case EMFILE:       // Per-process limit on open file descriptors reached
        case ENFILE:       // System-wide limit on total number of open files reached
            throw std::runtime_error("File descriptor limit reached: " + std::string(std::to_string(errno)));

        // Memory limitations
        case ENOBUFS:      // Not enough free memory to allocate network buffers
        case ENOMEM:       // Out of memory
            throw std::runtime_error("System memory exhausted: " + std::string(std::to_string(errno)));

        // Invalid socket descriptors or states
        case EBADF:        // socket_fd is not a valid open file descriptor
        case ENOTSOCK:     // socket_fd descriptor references a file, not a socket
        case EOPNOTSUPP:   // The referenced socket type is not supported (e.g., not SOCK_STREAM)
        case EINVAL:       // Invalid flags passed or socket is not currently listening
            throw std::runtime_error("Invalid socket configuration: " + std::string(std::to_string(errno)));

        // Security and memory fault restrictions
        case EFAULT:       // client_addr pointer is outside the accessible address space
            throw std::runtime_error("Memory fault in client address pointer: " + std::string(std::to_string(errno)));
        case EPERM:        // Firewall rules or local security policies block the connection
            throw std::runtime_error("Permission denied by firewall: " + std::string(std::to_string(errno)));

        // Catch-all for unhandled exceptions
        default:
            throw std::runtime_error("Fatal connection acceptance error: " + std::string(std::to_string(errno)));
    }

        }
         
        return SocketClient(clientfd,clientAddr);

    }
    int getFd(){
        return socketFd.get();
    }
  
};
  
}
#endif