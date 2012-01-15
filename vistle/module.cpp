#include <mpi.h>

#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <unistd.h>

#include <sstream>
#include <iostream>
#include <iomanip>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include "object.h"
#include "message.h"
#include "messagequeue.h"
#include "module.h"

using namespace boost::interprocess;

namespace vistle {

Module::Module(const std::string &n, const int r, const int s, const int m)
   : name(n), rank(r), size(s), moduleID(m) {

   vistle::Shm::instance(moduleID, rank);

   std::cout << "  module [" << name << "] [" << moduleID << "] [" << rank
             << "/" << size << "] started" << std::endl;

   std::string smqName =
      vistle::MessageQueue::createName("rmq", moduleID, rank);
   std::string rmqName =
      vistle::MessageQueue::createName("smq", moduleID, rank);

   try {
      sendMessageQueue = vistle::MessageQueue::open(smqName);
      receiveMessageQueue = vistle::MessageQueue::open(rmqName);
   } catch (interprocess_exception &ex) {
      std::cout << "module " << moduleID << " [" << rank << "/" << size << "] "
                << ex.what() << std::endl;
      exit(2);
   }
}

bool Module::createInputPort(const std::string &name) {

   std::map<std::string, std::list<std::string> *>::iterator i =
      inputPorts.find(name);

   if (i == inputPorts.end()) {

      std::list<std::string> *l = new std::list<std::string>;
      inputPorts[name] = l;

      message::CreateInputPort message(name);
      sendMessageQueue->getMessageQueue().send(&message, sizeof(message), 0);
      return true;
   }

   return false;
}

bool Module::createOutputPort(const std::string &name) {

   std::map<std::string, std::list<std::string> *>::iterator i =
      outputPorts.find(name);

   if (i == outputPorts.end()) {

      std::list<std::string> *l = new std::list<std::string>;
      outputPorts[name] = l;

      message::CreateOutputPort message(name);
      sendMessageQueue->getMessageQueue().send(&message, sizeof(message), 0);
      return true;
   }
   return false;
}

bool Module::addObject(const std::string &portName,
                       const std::string &objectName) {

   std::map<std::string, std::list<std::string> *>::iterator i =
      outputPorts.find(portName);

   if (i != outputPorts.end()) {
      i->second->push_back(objectName);
      message::AddObject message(portName, objectName);
      sendMessageQueue->getMessageQueue().send(&message, sizeof(message), 0);
      return true;
   }
   return false;
}

bool Module::addInputObject(const std::string &portName,
                            const std::string &objectName) {

   std::map<std::string, std::list<std::string> *>::iterator i =
      inputPorts.find(portName);

   if (i != inputPorts.end()) {
      i->second->push_back(objectName);
      return true;
   }
   return false;
}

bool Module::dispatch() {

   size_t msgSize;
   unsigned int priority;
   char msgRecvBuf[128];

   receiveMessageQueue->getMessageQueue().receive((void *) msgRecvBuf,
                                                  (size_t) 128, msgSize,
                                                  priority);

   vistle::message::Message *message = (vistle::message::Message *) msgRecvBuf;

   bool done = handleMessage(message);
   if (done) {
      vistle::message::ModuleExit m(moduleID, rank);
      sendMessageQueue->getMessageQueue().send(&m, sizeof(m), 0);
   }

   return done;
}

bool Module::handleMessage(const vistle::message::Message *message) {

   switch (message->getType()) {

      case vistle::message::Message::DEBUG: {

         vistle::message::Debug *debug = (vistle::message::Debug *) message;

         std::cout << "    module [" << name << "] [" << moduleID << "] ["
                   << rank << "/" << size << "] debug ["
                   << debug->getCharacter() << "]" << std::endl;
         break;
      }

      case message::Message::QUIT: {

         message::Quit *quit = (message::Quit *) message;
         (void) quit;
         return true;
         break;
      }

      case message::Message::COMPUTE: {

         message::Compute *comp = (message::Compute *) message;
         (void) comp;
         std::cout << "    module [" << name << "] [" << moduleID << "] ["
                   << rank << "/" << size << "] compute" << std::endl;

         compute();
         break;
      }

      case message::Message::ADDOBJECT: {

         message::AddObject *add = (message::AddObject *) message;
         addInputObject(add->getPortName(), add->getObjectName());
         break;
      }

      default:
         std::cout << "    module [" << name << "] [" << moduleID << "] ["
                   << rank << "/" << size << "] unknown message type ["
                   << message->getType() << "]" << std::endl;

         break;
   }

   return false;
}

Module::~Module() {

   std::cout << "  module [" << name << "] [" << moduleID << "] [" << rank
             << "/" << size << "] quit" << std::endl;

   MPI_Finalize();
}

} // namespace vistle
