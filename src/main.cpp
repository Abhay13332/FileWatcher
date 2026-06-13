#include "util.hpp"
#include<bits/stdc++.h>
#include<epoll.hpp>
#include <exception>
#include <expected>
#include <memory>
#include<socket.hpp>
#include<fdObject.hpp>
#include<fileWatcher.hpp>
#include<processer.hpp>
#include <stdexcept>
#include <sys/types.h>
#include <variant>
int main(){
   using namespace FileWatcherSystem;
   NBTcpSocket commandHandler(4001,10);
   NBTcpSocket subscriberH(4002,10);
   
   Watcher inotifyWatcher(Watchfs::init::closeOnExec|Watchfs::init::nonBLockMode);
   SkSubscriberController skctl(subscriberH);
   
   EpollMan epollManager(10,epollFlags::createcloseonExec);
   //leaks needs to be fix clients and epollClientObj for both server

   std::unique_ptr<EpollEvent> commandHandlerEpollObj(epollManager.createEventObj(&commandHandler).onReading([epollMan=&epollManager,wth=&inotifyWatcher](NBTcpSocket* serverSock,EpollEvent*){
       try{
           std::expected<SocketClient,NonBlockReadError> clientexp=serverSock->getClient();

           if(clientexp.has_value()){
            SocketClient *client=new SocketClient(std::move(clientexp.value()));
            debug::print("client",client->getFd());
            epollMan->createEventObj(client).onReading([epollMan,wth](SocketClient* cl,EpollEvent* clEventObj ){

                try {
                    using namespace ProtocolState;
                    if(!cl->read()){
                        return;
                    }
                    bool running =true;
                    while(running){
                    ProtocolState::ValidateData dataInfo=ProtocolProcessor::validate(cl);
                    std::visit(Overloaded{
                        [cl,wth](DataCheckSuccess& data){
                           ProtocolState::processResult res= ProtocolProcessor::process(cl, data.size,data.stpos, wth);
                           std::visit(Overloaded{
                            [cl](auto& res){
                              cl->WriteDataBuffremain.append(res.info()+"\n");  
                            }
                           },res);
                        },
                        [cl,runptr=&running](InvalidSizeBytes &data){
                           cl->WriteDataBuffremain.append("Invalid Size Bytes\n");
                           *runptr=false;
                        },
                        [cl,runptr=&running](auto &data){
                           *runptr=false;

                        },
                        
                    },dataInfo);
                }
                
            }catch(const std::runtime_error & e){
                cl->WriteDataBuffremain.append(e.what()).append("\n");
            }
            // std::cout << "here"<<std::endl;
            try{
            auto res= cl->write().transform_error([clEventObj,epollMan](auto&&)->std::expected<void, NonBlockReadError>{
                   EpollEventListenerModifier(clEventObj,epollMan).enableWriteEvent()->modifyinEpoll();
                   return {};
                }).transform([clEventObj](){
                    
                    // debug::printExp(clEventObj->getRdhupStatus(),"rdstatus=true","rdstatus=false");
                   
                    auto res=clEventObj->getRdhupStatus().transform([clEventObj](auto&&){
                       clEventObj->setCleanup();  
                    });
                });
           
                
            }catch(const std::runtime_error& e){
            }



            }
        )->onWrite([epollMan](SocketClient* cl,EpollEvent*cleventObj){
                try{
                   auto res= cl->write().transform([cleventObj,epollMan](){
                        auto res=cleventObj->getRdhupStatus().transform([cleventObj](auto&&){
                            cleventObj->setCleanup();
                        }).transform_error([cleventObj,epollMan](auto&&)->std::expected<void, EpollRdHup>{
                        EpollEventListenerModifier(cleventObj,epollMan).disableWriteEvent()->modifyinEpoll();
                            return {};
                        });
                    }); 
                }catch(const std::runtime_error& e){
                    debug::print(e.what());
                 }
            },false)->onHalfClose([](SocketClient* sc,EpollEvent*event){
                event->setCleanup();
            })
            ->onCleanup([](SocketClient* sc,EpollEvent*event){
                    delete sc;
                    delete event;
                    debug::print("onCleanup");
                 })
            ->addEvent();
         }
    }catch(const std::exception& e){
                    debug::print(e.what());
     }
   })->addEvent());

   std::unique_ptr<EpollEvent> subscribeEpollObj(epollManager.createEventObj(&subscriberH).onReading([epollMan=&epollManager,skctl=&skctl](NBTcpSocket* serverSock,EpollEvent*){
    try{
         std::expected<SocketClient,NonBlockReadError> clientexp=serverSock->getClient();

         if(clientexp.has_value()){
            SocketClient *client=new SocketClient(std::move(clientexp.value()));
            debug::print("get sub");
            EpollEvent* eventObj=epollMan->createEventObj(client).onWrite([epollMan](SocketClient* cl,EpollEvent*cleventObj){
                 try{
                   auto res= cl->write().transform([cleventObj,epollMan](){
                        auto res=cleventObj->getRdhupStatus().transform([cleventObj](auto&&){
                            cleventObj->setCleanup();
                        }).transform_error([cleventObj,epollMan](auto&&)->std::expected<void, EpollRdHup>{
                        EpollEventListenerModifier(cleventObj,epollMan).disableWriteEvent()->modifyinEpoll();
                            return {};
                        });
                    }); 
                }catch(const std::runtime_error& e){
                    debug::print(e.what());
                 }
            },false)->
            onHalfClose([](SocketClient*,EpollEvent*event){
                  event->setCleanup();

            })->onCleanup([skctl](SocketClient*sc,EpollEvent*event){
                debug::print("cleanup");
                 skctl->deletesub(event);

            })->addEvent();
            skctl->addsub(eventObj);

         }
    }catch(std::exception e){
        debug::print(e.what());
    }
   })->addEvent());

   std::unique_ptr<EpollEvent> inotifyEpollObj(epollManager.createEventObj(
       &inotifyWatcher).onReading([epollMan=&epollManager,controller=&skctl](Watcher* wth,EpollEvent*){
          
           wth->eventListener(controller);
           controller->writeToclients([epollMan](SocketClient* cl,EpollEvent*event){
                   EpollEventListenerModifier(event,epollMan).enableWriteEvent()->modifyinEpoll();


           });
        })->addEvent());
        

    epollManager.runEventLoop();

   
               
}