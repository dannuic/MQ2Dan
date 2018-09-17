#pragma once

#include <zmq_addon.hpp>

#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>

//  This is the version of MDP/Client we implement
#define MDPC_CLIENT         "MDPC01"

//  This is the version of MDP/Worker we implement
#define MDPW_WORKER         "MDPW01"

//  MDP/Server commands, as strings
#define MDPW_READY          "\001"
#define MDPW_REQUEST        "\002"
#define MDPW_REPLY          "\003"
#define MDPW_HEARTBEAT      "\004"
#define MDPW_DISCONNECT     "\005"

static char *mdps_commands [] = {
    NULL, (char*)"READY", (char*)"REQUEST", (char*)"REPLY", (char*)"HEARTBEAT", (char*)"DISCONNECT"
};

// TODO: Move these defines to config
#define HEARTBEAT_LIVENESS 3
#define HEARTBEAT_INTERVAL 2500
#define HEARTBEAT_EXPIRY   HEARTBEAT_INTERVAL * HEARTBEAT_LIVENESS

namespace MQ2DanZeroMQ {
    class Broker {
    protected:
        class service;

        class worker {
        public:
            worker(const std::string identity, const service* service = nullptr, int64_t expiry = 0) :
                _identity(identity), _service(service), _expiry(expiry) {}

        protected:
            const std::string _identity;
            const service* _service;
            const int64_t _expiry;
        };

        class service {
        public:
            service(const std::string name) : _name(name) {}
            ~service();

        protected:
            const std::string _name;
            const std::deque<zmq::multipart_t *> _requests;
            const std::list<worker *> _waiting;
            const size_t _workers;
        };

    public:
        Broker(const std::string name, int verbose);
        virtual ~Broker();

        // string identifer for listing
        inline const std::string name() const { return _name; }

        // Bind broker to an endpoint, which can be called multiple times
        void bind(std::string endpoint);

        // Start all processes
        void start_brokering();

    protected:
        const std::string _name;

        zmq::context_t *_context;
        zmq::socket_t *_socket;
        int _verbose;
        std::string _endpoint;
        std::map<std::string, service*> _services;
        std::map<std::string, worker*> _workers;
        std::set<worker*> _waiting;

        // Delete idle workers (based on pings)
        void purge_workers();

        // Locate/create service
        service* service_require(const std::string name);

        // Dispatch requests
        void service_dispatch(const service *srv, const zmq::multipart_t *multi);

        // Handle internal service
        void service_internal(const std::string service_name, const zmq::multipart_t *multi);

        // Locate/create worker
        worker* worker_require(const std::string identity);

        // Deletes worker from structures, and destroys
        void worker_delete(const worker *&wrk, int disconnect);

        // Process message sent by worker
        void worker_process(const std::string sender, const zmq::multipart_t *multi);

        // Send message to worker
        void worker_send(const worker *worker, const char *command, const std::string option, const zmq::multipart_t *multi);

        // Worker is now waiting
        void worker_waiting(const worker *worker);

        // Process client request
        void client_process(const std::string sender, const zmq::multipart_t *multi);
    };

    class Client {
    public:
        Client(const std::string broker, int verbose);
        ~Client();

        // connect or reconnect to the broker
        void connect();

        // set request timeout
        void set_timeout(int timeout);

        // send without waiting for a reply
        int send(const std::string service, const zmq::multipart_t *&request);

        // waits for a reply and returns to the caller
        zmq::multipart_t *recv();

    protected:
        std::string _broker;
        zmq::context_t *_context;
        zmq::socket_t *_client;
        int _verbose;
        int _timeout;
    };

    class Worker {
    public:
        Worker(const std::string broker, const std::string service, int verbose);
        ~Worker();

        void send(const char *command, const std::string option, const zmq::multipart_t *multi);
        void connect();
        zmq::multipart_t *recv(zmq::multipart_t *&reply_p);

        void set_heartbeat(int heartbeat);
        void set_reconnect(int reconnect);
        int setsockopt(int option, const void *optval, size_t optvallen);
        int getsockopt(int option, void *optval, size_t optvallen);

    protected:
        zmq::context_t *_context;
        std::string _broker;
        std::string _service;
        zmq::socket_t _worker;
        int _verbose;

        uint64_t _heartbeat_at;
        size_t _liveness;
        int _heartbeat;
        int _reconnect;

        int _expect_reply;

        std::string _reply_to;
    };
}