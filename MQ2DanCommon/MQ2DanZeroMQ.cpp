#include "MQ2DanZeroMQ.h"

#include <chrono>

MQ2DanZeroMQ::Broker::service::~service() {
    for (size_t i = 0; i < _requests.size(); ++i)
        delete _requests[i];
}

MQ2DanZeroMQ::Broker::Broker(const std::string name, int verbose) : 
    _name(name),
    _context(new zmq::context_t(1)),
    _socket(new zmq::socket_t(*_context, ZMQ_ROUTER)),
    _verbose(verbose) {}

MQ2DanZeroMQ::Broker::~Broker() {
    while (!_services.empty()) {
        delete _services.begin()->second;
        _services.erase(_services.begin());
    }

    while (!_workers.empty()) {
        delete _workers.begin()->second;
        _workers.erase(_workers.begin());
    }
}

void MQ2DanZeroMQ::Broker::bind(std::string endpoint) {
    _endpoint = endpoint;
    _socket->bind(_endpoint);
}

void MQ2DanZeroMQ::Broker::start_brokering() {
    using namespace std::chrono;
    int64_t now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    int64_t heartbeat_at = now + HEARTBEAT_INTERVAL;

    while (true) {
        zmq::pollitem_t items[] = { {*_socket, 0, ZMQ_POLLIN, 0} };
        int64_t timeout = heartbeat_at - now;
        if (timeout < 0) timeout = 0;
        zmq::poll(items, 1, (long)timeout);

        // check for and process next message
        if (items[0].revents & ZMQ_POLLIN) {
            zmq::multipart_t *multi = new zmq::multipart_t(*_socket);

            std::string sender = multi->popstr();
            std::string header = multi->popstr();
            zmq::message_t msg = multi->pop();

            // check sender so then route to client_process() or worker_process()
            // do not delete the message if its routed -- only if it's not (invalid message)
        }

        // disco and delete expired workers -- send heartbeats to idle workers
        now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        if (now >= heartbeat_at) {
            purge_workers();
            for (auto it = _waiting.begin(); it != _waiting.end() && (*it) != 0; ++it) {
                worker_send(*it, (char*)MDPW_HEARTBEAT, "", nullptr);
            }
            heartbeat_at += HEARTBEAT_INTERVAL;
            now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        }
    }
}
