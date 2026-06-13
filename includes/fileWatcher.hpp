#ifndef WATCHER_HPP
#define WATCHER_HPP
#include "fdObject.hpp"
#include <cerrno>
#include <cstdio>
#include <errno.hpp>
#include <expected>
#include <fcntl.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/inotify.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include<sys/epoll.h>
#include<epoll.hpp>
#include<util.hpp>
#include <vector>

namespace FileWatcherSystem {

namespace ios {
class OpenMode {
  int mode = -1;

public:
  OpenMode(int mode) : mode(mode) {}
  OpenMode operator|(const OpenMode& oth)const noexcept {
    return OpenMode(mode | oth.mode);
  }
  static int getValue(const OpenMode& val) { return val.mode; }
};

class OpenFlags {
  int flag = -1;
  
  public:
  OpenFlags(int mode) : flag(mode) {}
  OpenFlags operator|(const OpenFlags& oth)const noexcept {
    return OpenFlags(flag | oth.flag);
  }
  static int getValue(const OpenFlags& val) { return val.flag; }
};

using csOF = const OpenFlags;

const static csOF read = O_RDONLY;
const static csOF write = O_WRONLY;
const static csOF readWrite = O_RDWR;
static csOF closeOnExec = O_CLOEXEC;
static csOF createIFNotExist = O_CREAT;
static csOF errorIfExists = O_EXCL;
static csOF nonBlocking = O_NONBLOCK;
namespace FlushOnWrite {
static csOF low = O_DSYNC;
static csOF mid = O_SYNC;
static csOF high = O_RSYNC;
} // namespace FlushOnWrite
static csOF flushtoDiskOnWrite = O_SYNC; // OR_SYNC O_DSYNC
static csOF tmpFile = O_TMPFILE;
static csOF pathMustBeDir = O_DIRECTORY;
/*
in filesystem directFileaccess remove the need of kernal it directly controlled
by program but there are several disadvanges also 1.need to caching by program
itself for fast read otherwise it is slower as kernal cache it for you if not
used 2.write are faster but program need to see
3. need to make memory alginment proerly otherwise write will fail
4.if we write on same memory block multiple times it does not edit storage one
time ,it do it each time. advantages: 1.write will give error on access no false
success(like in kernal caching it may happen that write do not happen now it
will happen later,you get error on fsync or on close ()) 2.
*/
static csOF directFileAccess = O_DIRECT; //

} // namespace ios

class WriteObj{
  friend class File;
  std::string buffer;
  int pos=0;
  std::string_view getBuffer(){
  return std::string_view(buffer).substr(pos);
  }
public:
  WriteObj(const std::string& data):buffer(data){}
  void movepos(int off) noexcept{
    pos+=off;
  }
  bool isComplete() const noexcept{
    return pos==buffer.length();
  }

};
class File {
  FileDesc fileFd;
  std::filesystem::path path;
  int flags;
  int mode;

public:
  File(const std::string& path, ios::csOF flags, ios::OpenMode mode)
      : path(path), flags(ios::OpenFlags::getValue(flags)),
        mode(ios::OpenMode::getValue(mode)) {
    // fd=open(path,)//
    // auto ty=std::ios::app;
   
    debug::print("path",path);
    fileFd = openat(AT_FDCWD,path.c_str(), this->flags, this->mode);
    if (fileFd.get() == -1) {
      switch (errno) {
      case EACCES: {
        if (this->flags & O_CREAT) {
          throw std::runtime_error(
              "access not allowed or directory or file already exists");
        } else {
          throw std::runtime_error("file permission denied");
        }
      }
      case EBADF:
        throw std::runtime_error("path is relative but relative dir is broken");
      case EDQUOT:
        throw std::runtime_error("disk space exhausted for inodes");
      case EFAULT:
        throw std::runtime_error(
            "path points outside your accessible address space");

      case EINVAL:
        throw std::runtime_error(
            "invalid flags value, unsupported O_DIRECT/O_TMPFILE combination, "
            "or invalid basename character");
      case EISDIR:
        throw std::runtime_error(
            "path refers to a directory and write access was requested");
      case ELOOP:
        throw std::runtime_error(
            "too many symbolic links were encountered in resolving path");
      case ENAMETOOLONG:
        throw std::runtime_error("the file path or name is too long");
      case ENFILE:
        throw std::runtime_error("the system-wide limit on the total number of "
                                 "open files has been reached");
      case ENOENT:
        throw std::runtime_error("the file or a component of the directory "
                                 "path does not exist, or is a dangling link");
      case ENOMEM:
        throw std::runtime_error("insufficient kernel memory was available");
      case ENXIO:
        throw std::runtime_error(
            "request involves an unopen FIFO pipe, nonexistent device special "
            "file, or a UNIX domain socket");
      case EWOULDBLOCK:
        throw std::runtime_error(
            "operation would block or is not supported by the filesystem");
      default:
        throw std::runtime_error("unknown error occurred while opening file: " +
                                 std::to_string(errno));
      }
    } 

  }
  
