// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.

#include "match_server/Match.h"
#include "save_client/Save.h"
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/ThreadFactory.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/transport/TSocket.h>
#include <thrift/TToString.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <vector>
#include <string>
#include <unistd.h>
using namespace std;
using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace ::match_service;
using namespace ::save_service;

struct Task{
    User user;
    string type;
};

struct MessageQueue{
    queue<Task> q;
    mutex m;
    condition_variable cv;
}message_queue;

class Pool
{
    public:
        void save_result(int a, int b)
        {
            printf("the matched users are: %d %d\n", a, b);
            std::shared_ptr<TTransport> socket(new TSocket("123.57.47.211", 9090));
            std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
            std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
            SaveClient client(protocol);

            try {
                transport->open();

                client.save_data("acs_1751","086f2a51",a,b);

                transport->close();
            } catch (TException& tx) {
                cout << "ERROR: " << tx.what() << endl;
            }
        }

        bool check_match(int i, int j)
        {
            User a = users[i], b = users[j];
            int dt = abs(a.score-b.score);
            int a_max_diff = wt[i]*50;
            int b_max_diff = wt[j]*50;
            return dt <= a_max_diff && dt <= b_max_diff;
        }

        void match()
        {
            for(uint32_t i = 0; i < wt.size(); i++)
                wt[i] ++ ; // ??????????????????
            while(users.size() > 1)
            {
                /*
                   sort(users.begin(), users.end(), [&](User &a, User &b){
                   return a.score < b.score;});
                   for(uint32_t i = 1; i < users.size(); i++){
                   User a = users[i-1], b = users[i];
                   if(b.score - a.score <= 50){
                   users.erase(users.begin()+i-1, users.begin()+i+1);
                   save_result(a.id, b.id);
                   break;
                   }
                   } */
                bool flag = true;
                for(uint32_t i = 0; i < users.size(); i++)
                {
                    for(uint32_t j = i+1; j < users.size(); j++)
                    {
                        if(check_match(i, j))
                        {
                            User a = users[i], b = users[j];
                            users.erase(users.begin()+j);
                            users.erase(users.begin()+i);
                            wt.erase(wt.begin()+j);
                            wt.erase(wt.begin()+i);
                            save_result(a.id, b.id);
                            flag = false;
                            break;
                        }
                    }
                    if(!flag)
                        break;
                }
                if(flag)
                    break;
            }
        }
        void add_user(User user)
        {
            users.push_back(user);
            wt.push_back(0);
        }
        void remove_user(User user)
        {
            for(uint32_t i = 0; i < users.size(); i ++ )
            {
                if(users[i].id == user.id)
                {
                    users.erase(users.begin() + i);
                    wt.erase(wt.begin() + i);
                    break;
                }
            }
        }
    private:
        vector<User> users;
        vector<int> wt; // ????????????????????????s
}pool;

class MatchHandler : virtual public MatchIf {
    public:
        MatchHandler() {
            // Your initialization goes here
        }

        int32_t add_user(const User& user, const std::string& info) {
            // Your implementation goes here
            printf("add_user\n");

            unique_lock<mutex> lck(message_queue.m);
            message_queue.q.push({user,"add"});
            message_queue.cv.notify_all();
            return 0;
        }

        int32_t remove_user(const User& user, const std::string& info) {
            // Your implementation goes here
            printf("remove_user\n");

            unique_lock<mutex> lck(message_queue.m);
            message_queue.q.push({user,"remove"});
            message_queue.cv.notify_all();
            return 0;
        }

};

class MatchCloneFactory : virtual public MatchIfFactory {
    public:
        ~MatchCloneFactory() override = default;
        MatchIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) override
        {
            std::shared_ptr<TSocket> sock = std::dynamic_pointer_cast<TSocket>(connInfo.transport);
            /* cout << "Incoming connection\n";
               cout << "\tSocketInfo: "  << sock->getSocketInfo() << "\n";
               cout << "\tPeerHost: "    << sock->getPeerHost() << "\n";
               cout << "\tPeerAddress: " << sock->getPeerAddress() << "\n";
               cout << "\tPeerPort: "    << sock->getPeerPort() << "\n"; */
            return new MatchHandler;
        }
        void releaseHandler( MatchIf* handler) override {
            delete handler;
        }
};

void consume_task()
{
    while(true)
    {
        unique_lock<mutex> lck(message_queue.m);
        if(message_queue.q.empty())
        {
            // message_queue.cv.wait(lck);
            lck.unlock();
            pool.match();
            sleep(1);
        }
        else
        {
            Task task = message_queue.q.front();
            message_queue.q.pop();
            lck.unlock();

            //do task
            if(task.type == "add")
                pool.add_user(task.user);
            else if(task.type == "remove")
                pool.remove_user(task.user);
        }

    }
}


int main(int argc, char **argv) {
    TThreadedServer server(
            std::make_shared<MatchProcessorFactory>(std::make_shared<MatchCloneFactory>()),
            std::make_shared<TServerSocket>(9090), //port
            std::make_shared<TBufferedTransportFactory>(),
            std::make_shared<TBinaryProtocolFactory>());

    thread matching_thread(consume_task);
    server.serve();
    return 0;
}

