#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>

#include "../MQ2Plugin.h"

// forward declarations
#ifdef __cplusplus
extern "C" {
#endif

struct _zyre_t;
typedef struct _zyre_t zyre_t;

struct _zactor_t;
typedef struct _zactor_t zactor_t;

struct _zsock_t;
typedef struct _zsock_t zsock_t;

#ifdef __cplusplus
}
#endif

#ifdef MQ2DANNET_NODE_EXPORTS
#define MQ2DANNET_NODE_API __declspec(dllexport)
#else
#define MQ2DANNET_NODE_API __declspec(dllimport)
#endif

// reduce some boilerplate - we don't actually want to instantiate our commands, so delete all 5 assign/ctors
#define COMMAND(_Name, ...) class _Name {\
public:\
    static const std::string name() { return #_Name; }\
    static const bool callback(std::stringstream&& args);\
    static std::stringstream pack( ##__VA_ARGS__ );\
private:\
    _Name() = delete;\
    _Name(const _Name&) = delete;\
    _Name& operator=(const _Name&) = delete;\
    _Name(_Name&&) = delete;\
    _Name& operator=(_Name&&) = delete;\
}

namespace MQ2DanNet {
    class Node final {
    public:
        MQ2DANNET_NODE_API static Node& get();

        MQ2DANNET_NODE_API void join(const std::string& group);
        MQ2DANNET_NODE_API void leave(const std::string& group);

        MQ2DANNET_NODE_API void on_join(std::function<bool(const std::string&, const std::string&)> callback);
        MQ2DANNET_NODE_API void on_leave(std::function<bool(const std::string&, const std::string&)> callback);

        template<typename T, typename... Args>
        void whisper(const std::string& recipient, Args&&... args) {
            std::stringstream arg_stream = pack<T>(std::forward<Args>(args)...);
            respond(recipient, name<T>(), std::move(arg_stream));
        }

        template<typename T, typename... Args>
        void shout(const std::string& group, Args&&... args) {
            std::stringstream arg_stream = pack<T>(std::forward<Args>(args)...);
            publish(group, name<T>(), std::move(arg_stream));
        }

        MQ2DANNET_NODE_API const std::string get_info();
        MQ2DANNET_NODE_API const std::string get_interfaces();
        MQ2DANNET_NODE_API const std::string get_full_name(const std::string& name);

        // quick helper function to safely init strings from chars
        MQ2DANNET_NODE_API static std::string init_string(const char *szStr);

        template<typename T>
        static const std::string name() { return T::name(); }

        template<typename T>
        static const std::function<bool(std::stringstream&&)> callback() {
            return T::callback;
        }

        // we gotta trust that copy elision works here, which it should in c++14 or more for stringstream.
        // worst case is a slightly slower command because we have to copy the stream
        template<typename T, typename... Args>
        static std::stringstream pack(Args&&... args) { return T::pack(std::forward<Args>(args)...); }

        template<typename T>
        void register_command() { register_command(name<T>(), callback<T>()); }

        template<typename T>
        void unregister_command() { unregister_command(name<T>()); }
        
        // register custom commands (for responses)
        void register_command(const std::string& name, std::function<bool(std::stringstream&&)> callback) { _command_map[name] = callback; }
        void unregister_command(const std::string& name) { _command_map.erase(name); }

        // finds and inserts the next int key, returns `"response" + new_key`
        // this is generated by the requester
        MQ2DANNET_NODE_API std::string register_response(std::function<bool(std::stringstream&&)> callback);
        MQ2DANNET_NODE_API void respond(const std::string& name, const std::string& cmd, std::stringstream&& args);

        struct Observation final {
            MQ2TYPEVAR data;
            unsigned __int64 received;

            Observation(MQ2TYPEVAR data, unsigned __int64 received) : data(data), received(received) {}
            Observation() : received(0) { data.Type = 0; data.Int64 = 0; }
        };

        // finds query and returns the observation group, generates new group name if query not found
        MQ2DANNET_NODE_API std::string register_observer(const std::string& group, const std::string& query);
        MQ2DANNET_NODE_API void observe(const std::string& group, const std::string& name, const std::string& query);
        MQ2DANNET_NODE_API void forget(const std::string& group);
        MQ2DANNET_NODE_API void forget(const std::string& name, const std::string& query);
        MQ2DANNET_NODE_API void update(const std::string& group, const MQ2TYPEVAR& data);
        MQ2DANNET_NODE_API const Observation read(const std::string& group);
        MQ2DANNET_NODE_API const Observation read(const std::string& name, const std::string& query);
        MQ2DANNET_NODE_API void publish(const std::string& group, const std::string& cmd, std::stringstream&& args);

        template<typename T, typename... Args>
        void publish(Args&&... args) {
            for (auto observer_it = _observer_map.begin(); observer_it != _observer_map.end(); ++ observer_it) {
                auto tick = MQGetTickCount64();
                if (tick - observer_it->second.last >= std::max<unsigned __int64>(10 * observer_it->second.benchmark, 1000)) { // wait at least a second between updates
                    std::string group = observer_group(observer_it->first);
                    auto group_it = _group_peers.cbegin();
                    if (group_it != _group_peers.cend() && !group_it->second.empty()) {
                        shout<T>(group_it->first, observer_it->second.query, std::forward<Args>(args)...);

                        auto proc_time = MQGetTickCount64() - tick;
                        if (observer_it->second.benchmark == 0)
                            observer_it->second.benchmark = proc_time;
                        else
                            observer_it->second.benchmark = static_cast<unsigned __int64>(0.5 * (observer_it->second.benchmark + proc_time));

                        observer_it->second.last = tick;
                    }
                }
            }
        }

    private:
        std::string _node_name;
        std::set<std::string> _groups;
        std::map<std::string, std::string> _peers;
        std::map<std::string, std::set<std::string> > _group_peers;

        std::vector<std::function<bool(const std::string&, const std::string&)> > _join_callbacks;
        std::vector<std::function<bool(const std::string&, const std::string&)> > _leave_callbacks;

        // I don't like this, but since zyre/czmq does the memory management for these, I should store these as raw pointers
        zyre_t *_node;
        zactor_t *_actor;

        // command containers
        std::map<std::string, std::function<bool(std::stringstream&& args)> > _command_map; // callback name, callback
        std::queue<std::pair<std::string, std::stringstream> > _command_queue; // pair callback name, callback

        std::set<unsigned char> _response_keys; // ordered number of responses

        struct Query final {
            std::string query;
            unsigned __int64 benchmark;
            unsigned __int64 last;

            //Benchmarks[bmParseMacroParameter];

            Query() = default;
            Query(const std::string& query) : query(query), benchmark(0), last(0) {}

            // let's do some copy and swap for a bit of easy optimization
            friend void swap(Query& left, Query& right) {
                using std::swap;
                swap(left.query, right.query);
                swap(left.benchmark, right.benchmark);
                swap(left.last, right.last);
            }

            Query(const Query& other) : query(other.query), benchmark(other.benchmark), last(other.last) {}
            Query(Query&& other) noexcept : query(std::move(other.query)), benchmark(std::move(other.benchmark)), last(std::move(other.last)) {}
            Query& operator=(Query rhs) { swap(*this, rhs); return *this; }
        };

        struct Observed final {
            std::string query;
            std::string name;

            Observed() = default;
            Observed(const std::string& query, const std::string& name) : query(query), name(name) {}

            friend void swap(Observed& left, Observed& right) {
                using std::swap;
                swap(left.query, right.query);
                swap(left.name, right.name);
            }

            Observed(const Observed& other) : query(other.query), name(other.name) {}
            Observed(Observed&& other) noexcept : query(std::move(other.query)), name(std::move(other.name)) {}
            Observed& operator=(Observed rhs) { swap(*this, rhs); return *this; }
        };

        struct ObservedCompare final {
            bool operator() (const Observed& lhs, const Observed& rhs) const {
                if (lhs.query != rhs.query)
                    return lhs.query < rhs.query;
                else
                    return lhs.name < rhs.name;
            }
        };

        std::map<unsigned int, Query> _observer_map; // group number, query
        std::map<Observed, std::string, ObservedCompare> _observed_map; // maps query to group (for data access)
        std::map<std::string, Observation> _observed_data; // maps group to query result (could be empty)

        static void node_actor(zsock_t *pipe, void *args);
        const std::string observer_group(const unsigned int key);
        void queue_command(const std::string& command, std::stringstream&& args);

        std::string _current_query; // for the Query data member
        Observation _query_result;

        bool _debugging;

        // explicitly prevent copy/move operations.
        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;
        Node(Node&&) = delete;
        Node& operator=(Node&&) = delete;

        Node();
        ~Node();

    public:
        // IMPORTANT: these are not exposed as an API, this is on purpose! We need a single point of control for our node (this plugin)
        std::string name() { return _node_name; }

        bool has_peer(const std::string& peer) { return _peers.find(get_full_name(peer)) != _peers.end(); }
        size_t peers() { return _peers.size(); }
        std::string peers_arr();

        // smartly reads/sets/clears _current_query
        Observation query(const std::string& query);
        void query_result(const Observation& obs);

        bool debugging(bool debugging) { _debugging = debugging; return _debugging; }
        bool debugging() { return _debugging; }

        void enter();
        void exit();
        void shutdown();

        void do_next();
    };
}
