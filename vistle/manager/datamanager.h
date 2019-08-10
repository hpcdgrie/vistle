#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <functional>
#include <set>
#include <thread>
#include <deque>
#include <future>

#include <core/message.h>
#include <core/messages.h>
#include <core/object.h>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/mpi/request.hpp>
#include <boost/mpi/communicator.hpp>

namespace vistle {

class Communicator;
class StateTracker;
class Object;

class DataManager {

public:
    DataManager(boost::mpi::communicator &comm);
    ~DataManager();
    bool handle(const message::Message &msg, std::vector<char> *payload);
    //! request a remote object for servicing an AddObject request
    bool requestObject(const message::AddObject &add, const std::string &objId, const ObjectCompletionHandler &handler);
    //! request a remote object for resolving a reference to a sub-object
    bool requestObject(const std::string &referrer, const std::string &objId, int hub, int rank, const ObjectCompletionHandler &handler);
    bool requestArray(const std::string &referrer, const std::string &arrayId, int type, int hub, int rank, const ArrayCompletionHandler &handler);
    bool prepareTransfer(const message::AddObject &add);
    bool completeTransfer(const message::AddObjectCompleted &complete);
    bool notifyTransferComplete(const message::AddObject &add);
    bool connect(boost::asio::ip::tcp::resolver::iterator &hub);
    bool dispatch();

    void trace(message::Type type);

    bool send(const message::Message &message, std::shared_ptr<std::vector<char>> payload=nullptr);

    struct Msg {
       Msg(message::Buffer &&buf, std::vector<char> &&payload);
       message::Buffer buf;
       std::vector<char> payload;
    };

private:
    bool handlePriv(const message::RequestObject &req);
    bool handlePriv(const message::SendObject &snd, std::vector<char> *payload);
    bool handlePriv(const message::AddObjectCompleted &complete);
    void updateStatus();

    std::mutex m_recvMutex;
    std::deque<Msg> m_recvQueue;
    bool m_quit = false;

    void recvLoop();
    void sendLoop();
    void cleanLoop();

    boost::mpi::communicator m_comm;
    const int m_rank, m_size;
    boost::mpi::request m_req;
    int m_msgSize;

    boost::asio::io_service m_ioService;
    boost::asio::ip::tcp::socket m_dataSocket;

    std::set<message::AddObject> m_inTransitObjects; //!< objects for which AddObject messages have been sent to remote hubs -- cannot be deleted yet

    std::map<std::string, std::set<message::AddObject>> m_outstandingAdds; //!< AddObject messages for which requests to retrieve objects from remote have been sent

    std::map<std::string, std::vector<ArrayCompletionHandler>> m_requestedArrays; //!< requests for (sub-)objects which have not been serviced yet

    struct OutstandingObject {
       vistle::Object::const_ptr obj;
       std::vector<ObjectCompletionHandler> completionHandlers;
    };
    std::map<std::string, OutstandingObject> m_requestedObjects; //!< requests for (sub-)objects which have not been serviced yet
    std::mutex m_requestMutex;

    std::mutex m_recvTaskMutex;
    std::deque<std::future<bool>> m_recvTasks;

    std::mutex m_sendTaskMutex;
    std::deque<std::future<bool>> m_sendTasks;

    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_workGuard;
    std::thread m_ioThread;
    std::thread m_recvThread;
    std::thread m_cleanThread;
    message::Type m_traceMessages = message::INVALID;
};

} // namespace vistle
#endif
