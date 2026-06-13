#ifndef PROCESSOR_HPP
#define PROCESSOR_HPP
#include "fileWatcher.hpp"
#include "socket.hpp"
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <variant>

namespace FileWatcherSystem {
enum class methods : std::int8_t {
    ADD,
    REMOVE,
};

std::string strip(std::string_view str) {
    const std::string_view whitespace = " \t\r\n\v\f";

    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string_view::npos) {
        return "";
    }

    size_t end = str.find_last_not_of(whitespace);

    return std::string(str.substr(start, end - start + 1));
}
namespace ProtocolState{
   class DataCheckSuccess {
     public:
        ssize_t size;
        int stpos;
   };
   class DataNotArrived {
    public:
      int expected = -1;
      int arrived = -1;
      public:
      DataNotArrived() = default;
    DataNotArrived(int exp, int arr) : expected(exp), arrived(arr) {}
    std::string info()  noexcept  {
       if (expected == -1) {
          return "data corrupted";
         }
        return std::format("operation can't perform data not arrived yet expected={},arrived={}",
            expected, arrived);
         }
      };
      class SizeBytesNotArrived  {
         public:

         const std::string info() const noexcept  {
            return "size data bytes not arrived";
         }
      };
      class ReadBufferEmpty{
         public:

         const std::string info() const noexcept  {
            return "buffer is empty";
         }
      } ;
      class InvalidSizeBytes{
         public:

         const std::string info() const noexcept  {
            return "size data bytes not arrived";
         }
      };
      using ValidateData=std::variant<DataCheckSuccess,DataNotArrived,SizeBytesNotArrived,ReadBufferEmpty,InvalidSizeBytes>;
      
      // class UnknownCommand : public std::exception {
      //    public:
      //    std::string data = "unknown command";
      //    UnknownCommand() = default;
      //    UnknownCommand(std::string data) : data(data) {}
      //    const char* info() const noexcept override {
      //       return data.data();
      //    }
      // };
      class InvalidMainCommand{
         public:

         const std::string info()  noexcept  {
            return "invalid main command";
         }
      };
      class InvalidSubCommand{
         public:

           const std::string info()  noexcept  {
            return "invalid sub command";
         }
      };
      class FileAlreadyExists{
         public:
         std::string path;
         const std::string info()  noexcept  {
            return std::format("file already exists in watchlist:\n\t path :{} ",path);
         }  
      };
      class  FolderAlreadyExists{
          public:
         std::string path;
         const std::string info()  noexcept  {
            return std::format("folder already exists in watchlist path :{} ",path);
         }  
      };
        class FileNotExists{
         public:
         std::string path;
         const std::string info()  noexcept  {
            return std::format("file not exists in watchlist path :{} ",path);
         }  
      };
      class  FolderNotExists{
          public:
         std::string path;
         const std::string info()  noexcept  {
            return std::format("folder already exists in watchlist path :{} ",path);
         }  
      };
      class CommandIncomplete{
         std::string commandInfo;
         public:
         CommandIncomplete()=default;
         CommandIncomplete(std::string &&commandInfo):commandInfo(std::move(commandInfo)){};
           constexpr std::string info()  noexcept  {
            if(!commandInfo.empty()) return std::format("incomplete command commandInfo:{}",commandInfo);
            return "incomplete command provided";
         }  
      };
      class CommandProcessed{
         public:
         std::string mainCommand;
         std::string subCommand;
         std::string path;
          constexpr std::string info()  noexcept  {
             return std::format("main command: {},\n\tsub command:{},\n\tpath:{}",mainCommand,subCommand,path);
          }
      };
      using processResult=std::variant<CommandProcessed,InvalidMainCommand,InvalidSubCommand,FileAlreadyExists,FolderAlreadyExists,CommandIncomplete,FileNotExists,FolderNotExists>;


   }