  int pos=0;
  std::string_view getPath() const noexcept{
    return path.native();
  }
  void moveoff(int off){
    resetErrno();
    int res=lseek(fileFd.get(),off, SEEK_CUR);
    if(res==-1){
        if(errno==ENXIO){
         throw std::runtime_error("offset is beyond the scope of eof");
        }else{
          throw std::runtime_error("unknown error occurred while moving file pointer: " + std::to_string(errno));
        }
    }
    pos=res;

  }
  void moveto(int newpos){
    int res=lseek(fileFd.get(),newpos, SEEK_SET);
    if(res==-1){
        if(errno==ENXIO){
         throw std::runtime_error("offset is beyond the scope of eof");
        }else{
          throw std::runtime_error("unknown error occurred while moving file pointer: " + std::to_string(errno));
        }
    }
    pos=res;
  }
  
  std::expected<std::string,NonBlockReadError> read(int pos=-1){
    if(mode& O_RDONLY || mode & O_RDWR){
      char buffer[2048];
      ssize_t bytesRead = ::read(fileFd.get(), buffer, sizeof(buffer));
      if(bytesRead == -1 && (flags & O_NONBLOCK) && (errno == EAGAIN || errno == EWOULDBLOCK)){
        return std::unexpected(NonBlockReadError());
      }
      if (bytesRead == -1) {
        switch (errno) {
          case EBADF:
            throw std::runtime_error("invalid file descriptor");
          case EFAULT:
            throw std::runtime_error("buffer is outside your accessible address space");
          case EINVAL:
            throw std::runtime_error("invalid file offset or buffer size");
          case EIO:
            throw std::runtime_error("I/O error occurred while reading"); 
  }
  } 
  if(bytesRead == 0){
     //need to implement eof handling in caller function
  }  pos+=bytesRead;
     return std::string(buffer, bytesRead);
}else{
  throw std::runtime_error("file not opened in read mode");
}

}
std::expected<WriteObj,NonBlockReadError> write(const std::string_view content){
     if(mode &O_WRONLY || mode & O_RDWR){
         size_t bytesWritten=::write(fileFd.get(), content.data(), content.length());
         if(bytesWritten==-1){
            if((flags & O_NONBLOCK) && (errno == EAGAIN || errno == EWOULDBLOCK)){
                return std::unexpected(NonBlockReadError());
            }
            switch (errno) {
              case EBADF:
                throw std::runtime_error("invalid file descriptor");
              case EFAULT:
                throw std::runtime_error("buffer is outside your accessible address space");
              case EINVAL:
                throw std::runtime_error("invalid file offset or buffer size");
              case EIO:
                throw std::runtime_error("I/O error occurred while writing"); 
            }
         }
         pos+=bytesWritten;
         if(bytesWritten < content.length()){
            return WriteObj(std::string(content.substr(bytesWritten)));
         }
         return WriteObj(std::string());

     }else{
      throw std::runtime_error("no write access");
     }
}
std::expected<void,NonBlockReadError> write( WriteObj& content){
    if(mode &O_WRONLY || mode & O_RDWR){
         size_t bytesWritten=::write(fileFd.get(), content.getBuffer().data(), content.getBuffer().length());
         if(bytesWritten==-1){
            if((flags & O_NONBLOCK) && (errno == EAGAIN || errno == EWOULDBLOCK)){
                return std::unexpected(NonBlockReadError());
            }
            switch (errno) {
              case EBADF:
                throw std::runtime_error("invalid file descriptor");
              case EFAULT:
                throw std::runtime_error("buffer is outside your accessible address space");
              case EINVAL:
                throw std::runtime_error("invalid file offset or buffer size");
              case EIO:
                throw std::runtime_error("I/O error occurred while writing"); 
            }
         }
         pos+=bytesWritten;

          content.movepos(bytesWritten);
        

     }else{
      throw std::runtime_error("no write access");
     }
     return {};
}