class ProtocolProcessor {
  public:
    static ProtocolState::ValidateData validate(SocketClient* cl) {
     try{
        int stpos=0;
        if(cl->ReadDataBuff[0]=='\n')stpos++;
        std::string_view checkcommand=std::string_view(cl->ReadDataBuff).substr(stpos);
        if(checkcommand.size()==0) return ProtocolState::ReadBufferEmpty();
        if (checkcommand.size() < 4){
           try{
              int res=std::stoi(std::string(checkcommand));

              return ProtocolState::SizeBytesNotArrived();
           }catch(const std::runtime_error& e){
            flush(cl);
            return ProtocolState::InvalidSizeBytes();
           }
        } 
        ssize_t datalength = std::stoi(std::string(checkcommand.substr(0, 4)));
        if (checkcommand.size() < 5 + datalength && datalength > 0) {
            return ProtocolState::DataNotArrived(datalength, checkcommand.size() - 5);
        }
        return ProtocolState::DataCheckSuccess(datalength,stpos);

     }catch(std::invalid_argument& e){
        flush(cl);
        return ProtocolState::InvalidSizeBytes();
     }
    }
    static ProtocolState::processResult process( SocketClient* cl, int dataSize,int stpos,
                                                       Watcher* wth) {
        
        int upto = dataSize+5;

        std::string_view data(std::string_view(cl->ReadDataBuff).substr(stpos,dataSize+5));
        ScopeGuard guard([cl, upto = &upto,stpos] { flush(cl, *upto,stpos); });
        if(dataSize<4) return ProtocolState::CommandIncomplete("size is lesser than 4 means not any command possible");
        if (data.substr(5, 4) == "ADD ") {
            if(dataSize<9) return ProtocolState::CommandIncomplete("command has add but File");
            if (data.substr(9, 5) == "FILE ") {
                std::string path = strip(data.substr(14));
                if (!wth->haslareadyFile(path)) {
                    FileWatcherSystem::File fs(path, ios::readWrite, S_IRWXU);
                    wth->addFile(std::move(fs), Watchfs::file::accessForRead |
                                                    Watchfs::file::fileDeleted |
                                                    Watchfs::file::modified |
                                                    Watchfs::file::closed | Watchfs::file::opende);
                  return ProtocolState::CommandProcessed("ADD","FILE",(path));
                } else {
                    return ProtocolState::FileAlreadyExists(path);
                }

            } 
            if(dataSize<11) return ProtocolState::CommandIncomplete("command has add but not Folder");
            if (data.substr(9, 7) == "FOLDER ") {
               std::string path=strip(data.substr(15));
               if(!wth->haslareadyFolder(path)){

                  FileWatcherSystem::Folder fl(path);
                  wth->addFolder(std::move(fl),
                  Watchfs::dir::fileMovedin | Watchfs::dir::fileMovedout |
                  Watchfs::dir::fileCreated | Watchfs::dir::filedeleted |
                  Watchfs::dir::deleted);
                  return ProtocolState::CommandProcessed("ADD","FOLDER",std::move(path));
                  
               }else{
                  return ProtocolState::FolderAlreadyExists(path);
               }
            } 
            return ProtocolState::InvalidSubCommand();
        } 
        if(dataSize<7) return ProtocolState::CommandIncomplete("cannot store Remove");
        if (data.substr(5, 7) == "REMOVE ") {
            if(dataSize<12) return ProtocolState::CommandIncomplete("cannot store remove file");
      
            if (data.substr(12, 5) == "FILE ") {
                 std::string path=strip(data.substr(16));
                if(wth->removeFile(path)){
                  return ProtocolState::CommandProcessed("REMOVE","FILE",path);
                }else{
                  return ProtocolState::FileNotExists();
                };
            
            }
             if(dataSize<14) return ProtocolState::CommandIncomplete("cannot store Remove folder");

             if (data.substr(12, 7) == "FOLDER ") {
                 std::string path=strip(data.substr(19));
               if( wth->removeFolder(path)){
                  return ProtocolState::CommandProcessed("REMOVE","FOLDER",path);

               }else{
                  return ProtocolState::FolderNotExists();
               };

            }
          ProtocolState::InvalidSubCommand();
        }  
        return ProtocolState::InvalidMainCommand();
    }
    static void flush(SocketClient* cl, int upto=-1,int stpos=0) {
         //   std::cout<<
        if (upto == 0)
            return;
        if (upto == -1 || cl->ReadDataBuff.size() <= upto+stpos) {
            cl->ReadDataBuff.clear();
        } else {
            cl->ReadDataBuff.erase(0,stpos+ upto);
        }
    }
};
} 
#endif