  File(File &) = delete;
  File &operator=(File &) = delete;
  File(File &&) = default;
  File &operator=(File &&) = default;

  int getFd() { return fileFd.get(); }
};
namespace Watchfs {
  class FileWatchFlags;
}
inline uint32_t operator&(uint32_t lhs, Watchfs::FileWatchFlags rhs);
inline uint32_t operator&=(uint32_t& lhs, Watchfs::FileWatchFlags rhs);
namespace Watchfs {
    constexpr  int inotifyEventSize = sizeof(struct inotify_event);
    constexpr static int bufferLen = 1024 * (inotifyEventSize + 16);
    class FileWatchFlags{
      friend inline uint32_t FileWatcherSystem::operator&(uint32_t lhs, Watchfs::FileWatchFlags rhs);
      friend inline uint32_t FileWatcherSystem::operator&=(uint32_t& lhs, Watchfs::FileWatchFlags rhs); 
      const int val=-1;
      public:
      constexpr FileWatchFlags(int val):val(val){}
      constexpr FileWatchFlags operator|(const FileWatchFlags&oth)const {
        return this->val|oth.val;
      }
    constexpr static int getValue(const FileWatchFlags& flags) { return flags.val; }

    };
    using csFW=const FileWatchFlags;
    namespace file {
    
      static constexpr csFW opende=IN_OPEN;
      static constexpr csFW accessForRead=IN_ACCESS;
      static constexpr csFW modified=IN_MODIFY;
      static constexpr csFW closed=IN_CLOSE;
      static constexpr csFW fileDeleted=IN_DELETE_SELF;
    }
    namespace dir{

      static constexpr csFW fileCreated=IN_CREATE;
      static constexpr csFW fileMovedout=IN_MOVED_FROM;
      static constexpr csFW fileMovedin=IN_MOVED_TO;
      static constexpr csFW filedeleted=IN_DELETE;
      static constexpr csFW deleted=IN_DELETE_SELF;
      static constexpr csFW isDir=IN_ISDIR;
    }
    namespace misc{

      static constexpr csFW readOnlyOnce=IN_ONESHOT;
      static constexpr csFW updateMaskOnDupl=IN_MASK_ADD;
      static constexpr csFW onlyDir=IN_ONLYDIR;

      
    }
    namespace init{

      static constexpr csFW nonBLockMode=IN_NONBLOCK;
      static constexpr csFW closeOnExec=IN_CLOEXEC;
    }
  constexpr std::string_view dirflagToDescription(csFW flag) {
    switch (csFW::getValue(flag)) {
        case IN_CREATE:      return "File or directory was created in the watched directory";
        case IN_MOVED_FROM:  return "File or directory was moved out of the watched directory";
        case IN_MOVED_TO:    return "File or directory was moved into the watched directory";
        case IN_DELETE:      return "File or directory was deleted from the watched directory";
        case IN_DELETE_SELF: return "The watched file or directory itself was deleted";
        case IN_ISDIR:       return "The target of the event is a directory";
        default:             return "Unknown file watcher event flag";
    }
}
constexpr std::string_view fileFlagToDescription(csFW flag) {
    switch (csFW::getValue(flag)) {
        case IN_OPEN:        return "File or directory was opened";
        case IN_ACCESS:      return "File was accessed for reading";
        case IN_MODIFY:      return "File was modified";
        case IN_CLOSE:       return "File or directory was closed";
        case IN_DELETE_SELF: return "The watched file or directory itself was deleted";
        default:             return "Unknown file operation event";
    }
}


    
}
inline uint32_t operator&(uint32_t lhs, Watchfs::FileWatchFlags rhs){
  return lhs & rhs.val;
};
inline uint32_t operator&=(uint32_t &lhs, Watchfs::FileWatchFlags rhs){
  return lhs &= rhs.val;
};

class Folder{
    std::filesystem::path path ;
    public:
  
    Folder(const std::string& path):path(path){}
    std::string_view getPath(){
      return path.native();
    }
    Folder(Folder&)=delete;
    Folder& operator=(Folder&)=delete;
    Folder(Folder&&)=default;
    Folder& operator=(Folder&&)=default;


};
class Watcher :public EpollSatisfy<Watcher>{
  FileDesc inotifyfd;
  std::unordered_map<int, File> filemap;
  std::unordered_map<int, Folder> foldermap;
  std::vector<char> buffer;
  public:
  Watcher(Watchfs::csFW flags):EpollSatisfy<Watcher>(),buffer(std::vector<char>(Watchfs::bufferLen)) {
    int res = inotify_init1(Watchfs::csFW::getValue(flags));
    if (res == -1) {

      switch (errno) {
      case EINVAL: {
        throw std::runtime_error("An invalid value was specified in flags");
      }
      case EMFILE: {
        throw std::runtime_error("An invalid value was specified in flags");
      }
      case ENFILE: {
        throw std::runtime_error("An invalid value was specified in flags");
      }
      case ENOMEM: {
        throw std::runtime_error("An invalid value was specified in flags");
      }
      default:
      }
    }
    inotifyfd = res;
    
  }
  std::pair<int,epollFlags::EpollModFlags> getEpollInfo(){
      return {inotifyfd.get(),epollFlags::forEdgeTriggered};
  }

  Watcher(Watcher &) = delete;
  Watcher &operator=(Watcher &) = delete;
  Watcher(Watcher &&) = default;
  Watcher &operator=(Watcher &&) = default;
 
 
  bool haslareadyFile(const std::string& path){
   for(auto& [watchDescr,file]:filemap){
      if(file.getPath()==path){
         return true;
      }
    }
    return false;

  }
   bool haslareadyFolder(const std::string& path){
   for(auto& [watchDescr,folder]:foldermap){
      if(folder.getPath()==path){
         return true;
      }
    }
    return false;
    
  }
   void removeInode(int watchDescr) {
     inotify_rm_watch(inotifyfd.get(), watchDescr);
      filemap.erase(watchDescr);
      foldermap.erase(watchDescr);
  }
  bool removeFile(std::string_view path){
    for(auto& [watchDescr,file]:filemap){
      if(file.getPath()==path){
        removeInode(watchDescr);
        return true;
      }
    }
    return false;
  }
  bool removeFolder(std::string_view path){
    for(auto& [watchDescr,folder]:foldermap){
      if(folder.getPath()==path){
        removeInode(watchDescr);
        return true;
      }
    }
    return false;
  }
  void addFile(File file,const Watchfs::csFW watchflags) {
    int flags=Watchfs::csFW::getValue(watchflags|Watchfs::misc::updateMaskOnDupl);
    int watchDesc = inotify_add_watch(inotifyfd.get(),file.getPath().data(),flags);
    if(watchDesc==-1){
      switch (errno) {
         case EACCES:
            throw std::runtime_error("Permission denied to watch the file or directory");
          case EBADF:
            throw std::runtime_error("Invalid inotify file descriptor");
          case EFAULT:
            throw std::runtime_error("Path is outside your accessible address space");
          case EINVAL:
            throw std::runtime_error("Invalid watch flags or path");
          case ENOENT:
            throw std::runtime_error("The file or directory does not exist");
          case ENOMEM:
            throw std::runtime_error("Insufficient kernel memory to add the watch");
          case ENOSPC:
            throw std::runtime_error("The user limit on the total number of inotify watches has been reached");
          case ENOTDIR:
            throw std::runtime_error("The path is not a directory and watch flags require a directory");
          default:
            throw std::runtime_error("Unknown error occurred while adding watch: " + std::to_string(errno));
      }
    }
    this->filemap.insert({watchDesc,std::move(file)});

  }
  void  addFolder(Folder folder, Watchfs::csFW watchflags){
  int flags=Watchfs::csFW::getValue(watchflags|Watchfs::misc::updateMaskOnDupl|Watchfs::misc::onlyDir);
    int watchDesc = inotify_add_watch(inotifyfd.get(),folder.getPath().data(),flags);
    if(watchDesc==-1){
      switch (errno) {
         case EACCES:
            throw std::runtime_error("Permission denied to watch the file or directory");
          case EBADF:
            throw std::runtime_error("Invalid inotify file descriptor");
          case EFAULT:
            throw std::runtime_error("Path is outside your accessible address space");
          case EINVAL:
            throw std::runtime_error("Invalid watch flags or path");
          case ENOENT:
            throw std::runtime_error("The file or directory does not exist");
          case ENOMEM:
            throw std::runtime_error("Insufficient kernel memory to add the watch");
          case ENOSPC:
            throw std::runtime_error("The user limit on the total number of inotify watches has been reached");
          case ENOTDIR:
            throw std::runtime_error("The path is not a directory and watch flags require a directory");
          default:
            throw std::runtime_error("Unknown error occurred while adding watch: " + std::to_string(errno));
      }
    }
    this->foldermap.insert({watchDesc,std::move(folder)});
  }
  
  void eventListener(Controller* client){
   while(true){
      debug::print("came to inotify");
     ssize_t bytesRead=::read(inotifyfd.get(),buffer.data(),buffer.size()) ;
     if(bytesRead==-1 && (errno==EWOULDBLOCK || errno==EWOULDBLOCK )){
      debug::print("nothing to read"); 
      return;
       
      }


      if(bytesRead==-1){
        switch (errno) {
          // need to implement 
          default:
          throw std::runtime_error("unknown error"+std::to_string(errno));
        }
      }
      process(client,bytesRead);
    }
  }

  void process(Controller* client,ssize_t length){
    debug::print("came to process",length);
     ssize_t i=0;
     using namespace Watchfs;
     while(i<length){
      auto* event=reinterpret_cast<inotify_event*>(&buffer[i]);
   

      if(event->mask&Watchfs::dir::isDir){
         auto it = foldermap.find(event->wd);
           if(it == foldermap.end()){
             client->write("directory not in db");
               return;
             }
          Folder& fld = it->second;
          std::string_view eventName;
          if (event->mask & dir::isDir) {
          eventName = dirflagToDescription(dir::isDir);
          } else if (event->mask & dir::deleted) {
              eventName = dirflagToDescription(dir::deleted);
          } else if (event->mask & dir::filedeleted) {
              eventName = dirflagToDescription(dir::filedeleted);
          } else if (event->mask & dir::fileCreated) {
              eventName = dirflagToDescription(dir::fileCreated);
          } else if (event->mask & dir::fileMovedout) {
              eventName = dirflagToDescription(dir::fileMovedout);
          } else if (event->mask & dir::fileMovedin) {
              eventName = dirflagToDescription(dir::fileMovedin);
          } else {
              eventName = "Unknown Event";
          }
          client->write(std::format("EventInfo:\n\t folderpath: {}\n\t event {}", fld.getPath(), eventName));
           
      }else{
         auto fileIt = filemap.find(event->wd);
         if(fileIt == filemap.end()){
             client->write("file not in db");
             return;
         }
         File& file = fileIt->second;
         std::string_view eventName;
         if (event->mask & file::fileDeleted) {
            eventName = fileFlagToDescription(file::fileDeleted);
        } else if (event->mask & file::modified) {
            eventName = fileFlagToDescription(file::modified);
        } else if (event->mask & file::accessForRead) {
            eventName = fileFlagToDescription(file::accessForRead);
        } else if (event->mask & file::opende) {
            eventName = fileFlagToDescription(file::opende);
        } else if (event->mask & file::closed) {
            eventName = fileFlagToDescription(file::closed);
        } else {
            eventName = "Unknown File Event";
        }        
          client->write(std::format("EventInfo:\n\t filepath: {}\n\t event {}\n", file.getPath(), eventName));
          

      }
      i+=(inotifyEventSize)+event->len;
     }

  }
  



};
} // namespace FileWatcherSystem
#endif

