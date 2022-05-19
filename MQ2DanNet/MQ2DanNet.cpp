/* MQ2DanNet -- peer to peer auto-discovery networking plugin
 */

// MQ2DanNet.cpp : Defines the entry point for the DLL application.
//

// PLUGIN_API is only to be used for callbacks.  All existing callbacks at this time
// are shown below. Remove the ones your plugin does not use.  Always use Initialize
// and Shutdown for setup and cleanup, do NOT do it in DllMain.

#if !defined(NOMINMAX)
#define NOMINMAX
#endif

// IMPORTANT! This must be included first because it includes <winsock2.h>, which needs to come before <windows.h> -- we cannot guarantee no inclusion of <windows.h> in other headers
#ifdef LOCAL_BUILD
#include <zyre.h>
#else
#include "..\MQ2DanNetDeps\libzyre\include\zyre.h"
#endif

#include <mq/Plugin.h>

#ifdef LOCAL_BUILD
#include <archive.h>
#else
#include "..\archive\archive.h"
#endif

#include <regex>
#include <iterator>
#include <functional>
#include <numeric>
#include <sstream>
#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <mutex>

PLUGIN_VERSION(0.7525);
PreSetup("MQ2DanNet");

#pragma region NodeDefs

#ifdef MQ2DANNET_NODE_EXPORTS
#define MQ2DANNET_NODE_API __declspec(dllexport)
#else
#define MQ2DANNET_NODE_API __declspec(dllimport)
#endif

// reduce some boilerplate - we don't actually want to instantiate our commands, so delete all 5 assign/ctors
#define COMMAND(_Name, ...)                                                         \
    class _Name {                                                                   \
    public:                                                                         \
        static const std::string name() { return #_Name; }                          \
        static const bool callback(std::stringstream&& args);                       \
        static std::stringstream pack(const std::string& recipient, ##__VA_ARGS__); \
                                                                                    \
    private:                                                                        \
        _Name() = delete;                                                           \
        _Name(const _Name&) = delete;                                               \
        _Name& operator=(const _Name&) = delete;                                    \
        _Name(_Name&&) = delete;                                                    \
        _Name& operator=(_Name&&) = delete;                                         \
    };

namespace MQ2DanNet {
class Node final {
public:
    MQ2DANNET_NODE_API static Node& get();

    MQ2DANNET_NODE_API void join(const std::string& group);
    MQ2DANNET_NODE_API void leave(const std::string& group);

    MQ2DANNET_NODE_API void on_join(std::function<bool(const std::string&, const std::string&)> callback);
    MQ2DANNET_NODE_API void on_leave(std::function<bool(const std::string&, const std::string&)> callback);

    template <typename T, typename... Args>
    void whisper(const std::string& recipient, Args&&... args) {
        std::stringstream arg_stream = pack<T>(recipient, std::forward<Args>(args)...);
        respond(recipient, name<T>(), std::move(arg_stream));
    }

    template <typename T, typename... Args>
    void shout(const std::string& group, Args&&... args) {
        std::stringstream arg_stream = pack<T>(group, std::forward<Args>(args)...);
        publish(group, name<T>(), std::move(arg_stream));
    }

    MQ2DANNET_NODE_API const std::list<std::string> get_info();
    MQ2DANNET_NODE_API const std::set<std::string> get_peers();
    MQ2DANNET_NODE_API const std::set<std::string> get_all_groups();
    MQ2DANNET_NODE_API const std::set<std::string> get_own_groups();
    MQ2DANNET_NODE_API const std::map<std::string, std::set<std::string>> get_group_peers();
    MQ2DANNET_NODE_API const std::set<std::string> get_group_peers(const std::string& group);
    MQ2DANNET_NODE_API const std::set<std::string> get_peer_groups(const std::string& peer);
    MQ2DANNET_NODE_API const std::string get_interfaces();
    MQ2DANNET_NODE_API const std::string get_full_name(const std::string& name);
    MQ2DANNET_NODE_API const std::string get_short_name(const std::string& name);
    MQ2DANNET_NODE_API const std::string get_name(const std::string& name);

    // quick helper function to safely init strings from chars
    MQ2DANNET_NODE_API static std::string init_string(const char* szStr);

    template <typename T>
    static std::string name() { return T::name(); }

    template <typename T>
    static std::function<bool(std::stringstream&&)> callback() {
        return T::callback;
    }

    // we gotta trust that copy elision works here, which it should in c++14 or more for stringstream.
    // worst case is a slightly slower command because we have to copy the stream
    template <typename T, typename... Args>
    static std::stringstream pack(Args&&... args) { return T::pack(std::forward<Args>(args)...); }

    template <typename T>
    void register_command() { register_command(name<T>(), callback<T>()); }

    template <typename T>
    void unregister_command() { unregister_command(name<T>()); }

    // register custom commands (for responses)
    void register_command(const std::string& name, std::function<bool(std::stringstream&&)> callback) { _command_map.upsert(name, callback); }
    void unregister_command(const std::string& name) { _command_map.erase(name); }

    // finds and inserts the next int key, returns `"response" + new_key`
    // this is generated by the requester
    MQ2DANNET_NODE_API std::string register_response(std::function<bool(std::stringstream&&)> callback);
    MQ2DANNET_NODE_API void respond(const std::string& name, const std::string& cmd, std::stringstream&& args);

    struct Observation final {
        std::string output;
        std::string data;
        unsigned __int64 received;

        Observation(const Observation& obs) : output(obs.output), data(obs.data), received(obs.received) {}
        Observation(const std::string& output) : output(output), data("NULL"), received(0) {}
        Observation(const std::string& output, const std::string& data, unsigned __int64 received) : output(output), data(data), received(received) {}
        Observation() : output(), data("NULL"), received(0) {}
    };

    // finds query and returns the observation group, generates new group name if query not found
    MQ2DANNET_NODE_API std::string register_observer(const std::string& group, const std::string& query);
    MQ2DANNET_NODE_API void unregister_observer(const std::string& query);
    MQ2DANNET_NODE_API void observe(const std::string& group, const std::string& name, const std::string& query);
    MQ2DANNET_NODE_API void forget(const std::string& group);
    MQ2DANNET_NODE_API void forget(const std::string& name, const std::string& query);
    MQ2DANNET_NODE_API void forget_all(const std::string& name);
    MQ2DANNET_NODE_API void forget_if(bool (*predicate)(const Observation& observation));
    MQ2DANNET_NODE_API void update(const std::string& group, const std::string& data, const std::string& output);
    MQ2DANNET_NODE_API const Observation read(const std::string& group);
    MQ2DANNET_NODE_API const Observation read(const std::string& name, const std::string& query);
    MQ2DANNET_NODE_API bool can_read(const std::string& name, const std::string& query);
    MQ2DANNET_NODE_API size_t observed_count(const std::string& name);
    MQ2DANNET_NODE_API std::set<std::string> observed_queries(const std::string& name);
    MQ2DANNET_NODE_API size_t observer_count();
    MQ2DANNET_NODE_API std::set<std::string> observer_queries();
    MQ2DANNET_NODE_API std::set<std::string> observers(const std::string& query);
    MQ2DANNET_NODE_API void publish(const std::string& group, const std::string& cmd, std::stringstream&& args);

    template <typename T, typename... Args>
    void publish(Args&&... args) {
        std::map<unsigned int, Query> updated_values;

        _observer_map.foreach ([this, &updated_values](std::pair<unsigned int, Query> observer) -> void {
            const auto tick = MQGetTickCount64();
            if (tick - observer.second.last >= std::max<unsigned __int64>(10 * observer.second.benchmark, observe_delay())) { // wait at least a second between updates
                std::string group = observer_group(observer.first);
                std::string query_result = parse_query(observer.second.query);

				if (!_query_map.contains(observer.second.query) || _query_map.get(observer.second.query) != query_result) {
                    _query_map.upsert(observer.second.query, query_result);
                    shout<T>(group, query_result, std::forward<Args>(args)...);
                }

                Query new_query(observer.second.query);

                const auto proc_time = MQGetTickCount64() - tick;
                if (observer.second.benchmark == 0)
                    new_query.benchmark = proc_time;
                else
                    new_query.benchmark = static_cast<unsigned __int64>(0.5 * (observer.second.benchmark + proc_time));

                new_query.last = tick;

                updated_values[observer.first] = new_query;
            }
        });

        for (auto& updated_value : updated_values) {
            _observer_map.upsert(updated_value.first, updated_value.second);
        }
    }

	void clear_observer_cache() {
        _query_map.clear();
	}

private:
    std::string _node_name;

    // we don't need anything crazy here, there is only a single actor so deadlocks won't be an issue (if we're not dumb about it)
    template <typename T>
    class locked_vector {
    private:
        std::mutex _mutex;
        std::vector<T> _vector;

    public:
        void push_back(T& e) {
            std::scoped_lock<std::mutex> lock(_mutex);
            _vector.push_back(e);
        }

        void remove_if(std::function<bool(T)> f) {
            std::scoped_lock<std::mutex> lock(_mutex);
            for (auto it = _vector.begin(); it != _vector.end();) {
                if (f(*it))
                    _vector.erase(it);
                else
                    ++it;
            }
        }
    };

    template <typename T>
    class locked_set {
    private:
        std::mutex _mutex;
        std::set<T> _set;

    public:
        std::set<T> copy() {
            std::scoped_lock<std::mutex> lock(_mutex);
            std::set<T> r;
            r.insert(_set.cbegin(), _set.cend());
            return r;
        }

        void clear() {
            std::scoped_lock<std::mutex> lock(_mutex);
            _set.clear();
        }

        void emplace(T e) {
            std::scoped_lock<std::mutex> lock(_mutex);
            _set.emplace(e);
        }

        void erase(T e) {
            std::scoped_lock<std::mutex> lock(_mutex);
            _set.erase(e);
        }

        T get_next(std::function<T(T)> f) {
            std::scoped_lock<std::mutex> lock(_mutex);
            T r = _set.empty() ? T() : f(*(_set.crbegin()));
            return r;
        }

        void insert(T& e) {
            std::scoped_lock<std::mutex> lock(_mutex);
            _set.insert(e);
        }
    };

    template <typename T>
    class locked_queue {
    private:
        std::mutex _mutex;
        std::deque<T> _queue;

    public:
        //emplace empty front pop
        void emplace(T& e) {
            std::scoped_lock<std::mutex> lock(_mutex);
            _queue.emplace_front(std::move(e));
        }

        T pop() {
            std::scoped_lock<std::mutex> lock(_mutex);
            T r;
            if (!_queue.empty()) {
                r = std::move(_queue.front());
                _queue.pop_front(); // go ahead and pop it off, we've moved it
            }
            return r;
        }

        void remove_if(const std::function<bool(T&)>& f) {
            std::scoped_lock<std::mutex> lock(_mutex);
            // TODO: Confirm this is correct handling of the nodiscard / intention
            static_cast<void>(std::remove_if(_queue.begin(), _queue.end(), f));
        }
    };

    template <typename T, typename U, typename V = std::less<T>>
    class locked_map {
    private:
        std::mutex _mutex;
        std::map<T, U, V> _map;

    public:
        std::size_t erase(const T& n) {
            std::scoped_lock<std::mutex> lock(_mutex);
            std::size_t r = _map.erase(n);
            return r;
        }

        void upsert(const T& n, const U& v) {
            std::scoped_lock<std::mutex> lock(_mutex);
            _map[n] = v;
        }

        void upsert(const T& n, std::function<void(U&)> f) {
            std::scoped_lock<std::mutex> lock(_mutex);
            f(_map[n]);
        }

        T upsert_wrap(U const& e, std::function<T(T)> f) {
            std::scoped_lock<std::mutex> lock(_mutex);
            // C99, 6.2.5p9 -- guarantees that this will wrap to 0 once we reach max value
            T position = _map.empty() ? T() : f(_map.crbegin()->first);
            _map[position] = e;
            return position;
        }

        U get(const T& n) {
            std::scoped_lock<std::mutex> lock(_mutex);
            U r; // it's default constructed
            auto r_it = _map.find(n);
            if (r_it != _map.end())
                r = r_it->second;
            return r;
        }

        bool contains(const T& n) {
            std::scoped_lock<std::mutex> lock(_mutex);
            bool r = (_map.find(n) != _map.end());
            return r;
        }

        std::set<T, V> keys() {
            std::scoped_lock<std::mutex> lock(_mutex);
            std::set<T, V> r;
            std::transform(_map.cbegin(), _map.cend(), std::inserter(r, r.begin()),
                [](std::pair<T, U> key_val) -> T { return key_val.first; });

            return r;
        }

        std::list<U> values() {
            std::scoped_lock<std::mutex> lock(_mutex);
            std::list<U> r;
            std::transform(_map.cbegin(), _map.cend(), std::inserter(r, r.begin()),
                [](std::pair<T, U> key_val) -> U { return key_val.second; });

            return r;
        }

        void foreach (std::function<void(std::pair<T, U>)> f) {
            std::scoped_lock<std::mutex> lock(_mutex);
            for (auto it = _map.begin(); it != _map.end(); ++it) {
                f(*it);
            }
        }

        void erase_if(const T& n, std::function<bool(U&)> f) {
            std::scoped_lock<std::mutex> lock(_mutex);
            auto it = _map.find(n);
            if (it != _map.end() && f(it->second)) {
                _map.erase(it);
            }
        }

        void erase_if(std::function<bool(std::pair<T, U>)> f) {
            std::scoped_lock<std::mutex> lock(_mutex);
            for (auto it = _map.begin(); it != _map.end();) {
                if (f(*it))
                    it = _map.erase(it);
                else
                    ++it;
            }
        }

		void clear() {
            std::scoped_lock<std::mutex> lock(_mutex);
            _map.clear();
		}

        std::map<T, U, V> copy() {
            std::scoped_lock<std::mutex> lock(_mutex);
            std::map<T, U, V> r;
            r.insert(_map.cbegin(), _map.cend());
            return r;
        }
    };

    locked_vector<std::function<bool(const std::string&, const std::string&)>> _enter_callbacks;
    locked_vector<std::function<bool(const std::string&, const std::string&)>> _exit_callbacks;
    locked_vector<std::function<bool(const std::string&, const std::string&)>> _join_callbacks;
    locked_vector<std::function<bool(const std::string&, const std::string&)>> _leave_callbacks;

    locked_map<std::string, std::string> _connected_peers;       // peer_name, peer_uuid
    locked_map<std::string, std::set<std::string>> _peer_groups; // group name, peer_names
    locked_set<std::string> _own_groups;                         // group name

    // I don't like this, but since zyre/czmq does the memory management for these, I should store these as raw pointers
    zyre_t* _node;
    zactor_t* _actor;
    zpoller_t* _poller;

    // command containers
    locked_map<std::string, std::function<bool(std::stringstream&& args)>> _command_map; // callback name, callback
    locked_queue<std::pair<std::string, std::stringstream>> _command_queue;              // pair callback name, callback
    locked_map<std::string, std::string> _query_map;                                     // query, result

    locked_set<unsigned char> _response_keys; // ordered number of responses

    struct Query final {
        std::string query;
        unsigned __int64 benchmark;
        unsigned __int64 last;

        //Benchmarks[bmParseMacroParameter];

        Query() {
            benchmark = 0L;
            last = 0L;
        }

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
        Query& operator=(Query rhs) {
            swap(*this, rhs);
            return *this;
        }
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
        Observed& operator=(Observed rhs) {
            swap(*this, rhs);
            return *this;
        }
    };

    struct ObservedCompare final {
        bool operator()(const Observed& lhs, const Observed& rhs) const {
            if (lhs.query != rhs.query)
                return lhs.query < rhs.query;
            else
                return lhs.name < rhs.name;
        }
    };

    locked_map<unsigned int, Query> _observer_map;                    // group number, query
    locked_map<Observed, std::string, ObservedCompare> _observed_map; // maps query to group (for data access)
    locked_map<std::string, Observation> _observed_data;              // maps group to query result (could be empty)

    static void node_actor(zsock_t* pipe, void* args);
    const std::string observer_group(const unsigned int key);
    void queue_command(const std::string& command, std::stringstream&& args);

    locked_map<Observed, Observation, ObservedCompare> _query_result_map; // maps query to result (for data access)
    Observation _query_result;

    locked_set<std::string> _rejoin_groups;

    bool _debugging;
    bool _local_echo;
    bool _command_echo;
    bool _full_names;
    bool _front_delimiter;
    unsigned int _observe_delay;
    unsigned int _keepalive;
    unsigned int _evasive;
    bool _evasive_refresh;
    unsigned int _expired;
    unsigned __int64 _last_group_check;
    MQMacroBlock* _last_macro_check;
    bool _show_groups;

    // explicitly prevent copy/move operations.
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    Node();
    ~Node();

    // this is a private helper function ONLY THE STATIC ACTOR FUNCTION SHOULD CALL THIS
    std::string peer_uuid(const std::string& name) {
        std::string full_name = get_full_name(name);
        std::string uuid;
        zlist_t* peers = zyre_peers(_node);

        if (peers) {
            const char* z_peer = reinterpret_cast<const char*>(zlist_first(peers));
            while (z_peer) {
                std::string peer_name(zyre_peer_header_value(_node, z_peer, "name"));
                if (full_name == peer_name) {
                    uuid = z_peer;
                    break;
                }

                z_peer = reinterpret_cast<const char*>(zlist_next(peers));
            }

            zlist_destroy(&peers);
        }

        return uuid;
    }


public:
    // IMPORTANT: these are not exposed as an API, this is on purpose! We need a single point of control for our node (this plugin)
    std::string name() { return _node_name; }

    bool has_peer(const std::string& peer) {
        if (_node_name == get_full_name(peer))
            return true;

        return _connected_peers.contains(get_full_name(peer));
    }

    size_t peers() {
        return get_peers().size();
    }

    bool is_in_group(const std::string& group) {
        auto groups = get_own_groups();
        return groups.find(group) != groups.end();
    }

    // smartly reads/sets/clears _current_query
    Observation query(const std::string& name, const std::string& query);
    Observation query();
    void query_result(const std::string& name, const std::string& query, const Observation& obs);
    std::string trim_query(const std::string& query);
    std::string parse_query(const std::string& query);
    MQTypeVar parse_response(const std::string& output, const std::string& data);
    std::string peer_address(const std::string& name);

    bool debugging(bool debugging) {
        _debugging = debugging;
        return _debugging;
    }
    bool debugging() { return _debugging; }

    bool local_echo(bool local_echo) {
        _local_echo = local_echo;
        return _local_echo;
    }
    bool local_echo() { return _local_echo; }

    bool command_echo(bool command_echo) {
        _command_echo = command_echo;
        return _command_echo;
    }
    bool command_echo() { return _command_echo; }

    bool full_names(bool full_names) {
        _full_names = full_names;
        return _full_names;
    }
    bool full_names() { return _full_names; }

    bool front_delimiter(bool front_delimiter) {
        _front_delimiter = front_delimiter;
        return _front_delimiter;
    }
    bool front_delimiter() { return _front_delimiter; }

    unsigned int observe_delay(unsigned int observe_delay) {
        _observe_delay = observe_delay;
        return _observe_delay;
    }
    unsigned int observe_delay() { return _observe_delay; }

    unsigned int keepalive(unsigned int keepalive) {
        _keepalive = keepalive;
        if (_actor)
            zstr_sendx(_actor, "KEEPALIVE", std::to_string(keepalive).c_str(), NULL);
        return _keepalive;
    }
    unsigned int keepalive() { return _keepalive; }

    unsigned int evasive(unsigned int evasive) {
        _evasive = evasive;
        if (_actor)
            zstr_sendx(_actor, "EVASIVE", std::to_string(evasive).c_str(), NULL);
        return _evasive;
    }
    unsigned int evasive() { return _evasive; }

	bool evasive_refresh(bool evasive_refresh) {
        _evasive_refresh = evasive_refresh;
        return _evasive_refresh;
	}
    bool evasive_refresh() { return _evasive_refresh; }

    unsigned int expired(unsigned int expired) {
        _expired = expired;
        if (_actor)
            zstr_sendx(_actor, "EXPIRED", std::to_string(expired).c_str(), NULL);
        return _expired;
    }
    unsigned int expired() { return _expired; }

    unsigned __int64 last_group_check(unsigned __int64 last_group_check) {
        _last_group_check = last_group_check;
        return _last_group_check;
    }
    unsigned __int64 last_group_check() { return _last_group_check; }

    MQMacroBlock* last_macro_check(MQMacroBlock* last_macro_check) {
        _last_macro_check = last_macro_check;
        return _last_macro_check;
    }
    MQMacroBlock* last_macro_check() { return _last_macro_check; }

    bool show_groups(bool show_groups) {
        _show_groups = show_groups;
        return _show_groups;
    }
    bool show_groups() { return _show_groups; }

    void save_channels();

    void clear_saved_channels();

    void enter();
    void exit();
    void startup();
    void set_timeout(int timeout);
    void shutdown();
    void recv();

    void do_next();
    void remove_commands(const std::function<bool(std::pair<std::string, std::stringstream>&)>& f);
};
}

#pragma endregion

#pragma region CommandDefs

namespace MQ2DanNet {
COMMAND(Echo, const std::string& message)

COMMAND(Execute, const std::string& command)

// NOTE: Query is asynchronous
COMMAND(Query, const std::string& request)

COMMAND(Observe, const std::string& query, const std::string& output)

COMMAND(Update, const std::string& result)

COMMAND(Reupdate)
}

#pragma endregion

using namespace MQ2DanNet;

#pragma region Node

MQ2DANNET_NODE_API Node& Node::get() {
    static Node instance;
    return instance;
}

MQ2DANNET_NODE_API void Node::join(const std::string& group) {
    if (_actor) {
        zmsg_t* msg = zmsg_new();
        zmsg_pushstr(msg, group.c_str());
        zmsg_pushstr(msg, "JOIN");
        zmsg_send(&msg, _actor);
    }
}

MQ2DANNET_NODE_API void Node::leave(const std::string& group) {
    if (_actor) {
        zmsg_t* msg = zmsg_new();
        zmsg_pushstr(msg, group.c_str());
        zmsg_pushstr(msg, "LEAVE");
        zmsg_send(&msg, _actor);
    }
}

MQ2DANNET_NODE_API void MQ2DanNet::Node::on_join(std::function<bool(const std::string&, const std::string&)> callback) {
    _join_callbacks.push_back(std::move(callback));
}

MQ2DANNET_NODE_API void MQ2DanNet::Node::on_leave(std::function<bool(const std::string&, const std::string&)> callback) {
    _leave_callbacks.push_back(std::move(callback));
}

MQ2DANNET_NODE_API void Node::publish(const std::string& group, const std::string& cmd, std::stringstream&& args) {
    if (!_actor)
        return;

    args.seekg(0, args.end);
    size_t args_size = (size_t)args.tellg();
    args.seekg(0, args.beg);

    char* args_buf = new char[args_size];
    args.read(args_buf, args_size);

    zframe_t* args_frame = zframe_new(args_buf, args_size);

    zmsg_t* msg = zmsg_new();
    zmsg_prepend(msg, &args_frame);
    zmsg_pushstr(msg, cmd.c_str());

    zmsg_pushstr(msg, group.c_str());
    zmsg_pushstr(msg, "SHOUT");

    zmsg_send(&msg, _actor);

    delete[] args_buf;
}

MQ2DANNET_NODE_API void Node::respond(const std::string& name, const std::string& cmd, std::stringstream&& args) {
    if (!_actor)
        return;

    args.seekg(0, args.end);
    size_t args_size = (size_t)args.tellg();
    args.seekg(0, args.beg);

    char* args_buf = new char[args_size];
    args.read(args_buf, args_size);

    zframe_t* args_frame = zframe_new(args_buf, args_size);

    zmsg_t* msg = zmsg_new();
    zmsg_prepend(msg, &args_frame);
    zmsg_pushstr(msg, cmd.c_str());

    zmsg_pushstr(msg, name.c_str());
    zmsg_pushstr(msg, "WHISPER");

    zmsg_send(&msg, _actor);

    delete[] args_buf;
}

MQ2DANNET_NODE_API const std::list<std::string> Node::get_info() {
    if (!_actor)
        return std::list<std::string>{ "NONET" };

    std::list<std::string> output;
    std::set<std::string> groups = get_own_groups();
    output.push_back("CHANNELS: ");
    auto group_peers = get_group_peers();
    for (auto group : group_peers) {
        // this is our "observer" group filter
        if (group.first.find_first_of('_') != std::string::npos && std::isdigit(group.first.back()))
            continue;

        std::stringstream output_stream;

        if (groups.find(group.first) != groups.end()) {
            output_stream << " :: \ax\ag" << group.first << "\ax" << std::endl;
        } else {
            output_stream << " :: \ax\a-g" << group.first << "\ax" << std::endl;
        }

        for (auto peer : group.second) {
            if (_node_name == peer)
                output_stream << "\ax\aw";
            else
                output_stream << "\ax\a-w";

            std::string peer_out = get_name(peer);
            output_stream << peer_out << "\ax ";
        }

        output.push_back(output_stream.str());
    }

    return output;
}

MQ2DANNET_NODE_API const std::set<std::string> MQ2DanNet::Node::get_peers() {
    std::set<std::string> peers = _connected_peers.keys();
    peers.emplace(_node_name);

    return peers;
}

MQ2DANNET_NODE_API const std::set<std::string> MQ2DanNet::Node::get_all_groups() {
    std::set<std::string> groups = _peer_groups.keys();
    std::set<std::string> own_groups = _own_groups.copy();
    groups.insert(own_groups.cbegin(), own_groups.cend());

    return groups;
}

MQ2DANNET_NODE_API const std::set<std::string> MQ2DanNet::Node::get_own_groups() {
    return _own_groups.copy();
}

MQ2DANNET_NODE_API const std::map<std::string, std::set<std::string>> MQ2DanNet::Node::get_group_peers() {
    std::map<std::string, std::set<std::string>> group_peers;

    std::set<std::string> groups = get_all_groups();
    for (auto group : groups) {
        group_peers[group] = get_group_peers(group);
    }

    return group_peers;
}

MQ2DANNET_NODE_API const std::set<std::string> MQ2DanNet::Node::get_group_peers(const std::string& group) {
    std::set<std::string> peers = _peer_groups.get(group);

    if (is_in_group(group))
        peers.emplace(_node_name);

    return peers;
}

MQ2DANNET_NODE_API const std::set<std::string> MQ2DanNet::Node::get_peer_groups(const std::string& peer) {
    std::set<std::string> groups;

    _peer_groups.foreach ([&groups, peer](std::pair<std::string, std::set<std::string>> p) -> void {
        if (p.second.find(peer) != p.second.end())
            groups.emplace(p.first);
    });

    return groups;
}

MQ2DANNET_NODE_API const std::string MQ2DanNet::Node::get_interfaces() {
    const char* const current_iface = zsys_interface();
    const unsigned int current_iface_idx = (strlen(current_iface) == 1 && current_iface[0] >= '0' && current_iface[0] <= '9') ? GetIntFromString(current_iface, 0) : strlen(current_iface) == 0 ? 0 : -1;

    ziflist_t* l = ziflist_new_ipv6();
    std::string ifaces;

    const char* iface = ziflist_first(l);
    int iface_idx = -1;
    while (iface) {
        ++iface_idx;

        if ((ziflist_is_ipv6(l) && !zsys_ipv6()) || (!ziflist_is_ipv6(l) && zsys_ipv6()))
            continue;

        std::stringstream ifacestream;
        if ((current_iface_idx >= 0 && current_iface_idx == iface_idx) || streq(current_iface, iface))
            ifacestream << " --> ";
        else
            ifacestream << "     ";
        ifacestream << iface_idx << " [" << iface << "] " << ziflist_address(l) << " -- " << ziflist_broadcast(l) << "\r\n";
        ifaces += ifacestream.str();
        iface = ziflist_next(l);
    }
    ziflist_destroy(&l);

    return ifaces;
}

MQ2DANNET_NODE_API const std::string MQ2DanNet::Node::get_full_name(const std::string& name) {
    std::string ret = name;

    // this works because names and servers can't have underscores in them, therefore if
    // there is no underscore in the string, we assume a local character name was passed
    if (std::string::npos == name.find_last_of("_")) {
        ret = GetServerShortName() + std::string("_") + ret;
    }

    return init_string(ret.c_str());
}

MQ2DANNET_NODE_API const std::string MQ2DanNet::Node::get_short_name(const std::string& name) {
    std::string ret = name;
    size_t pos = name.find_last_of("_");

    if (pos != std::string::npos && name.find_first_of(GetServerShortName()) != std::string::npos) {
        ret = name.substr(pos + 1);
    }

    return init_string(ret.c_str());
}

MQ2DANNET_NODE_API const std::string MQ2DanNet::Node::get_name(const std::string& name) {
    if (full_names()) {
        return get_full_name(name);
    } else {
        return get_short_name(name);
    }
}

void Node::node_actor(zsock_t* pipe, void* args) {
    Node* node = reinterpret_cast<Node*>(args);
    if (!node)
        return;

    node->_node = zyre_new(node->_node_name.c_str());
    if (!node->_node) {
        throw new std::invalid_argument("Could not create node");
    }

    CHAR szBuf[MAX_STRING] = { 0 };
    GetPrivateProfileString("General", "Interface", NULL, szBuf, MAX_STRING, INIFileName);
    if (szBuf && szBuf[0] != '\0')
        zyre_set_interface(node->_node, szBuf);

    // send our node name for easier name recognition
    zyre_set_header(node->_node, "name", "%s", node->_node_name.c_str());
    zyre_start(node->_node);
    if (node->evasive() > 0)
        zyre_set_evasive_timeout(node->_node, node->evasive());
    if (node->expired() > 0)
        zyre_set_expired_timeout(node->_node, node->expired());
    unsigned int keepalive = node->keepalive() > 0 ? node->keepalive() : 30000;

    zsock_signal(pipe, 0); // ready signal, required by zactor contract

    auto my_sock = zyre_socket(node->_node);
    zpoller_t* poller = zpoller_new(pipe, my_sock, (void*)NULL);

    std::set<std::string> groups = node->_rejoin_groups.copy();
    node->_rejoin_groups.clear();

    for (auto group : groups) {
        node->_own_groups.emplace(group);
        zyre_join(node->_node, group.c_str());
    }

    // TODO: This doesn't appear necessary, but experiment with it
    //zpoller_set_nonstop(poller, true);

    DebugSpewAlways("Starting actor loop for %s : %s", node->_node_name.c_str(), zyre_uuid(node->_node));

    bool terminated = false;
    while (!terminated) {
        void* which = zpoller_wait(poller, keepalive);

        if (!which || !pipe || !node || !node->_node) {
            if (!pipe || !zpoller_expired(poller) || zstr_send(pipe, "PING") == -1)
                terminated = true;
        } else if (which == pipe) {
            // we've got a command from the caller here
            //DebugSpewAlways("Got message from caller");
            zmsg_t* msg = zmsg_recv(which);
            if (!msg)
                continue; // Interrupted

            // strings index commands because zeromq has the infrastructure and it's not time-critical
            // otherwise, we'd have to deal with byte streams, which is totally unnecessary
            char* command = zmsg_popstr(msg);

            //DebugSpewAlways("MQ2DanNet: command: %s", command);

            // IMPORTANT: local commands are all caps, Remote commands will be passed to this as their class name
            if (streq(command, "$TERM")) { // need to handle $TERM per zactor contract
                terminated = true;
            } else if (streq(command, "JOIN")) {
                char* group = zmsg_popstr(msg);
                if (group) {
                    node->_own_groups.emplace(group);
                    zyre_join(node->_node, group);
                    zstr_free(&group);
                }
            } else if (streq(command, "LEAVE")) {
                char* group = zmsg_popstr(msg);
                if (group) {
                    node->_own_groups.erase(group);
                    zyre_leave(node->_node, group);
                    zstr_free(&group);
                }
            } else if (streq(command, "SHOUT")) {
                char* group = zmsg_popstr(msg);
                if (group) {
                    zyre_shout(node->_node, group, &msg);
                    zstr_free(&group);
                }
            } else if (streq(command, "WHISPER")) {
                char* name = zmsg_popstr(msg);
                if (name) {
                    std::string uuid = node->peer_uuid(name);
                    zstr_free(&name);
                    if (!uuid.empty())
                        zyre_whisper(node->_node, uuid.c_str(), &msg);
                }
            } else if (streq(command, "PEER")) {
                char* name = zmsg_popstr(msg);
                std::string uuid;
                if (name) {
                    uuid = node->peer_uuid(name);
                    zstr_free(&name);
                }

                zstr_send(pipe, uuid.c_str());
            } else if (streq(command, "PEERS")) {
                zlist_t* peer_ids = zyre_peers(node->_node);
                zmsg_t* peers = zmsg_new();
                if (peer_ids) {
                    const char* peer_id = reinterpret_cast<const char*>(zlist_first(peer_ids));
                    while (peer_id) {
                        char* name = zyre_peer_header_value(node->_node, peer_id, "name");
                        if (name)
                            zmsg_pushstr(peers, name);
                        peer_id = reinterpret_cast<const char*>(zlist_next(peer_ids));
                    }

                    zlist_destroy(&peer_ids);
                }

                if (zmsg_size(peers) == 0)
                    zmsg_pushstr(peers, "0");
                zmsg_send(&peers, pipe);
            } else if (streq(command, "PEER_GROUPS")) {
                zlist_t* peer_groups = zyre_peer_groups(node->_node);
                zmsg_t* groups = zmsg_new();
                if (peer_groups) {
                    const char* peer_group = reinterpret_cast<const char*>(zlist_first(peer_groups));
                    while (peer_group) {
                        zmsg_pushstr(groups, peer_group);
                        peer_group = reinterpret_cast<const char*>(zlist_next(peer_groups));
                    }

                    zlist_destroy(&peer_groups);
                }

                if (zmsg_size(groups) == 0)
                    zmsg_pushstr(groups, "");
                zmsg_send(&groups, pipe);
            } else if (streq(command, "OWN_GROUPS")) {
                zlist_t* own_groups = zyre_own_groups(node->_node);
                zmsg_t* groups = zmsg_new();
                if (own_groups) {
                    const char* peer_group = reinterpret_cast<const char*>(zlist_first(own_groups));
                    while (peer_group) {
                        zmsg_pushstr(groups, peer_group);
                        peer_group = reinterpret_cast<const char*>(zlist_next(own_groups));
                    }

                    zlist_destroy(&own_groups);
                }

                if (zmsg_size(groups) == 0)
                    zmsg_pushstr(groups, "");
                zmsg_send(&groups, pipe);
            } else if (streq(command, "PEERS_BY_GROUP")) {
                char* group = zmsg_popstr(msg);
                zmsg_t* peers = zmsg_new();
                if (group) {
                    zlist_t* peer_ids = zyre_peers_by_group(node->_node, group);
                    if (peer_ids) {
                        const char* peer_id = reinterpret_cast<const char*>(zlist_first(peer_ids));
                        while (peer_id) {
                            char* name = zyre_peer_header_value(node->_node, peer_id, "name");
                            zmsg_pushstr(peers, name);
                            peer_id = reinterpret_cast<const char*>(zlist_next(peer_ids));
                        }

                        zlist_destroy(&peer_ids);
                    }
                }

                if (group)
                    zstr_free(&group);
                if (zmsg_size(peers) == 0)
                    zmsg_pushstr(peers, "");
                zmsg_send(&peers, pipe);
            } else if (streq(command, "PEER_ADDRESS")) {
                char* name = zmsg_popstr(msg);
                zmsg_t* address = zmsg_new();

                std::string uuid;
                if (name) {
                    uuid = node->peer_uuid(name);
                    zstr_free(&name);
                }

                if (!uuid.empty()) {
                    char* addr = zyre_peer_address(node->_node, uuid.c_str());
                    if (addr) {
                        zmsg_pushstr(address, addr);
                        zstr_free(&addr);
                    }
                }

                if (zmsg_size(address) == 0)
                    zmsg_pushstr(address, "");
                zmsg_send(&address, pipe);
            } else if (streq(command, "EVASIVE")) {
                char* szEvasive = zmsg_popstr(msg);
                if (IsNumber(szEvasive)) {
                    zyre_set_evasive_timeout(node->_node, node->evasive());
                } else if (szEvasive) {
                    DebugSpewAlways("EVASIVE: Trying to set non-numeric %s.", szEvasive);
                } else {
                    DebugSpewAlways("EVASIVE: Trying to set null.");
                }
            } else if (streq(command, "EXPIRED")) {
                char* szExpired = zmsg_popstr(msg);
                if (IsNumber(szExpired)) {
                    zyre_set_expired_timeout(node->_node, node->expired());
                } else if (szExpired) {
                    DebugSpewAlways("EXPIRED: Trying to set non-numeric %s.", szExpired);
                } else {
                    DebugSpewAlways("EXPIRED: Trying to set null.");
                }
            } else if (streq(command, "KEEPALIVE")) {
                char* szKeepalive = zmsg_popstr(msg);
                if (IsNumber(szKeepalive)) {
                    zyre_set_expired_timeout(node->_node, GetIntFromString(szKeepalive, 0));
                } else if (szKeepalive) {
                    DebugSpewAlways("KEEPALIVE: Trying to set non-numeric %s.", szKeepalive);
                } else {
                    DebugSpewAlways("KEEPALIVE: Trying to set null.");
                }

                if (szKeepalive)
                    zstr_free(&szKeepalive);
            } else if (streq(command, "PING")) {
                zstr_send(pipe, "PONG");
            } else if (streq(command, "PONG")) {
				// TODO: we can potentially track keepalive responses, but for now let's just discard this
            } else {
                zframe_t* body = zmsg_pop(msg);
                char* name = zmsg_popstr(msg);
                char* group = zmsg_popstr(msg);

                if (body) {
                    std::stringstream args;
                    Archive<std::stringstream> args_ar(args);
                    args_ar << std::string(name ? name : "") << std::string(group ? group : "");
                    char* body_data = (char*)zframe_data(body);
                    size_t body_size = zframe_size(body);

                    args.write(body_data, body_size);

                    node->queue_command(command, std::move(args));
                } else {
                    DebugSpewAlways("MQ2DanNet: Empty %s message in pipe handler: group %s, name %s, body %s.", command, group, name, body);
                }

                if (group)
                    zstr_free(&group);
                if (name)
                    zstr_free(&name);
                if (body)
                    zframe_destroy(&body);
            }

            if (command)
                zstr_free(&command);
            if (msg)
                zmsg_destroy(&msg);
        } else if (which == zyre_socket(node->_node)) {
            // we've received something over our socket
            //DebugSpewAlways("Got a message over the socket");
            zyre_event_t* z_event = zyre_event_new(node->_node);
            if (!z_event)
                continue;

            const char* szEventType = zyre_event_type(z_event);
            std::string event_type(szEventType ? szEventType : ""); // don't use init_string() because we don't want to make lower
            std::string name = init_string(zyre_event_peer_name(z_event));

            if (event_type.empty()) {
                DebugSpewAlways("MQ2DanNet: Got zyre message with empty event type!");
            } else if (name.empty()) {
                DebugSpewAlways("MQ2DanNet: Got %s message with empty name!", event_type.c_str());
            } else if (event_type == "ENTER") {
                // TODO: can possibly do something with headers here (`zyre_event_headers(z_event)`)
                // can also harvest the IP:port if we need it
                std::string uuid = init_string(zyre_event_peer_uuid(z_event));
                if (uuid.empty()) {
                    DebugSpewAlways("MQ2DanNet: ENTER with empty UUID for name %s, will not add to peers list.", name.c_str());
                } else {
                    node->_connected_peers.upsert(name, uuid);
                }
                //DebugSpewAlways("%s is ENTERing.", name.c_str());
            } else if (event_type == "EXIT") {
                node->_connected_peers.erase(name);

                std::map<std::string, std::set<std::string>> new_groups;
                node->_peer_groups.foreach ([&name, &new_groups](std::pair<std::string, std::set<std::string>> group) -> void {
                    std::set<std::string> new_set = group.second;
                    if (new_set.erase(name) > 0) {
                        new_groups[group.first] = new_set;
                    }
                });

                for (auto it = new_groups.begin(); it != new_groups.end(); ++it) {
                    node->_peer_groups.upsert(it->first, it->second);
                }

                //DebugSpewAlways("%s is EXITing.", name.c_str());
            } else if (event_type == "JOIN") {
                std::string group = init_string(zyre_event_group(z_event));

                if (group.empty()) {
                    DebugSpewAlways("MQ2DanNet: JOIN with empty group with name %s, will not add to lists.", name.c_str());
                } else {
                    node->_join_callbacks.remove_if([&name, &group](std::function<bool(const std::string&, const std::string&)> f) -> bool {
                        return f(name, group);
                    });

                    node->_peer_groups.upsert(group, [&name](std::set<std::string>& group) -> void {
                        group.emplace(name);
                    });
                    //DebugSpewAlways("JOIN %s : %s", group.c_str(), name.c_str());
                }
            } else if (event_type == "LEAVE") {
                std::string group = init_string(zyre_event_group(z_event));

                if (group.empty()) {
                    DebugSpewAlways("MQ2DanNet: LEAVE with empty group with name %s, will not remove from lists.", name.c_str());
                } else {
                    node->_leave_callbacks.remove_if([&name, &group](std::function<bool(const std::string&, const std::string&)> f) -> bool {
                        return f(name, group);
                    });
                    node->_peer_groups.erase_if(group, [&name](std::set<std::string>& group) -> bool {
                        group.erase(name);
                        return group.empty();
                    });
                    //DebugSpewAlways("LEAVE %s : %s", group.c_str(), name.c_str());
                }
            } else if (event_type == "WHISPER") {
                // use get_msg because we want ownership to pass the command up
                zmsg_t* message = zyre_event_get_msg(z_event);
                if (!message) {
                    DebugSpewAlways("MQ2DanNet: Got NULL WHISPER message from %s", name.c_str());
                } else {
                    zmsg_addstr(message, name.c_str());
                    zmsg_send(&message, node->_actor);
                }
            } else if (event_type == "SHOUT") {
                // this presumes that group will return NULL if not a shot, which is valid in zyre if we don't set ZYRE_DEBUG or ZYRE_PEDANTIC
                std::string group = init_string(zyre_event_group(z_event));

                if (group.empty()) {
                    DebugSpewAlways("MQ2DanNet: SHOUT with empty group from %s, not passing message.", name.c_str());
                } else {
                    // use get_msg because we want ownership to pass the command up
                    zmsg_t* message = zyre_event_get_msg(z_event);
                    if (!message) {
                        DebugSpewAlways("MQ2DanNet: Got NULL SHOUT message from %s in %s", name.c_str(), group.c_str());
                    } else {
                        // note that this goes to the end of the message
                        zmsg_addstr(message, name.c_str());
                        zmsg_addstr(message, group.c_str());
                        zmsg_send(&message, node->_actor);
                    }
                }
            } else if (event_type == "EVASIVE" || event_type == "SILENT") {
                // not sure if anything needs to be done here?
                // also, turns out this is done a lot so let's just mute it to reduce spam
                //TODO: need to maintain a keepalive list so we can remove peers that have disconnected (how to force remove peers? it might be a command to the actor, look this up.)
                auto tick = MQGetTickCount64();
                //zlist_t *peer_ids = zyre_peers(node->_node);
                //if (peer_ids) {
                //    const char *peer_id = reinterpret_cast<const char*>(zlist_first(peer_ids));
                //    while (peer_id) {
                //        char *peer = zyre_peer_header_value(node->_node, peer_id, "name");
                //        if (peer)
                //            DebugSpewAlways("PEER: %s", peer);
                //        peer_id = reinterpret_cast<const char*>(zlist_next(peer_ids));
                //    }

                //    zlist_destroy(&peer_ids);
                //}

                DebugSpewAlways("%s is being %s at %ull", name.c_str(), event_type.c_str(), tick);
                if (node->_evasive_refresh) node->whisper<Reupdate>(name);
            } else {
                DebugSpewAlways("MQ2DanNet: Got unhandled event type %s.", event_type.c_str());
            }

            zyre_event_destroy(&z_event);
        }
    }

    zpoller_destroy(&poller);

    zlist_t* own_groups = zyre_own_groups(node->_node);
    if (own_groups) {
        const char* peer_group = reinterpret_cast<const char*>(zlist_first(own_groups));
        while (peer_group) {
            zyre_leave(node->_node, peer_group);
            peer_group = reinterpret_cast<const char*>(zlist_next(own_groups));
        }

        zlist_destroy(&own_groups);
    }
    node->_own_groups.clear();

    zyre_stop(node->_node);
    zclock_sleep(100);
    zyre_destroy(&node->_node);
    zpoller_destroy(&node->_poller);

    zclock_sleep(100);
}

std::string Node::init_string(const char* szStr) {
    if (szStr) {
        std::string str(szStr);
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    }

    return std::string();
}

MQ2DANNET_NODE_API std::string MQ2DanNet::Node::register_response(std::function<bool(std::stringstream&&)> callback) {
    // C99, 6.2.5p9 -- guarantees that this will wrap to 0 once we reach max value
    unsigned char next_val = _response_keys.get_next([](unsigned char key) -> unsigned char {
        return key + 1;
    });
    _response_keys.insert(next_val);
    std::string key = "response_" + std::to_string((unsigned int)next_val);

    register_command(key, callback);
    return key;
}

// this is pretty much fire and forget. We could potentially have a bunch of vacant observers, but don't worry about that, let's just test it.
// if we have to start dropping observer groups, then we need to figure out a way to gracefully handle desyncs
// potentially on_join if no group is available, have the client re-register?
MQ2DANNET_NODE_API std::string MQ2DanNet::Node::register_observer(const std::string& name, const std::string& query) {
    // first search for the key in the map already
    for (auto observer : _observer_map.copy()) {
        if (observer.second.query == query)
            return observer_group(observer.first);
    }

    // didn't find anything, insert a new one
    Query obs(query);

    unsigned int position = _observer_map.upsert_wrap(std::move(obs), [](unsigned int p) -> unsigned int {
        return p + 1;
    });

    return observer_group(position);
}

MQ2DANNET_NODE_API void MQ2DanNet::Node::unregister_observer(const std::string& query) {
    _observer_map.erase_if([&query](std::pair<unsigned int, Query> p) -> bool {
        return p.second.query == query;
    });
}

MQ2DANNET_NODE_API void MQ2DanNet::Node::observe(const std::string& group, const std::string& name, const std::string& query) {
    join(group);
    _observed_map.upsert(Observed(query, name), group);
}

MQ2DANNET_NODE_API void MQ2DanNet::Node::forget(const std::string& group) {
    _observed_map.erase_if([&group](auto p) -> bool {
        return p.second == group;
    });

    _observed_data.erase(group);

    leave(group);
}

MQ2DANNET_NODE_API void MQ2DanNet::Node::forget(const std::string& name, const std::string& query) {
    Observed observed = Observed(query, name);
    _observed_map.foreach ([this, observed](std::pair<Observed, std::string> pair) -> void {
        if (pair.first.query == observed.query && pair.first.name == observed.name) {
            _observed_data.erase(pair.second);
            leave(pair.second);
        }
    });
    _observed_map.erase(observed);
}

MQ2DANNET_NODE_API void MQ2DanNet::Node::forget_all(const std::string& name) {
    std::list<Observed> to_drop;
    _observed_map.foreach ([this, &name, &to_drop](std::pair<Observed, std::string> pair) -> void {
        if (pair.first.name == name) {
            _observed_data.erase(pair.second);
            leave(pair.second);
            to_drop.push_back(pair.first);
        }
    });

    for (auto drop : to_drop) {
        _observed_map.erase(drop);
    }
}

MQ2DANNET_NODE_API void MQ2DanNet::Node::forget_if(bool (*predicate)(const Observation& observation)) {
    std::list<std::string> to_drop; // list of group names to drop
    _observed_data.foreach ([this, &predicate, &to_drop](std::pair<std::string, Observation> pair) -> void {
        if (predicate(pair.second)) {
            to_drop.push_back(pair.first);
        }
    });

    for (auto drop : to_drop) {
        forget(drop);
    }
}

MQ2DANNET_NODE_API void MQ2DanNet::Node::update(const std::string& group, const std::string& data, const std::string& output) {
    _observed_data.upsert(group, Observation(output, data, MQGetTickCount64()));
}

MQ2DANNET_NODE_API const Node::Observation MQ2DanNet::Node::read(const std::string& group) {
    return _observed_data.get(group);
}

MQ2DANNET_NODE_API const Node::Observation MQ2DanNet::Node::read(const std::string& name, const std::string& query) {
    // this is safe because these get return default constructed objects if they aren't found
    return _observed_data.get(_observed_map.get(Observed(query, name)));
}

MQ2DANNET_NODE_API bool MQ2DanNet::Node::can_read(const std::string& name, const std::string& query) {
    return _observed_map.contains(Observed(query, name));
}

MQ2DANNET_NODE_API size_t MQ2DanNet::Node::observed_count(const std::string& name) {
    std::set<Observed, ObservedCompare> keys = _observed_map.keys();
    std::list<Observed> observed;
    std::copy_if(keys.cbegin(), keys.cend(), std::inserter(observed, observed.begin()),
        [&name](const Observed& obs) { return obs.name == name; });

    return observed.size();
}

MQ2DANNET_NODE_API std::set<std::string> MQ2DanNet::Node::observed_queries(const std::string& name) {
    std::set<Observed, ObservedCompare> keys = _observed_map.keys();
    std::set<Observed, ObservedCompare> observed;
    std::copy_if(keys.cbegin(), keys.cend(), std::inserter(observed, observed.begin()),
        [&name](const Observed& obs) { return obs.name == name; });

    std::set<std::string> queries;
    std::transform(observed.cbegin(), observed.cend(), std::inserter(queries, queries.begin()),
        [](const Observed& obs) { return obs.query; });

    return queries;
}

MQ2DANNET_NODE_API size_t MQ2DanNet::Node::observer_count() {
    return _observer_map.keys().size();
}

MQ2DANNET_NODE_API std::set<std::string> MQ2DanNet::Node::observer_queries() {
    std::list<Query> values = _observer_map.values();
    std::set<std::string> queries;
    std::transform(values.cbegin(), values.cend(), std::inserter(queries, queries.begin()),
        [](const Query& query) { return query.query; });

    return queries;
}

MQ2DANNET_NODE_API std::set<std::string> MQ2DanNet::Node::observers(const std::string& query) {
    for (auto observer : _observer_map.copy()) {
        if (observer.second.query == query) {
            return get_group_peers(Node::get().observer_group(observer.first));
        }
    }

    return std::set<std::string>();
}

// stub these for now, nothing to do here since memory is managed elsewhere (and all registered commands will go away)
Node::Node() = default;
Node::~Node() = default;

Node::Observation MQ2DanNet::Node::query(const std::string& name, const std::string& query){
    // loop through results and find the entry where peer name and query matches
    std::string final_query = trim_query(query);
    std::string final_name = get_full_name(name);
    return _query_result_map.get(Observed(final_name, final_query));
}

Node::Observation MQ2DanNet::Node::query() {
    // this function is purely for backwards compat with the previous macro-centric design
    return _query_result;
}

void MQ2DanNet::Node::query_result(const std::string& name, const std::string& query, const Observation& obs) {
    _query_result = obs; // store the latest result for easy compat with macros

    // upsert a query from a peer
    std::string final_query = trim_query(query);
    std::string final_name = get_full_name(name);
    _query_result_map.upsert(Observed(final_name, final_query), obs);
}

std::string MQ2DanNet::Node::trim_query(const std::string& query) {
    std::string final_query = std::regex_replace(query, std::regex("\\$\\\\\\{"), "${");

    if (final_query.front() == '"')
        final_query.erase(final_query.begin(), final_query.begin() + 1);

    if (final_query.back() == '"')
        final_query.erase(final_query.end() - 1);

    if (final_query.find_first_of("${") == 0)
        final_query.erase(final_query.begin(), final_query.begin() + 2);

    if (final_query.back() == '}')
        final_query.erase(final_query.end() - 1);

    return final_query;
}

std::string MQ2DanNet::Node::parse_query(const std::string& query) {
    CHAR szQuery[MAX_STRING];
    strcpy_s(szQuery, ("${" + query + "}").c_str());

    ParseMacroData(szQuery, MAX_STRING);
    return szQuery;
}

MQTypeVar MQ2DanNet::Node::parse_response(const std::string& output, const std::string& data) {
    // we need to pass a string data into here because we need to make sure that the output type can handle
    // the data we give it, which is handled in `FromString`, and if we aren't in a macro we are just going
    // to write it out anyway.

    if (!output.empty() && gMacroBlock) { // let's make sure a macro is running here
        CHAR szOutput[MAX_STRING] = { 0 };
        strcpy_s(szOutput, output.c_str());
        MQDataVar* pVar = FindMQ2DataVariable(szOutput);
        if (pVar) {
            CHAR szData[MAX_STRING] = { 0 };
            strcpy_s(szData, data.c_str());
            if (!pVar->Var.Type->FromString(pVar->Var.VarPtr, szData)) {
                MacroError("/dquery: setting '%s' failed, variable type rejected new value of %s", szOutput, szData);
            }

            if (pVar)
                return pVar->Var;
        } else {
            MacroError("/dquery failed, variable '%s' not found", szOutput);
        }
    } else {
        // if we aren't in a macro or we have no output, we are dealing with a string
        MQTypeVar Result;
        strcpy_s(DataTypeTemp, data.c_str());
        Result.Ptr = &DataTypeTemp[0];
        Result.Type = mq::datatypes::pStringType;
        if (debugging())
            WriteChatf("%s", data.c_str());

        return Result;
    }

    MQTypeVar Result;
    Result.Type = 0;
    Result.Int64 = 0;
    return Result;
}

std::string MQ2DanNet::Node::peer_address(const std::string& name) {
    return _connected_peers.get(name);
}

void MQ2DanNet::Node::save_channels() {
    for (auto group : get_own_groups()) {
        _rejoin_groups.insert(group);
    }
}

void MQ2DanNet::Node::clear_saved_channels() {
    _rejoin_groups.clear();
}

void Node::enter() {
    PCHARINFO pChar = GetCharInfo();
    if (!pChar)
        return;

	if (IsMainThread()) {
        if (_actor) {
            DebugSpewAlways("Already had actor for %s", _node_name.c_str());
            zactor_destroy(&_actor);
        }

        _node_name = get_full_name(pChar->Name);

        DebugSpewAlways("Spinning up actor for %s", _node_name.c_str());
        _actor = zactor_new(Node::node_actor, this);

        if (_actor) {
            if (_poller) {
                zpoller_destroy(&_poller);
            }

            _poller = zpoller_new(_actor, (void*)NULL);
            if (!_poller)
                throw new std::invalid_argument("Could not create poller");
        }
    } else {
        PostToMainThread([this]() { this->enter(); });
	}
}

void Node::exit() {
    if (IsMainThread()) {
        if (_actor) {
            DebugSpewAlways("Destroying actor for %s", _node_name.c_str());
            zactor_destroy(&_actor);
        } else if (_node || _poller) {
            // in general destroying the zactor will do this, but just in case it's dangling, let's be safe
            // it's possible that the actor is in the process of destruction, so let's make sure the lock
            // has been released before attempting to destroy the constituents
            if (_node) {
                DebugSpewAlways("WARNING: had a node without an actor in %s", _node_name.c_str());
                zyre_destroy(&_node);
            }

            if (_poller) {
                DebugSpewAlways("WARNING: had a poller without an actor in %s", _node_name.c_str());
                zpoller_destroy(&_poller);
            }
        }

        _node_name = "";
	} else {
        PostToMainThread([this]() { this->exit(); });
	}
}

void MQ2DanNet::Node::startup() {
    if (IsMainThread()) {
        // ensure that startup has happened so that we can put our atexit at the proper place in the exit function queue
        zsys_init();
        //atexit([]() -> void {});
    } else {
        PostToMainThread([this]() { this->startup(); });
	}
}

void MQ2DanNet::Node::set_timeout(int timeout) {
    if (IsMainThread()) {
        zmq_setsockopt(_actor, ZMQ_RCVTIMEO, "", timeout);
    } else {
        PostToMainThread([this, timeout]() { this->set_timeout(timeout); });
	}
}

void MQ2DanNet::Node::shutdown() {
    if (IsMainThread()) {
        zsys_shutdown();
    } else {
        PostToMainThread([this]() { this->shutdown(); });
	}
}

void MQ2DanNet::Node::recv() {
    if (!_poller)
        return;

    void* which = zpoller_wait(_poller, 0);
    if (which) {
        // we currently only expect commands here for a keepalive -- this can be expanded to a full heartbeat if necessary
        zmsg_t* msg = zmsg_recv(which);
        if (msg) {
            char* command = zmsg_popstr(msg);
            if (command) {
                if (streq(command, "PING"))
                    zstr_send(_actor, "PONG");
				// TODO: can potentially handle PONG here like in the actor thread, but for now let's discard it
                zstr_free(&command);
            }
            zmsg_destroy(&msg);
        }
    }
}

void Node::queue_command(const std::string& command, std::stringstream&& args) {
    // defer the actual lookup to the execution so we can handle commands that remove themselves
    _command_queue.emplace(std::make_pair(command, std::move(args)));
}

const std::string MQ2DanNet::Node::observer_group(const unsigned int key) {
    return _node_name + "_" + init_string(std::to_string(key).c_str());
}

void Node::do_next() {
    std::pair<std::string, std::stringstream> command_pair = _command_queue.pop();
    _command_map.erase_if(command_pair.first, [&command_pair](std::function<bool(std::stringstream &&)> f) -> bool {
        return f(std::move(command_pair.second));
    });
}

void Node::remove_commands(const std::function<bool(std::pair<std::string, std::stringstream>&)>& f) {
    _command_queue.remove_if(f);
}

#pragma endregion

#pragma region Commands

const bool MQ2DanNet::Echo::callback(std::stringstream&& args) {
    Archive<std::stringstream> received(args);
    std::string from;
    std::string group;
    std::string text;

    try {
        received >> from >> group >> text;
        from = Node::get().get_name(from);
        //DebugSpewAlways("ECHO --> FROM: %s, GROUP: %s, TEXT: %s", from.c_str(), group.c_str(), text.c_str());

        if (group.empty() || !Node::get().show_groups())
            WriteChatf("\ax\a-t[\ax\at %s \ax\a-t]\ax \aw%s\ax", from.c_str(), text.c_str());
        else
            WriteChatf("\ax\a-t[\ax\at %s\ax\a-t (%s) ]\ax \aw%s\ax", from.c_str(), group.c_str(), text.c_str());

        return false;
    } catch (std::runtime_error&) {
        DebugSpewAlways("MQ2DanNet::Echo -- Failed to deserialize.");
        return false;
    }
}

std::stringstream MQ2DanNet::Echo::pack(const std::string& recipient, const std::string& message) {
    std::stringstream send_stream;
    Archive<std::stringstream> send(send_stream);
    send << message;

    return send_stream;
}

const bool MQ2DanNet::Execute::callback(std::stringstream&& args) {
    Archive<std::stringstream> received(args);
    std::string from;
    std::string group;
    std::string command;

    try {
        received >> from >> group >> command;
        //DebugSpewAlways("EXECUTE --> FROM: %s, GROUP: %s, TEXT: %s", from.c_str(), group.c_str(), command.c_str());

        std::string final_command = std::regex_replace(command, std::regex("\\$\\\\\\{"), "${");

        if (Node::get().command_echo()) {
            if (group.empty()) {
                WriteChatf("\ax\a-o[\ax\ao %s \ax\a-o]\ax \aw%s\ax", from.c_str(), final_command.c_str());
            } else {
                WriteChatf("\ax\a-o[\ax\ao %s\ax\a-o (%s) ]\ax \aw%s\ax", from.c_str(), group.c_str(), final_command.c_str());
            }
        }

        CHAR szCommand[MAX_STRING] = { 0 };
        strcpy_s(szCommand, final_command.c_str());
        EzCommand(szCommand);

        return false;
    } catch (std::runtime_error&) {
        DebugSpewAlways("MQ2DanNet::Echo -- Failed to deserialize.");
        return false;
    }
}

std::stringstream MQ2DanNet::Execute::pack(const std::string& recipient, const std::string& command) {
    std::stringstream send_stream;
    Archive<std::stringstream> send(send_stream);
    send << command;

    return send_stream;
}

const bool MQ2DanNet::Query::callback(std::stringstream&& args) {
    Archive<std::stringstream> received(args);
    std::string from;
    std::string group; // this is irrelevant, but we need to pull the parameter anyway.
    std::string key;
    std::string request;

    try {
        received >> from >> group >> key >> request;
        //DebugSpewAlways("QUERY --> FROM: %s, GROUP: %s, REQUEST: %s", from.c_str(), group.c_str(), request.c_str());

        std::stringstream send_stream;
        Archive<std::stringstream> send(send_stream);

        send << Node::get().parse_query(request);
        Node::get().respond(from, key, std::move(send_stream));

        return false;
    } catch (std::runtime_error&) {
        DebugSpewAlways("MQ2DanNet::Query -- Failed to deserialize.");
        return false;
    }
}

// we're going to generate a new command and register it with Node here in addition to packing
std::stringstream MQ2DanNet::Query::pack(const std::string& recipient, const std::string& request) {
    std::stringstream send_stream;
    Archive<std::stringstream> send(send_stream);

    // now we make a callback for the Query command that sets the variable
    auto f = [request](std::stringstream&& args) -> bool {
        Archive<std::stringstream> ar(args);
        std::string from;
        std::string group;
        std::string data;

        try {
            ar >> from >> group >> data;

            std::string output = Node::get().query(from, request).output;
            MQTypeVar Result = Node::get().parse_response(output, data);

            // this actually only determines when the delay breaks.
            CHAR szBuf[MAX_STRING] = { 0 };
            if (Result.Type)
                Result.Type->ToString(Result.VarPtr, szBuf);
            else
                strcpy_s(szBuf, "NULL");
            Node::get().query_result(from, request, Node::Observation(output, std::string(szBuf), MQGetTickCount64()));

            if (Node::get().debugging()) {
                if (Result.Type) {
                    CHAR szData[MAX_STRING] = { 0 };
                    Result.Type->ToString(Result.VarPtr, szData);
                    WriteChatf("%s : %s -- %llu (%llu)", Result.Type->GetName(), szData, Node::get().query(from, request).received, MQGetTickCount64());
                } else
                    WriteChatf("Failed to read data %s into %s at %llu.", data.c_str(), output.c_str(), MQGetTickCount64());
            }
        } catch (std::runtime_error&) {
            DebugSpewAlways("MQ2DanNet::Query -- response -- Failed to deserialize.");
        }

        return true;
    };

    std::string key = Node::get().register_response(f);
    send << key << request;

    return send_stream;
}

// this is the callback for the observable, so add to map and send back the result group to the requester
const bool MQ2DanNet::Observe::callback(std::stringstream&& args) {
    Archive<std::stringstream> received(args);
    std::string from;
    std::string group;
    std::string key;
    std::string query;

    try {
        received >> from >> group >> key >> query;
        //DebugSpewAlways("OBSERVE --> FROM: %s, GROUP: %s, QUERY: %s", from.c_str(), group.c_str(), query.c_str());

        std::stringstream args;
        Archive<std::stringstream> ar(args);

        // This can install invalid queries, which is by design. We have no way to determine when some queries are valid or invalid
        ar << Node::get().register_observer(from, query) << Node::get().parse_query(query);

        Node::get().respond(from, key, std::move(args));
    } catch (std::runtime_error&) {
        DebugSpewAlways("MQ2DanNet::Observe -- Failed to deserialize.");
    }

    return false;
}

std::stringstream MQ2DanNet::Observe::pack(const std::string& recipient, const std::string& query, const std::string& output) {
    std::stringstream send_stream;
    Archive<std::stringstream> send(send_stream);

    std::string final_query = Node::get().trim_query(query);

    if (recipient == Node::get().name()) {
        std::string new_group = Node::get().register_observer(recipient, final_query);
        Node::get().observe(new_group, recipient, final_query);
        Node::get().update(new_group, "NULL", output);

        std::stringstream self_send_stream;
        Archive<std::stringstream> self_send(self_send_stream);

        self_send << Node::get().name() << new_group << Node::get().parse_query(final_query);
        Update::callback(std::move(self_send_stream));

        // this isn't going to get sent anywhere.
        return std::stringstream();
    }

    // this is the callback to actually start observing. We can't just do it because the observed will come back with the right group
    auto f = [final_query, output](std::stringstream&& args) -> bool {
        Archive<std::stringstream> ar(args);
        std::string from;
        std::string group;
        std::string new_group;
        std::string data;

        try {
            ar >> from >> group >> new_group >> data;
            if (!new_group.empty()) {
                Node::get().observe(new_group, from, final_query);
                Node::get().update(new_group, "NULL", output);

                std::stringstream self_send_stream;
                Archive<std::stringstream> self_send(self_send_stream);

                self_send << Node::get().name() << new_group << data;
                Update::callback(std::move(self_send_stream));
            }
        } catch (std::runtime_error&) {
            DebugSpewAlways("MQ2DanNet::Observe -- response -- Failed to deserialize.");
        }

        return true;
    };

    // this registers the response from the observed that responds with a group name
    std::string key = Node::get().register_response(f);
    send << key << final_query;
    return send_stream;
}

const bool MQ2DanNet::Update::callback(std::stringstream&& args) {
    Archive<std::stringstream> received(args);
    std::string from;
    std::string group;
    std::string data;

    try {
        received >> from >> group >> data;
        Node::get().remove_commands([from, group, &data](std::pair<std::string, std::stringstream>& command) -> bool {
            if (command.first == Node::name<Update>()) {
                std::stringstream args_copy(command.second.str());
                Archive<std::stringstream> recv(args_copy);
                std::string copy_from, copy_group, copy_data;

                recv >> copy_from >> copy_group >> copy_data;
                if (from == copy_from && group == copy_group) {
                    //DebugSpewAlways("DROPPING EXTRA UPDATE --> FROM: %s, GROUP: %s", from.c_str(), group.c_str());
                    data = copy_data;
                    return true;
                }
            }

            return false;
        });

        //DebugSpewAlways("UPDATE --> FROM: %s, GROUP: %s, DATA: %s", from.c_str(), group.c_str(), data.c_str());

        std::string output = Node::get().read(group).output;
        CHAR szOutput[MAX_STRING] = { 0 };
        strcpy_s(szOutput, output.c_str());

        if (output.empty() || FindMQ2DataVariable(szOutput)) {
            MQTypeVar Result = Node::get().parse_response(output, data);

            CHAR szBuf[MAX_STRING] = { 0 };
            if (Result.Type)
                Result.Type->ToString(Result.VarPtr, szBuf);
            else
                strcpy_s(szBuf, "NULL");

            Node::get().update(group, std::string(szBuf), output);

            if (Node::get().debugging()) {
                if (Result.Type) {
                    CHAR szData[MAX_STRING] = { 0 };
                    Result.Type->ToString(Result.VarPtr, szData);
                    WriteChatf("%s : %s -- %llu (%llu)", Result.Type->GetName(), szData, Node::get().read(group).received, MQGetTickCount64());
                } else
                    WriteChatf("Failed to read data %s into %s at %llu.", data.c_str(), output.c_str(), MQGetTickCount64());
            }
        } else {
            // if we are storing to a variable, we need to drop the observer if the variable goes out of scope
            Node::get().forget(group);
            if (Node::get().debugging())
                WriteChatf("Could not find var %s at %llu.", output.c_str(), MQGetTickCount64());
        }
    } catch (std::runtime_error&) {
        DebugSpewAlways("MQ2DanNet::Update -- failed to deserialize.");
    }

    return false;
}

std::stringstream MQ2DanNet::Update::pack(const std::string& recipient, const std::string& result) {
    std::stringstream send_stream;

    Archive<std::stringstream> send(send_stream);
    send << result;

    // Update is never whispered, so we can assume that recipient is the group to update
    auto groups = Node::get().get_own_groups();
    if (groups.find(recipient) != groups.end()) {
        // also need to send this to self if we are observing self
        std::stringstream self_send_stream;
        Archive<std::stringstream> self_send(self_send_stream);

        self_send << Node::get().name() << recipient << result;
        callback(std::move(self_send_stream));
    }

    return send_stream;
}

const bool MQ2DanNet::Reupdate::callback(std::stringstream&& args) {
    Archive<std::stringstream> received(args);
    std::string from;
    std::string group;

    try {
        received >> from >> group;
        from = Node::get().get_name(from);
        DebugSpewAlways("REUPDATE --> FROM: %s, GROUP: %s", from.c_str(), group.c_str());

		Node::get().clear_observer_cache();

        return false;
    } catch (std::runtime_error&) {
        DebugSpewAlways("MQ2DanNet::Reupdate -- Failed to deserialize.");
        return false;
    }
}

std::stringstream MQ2DanNet::Reupdate::pack(const std::string& recipient) {
    return std::stringstream();
}

#pragma endregion

#pragma region MainPlugin

std::string GetDefault(std::string_view val) {
    if (val == "Debugging")
        return std::string("off");
    else if (val == "Local Echo")
        return std::string("on");
    else if (val == "Command Echo")
        return std::string("on");
    else if (val == "Tank")
        return std::string("war|pal|shd|");
    else if (val == "Priest")
        return std::string("clr|dru|shm|");
    else if (val == "Melee")
        return std::string("brd|rng|mnk|rog|bst|ber|");
    else if (val == "Caster")
        return std::string("nec|wiz|mag|enc|");
    else if (val == "Query Timeout")
        return std::string("1s");
    else if (val == "Full Names")
        return std::string("on");
    else if (val == "Front Delimiter")
        return std::string("off");
    else if (val == "Show Groups")
        return std::string("on");
    else if (val == "Observe Delay")
        return std::string("1000");
    else if (val == "Evasive")
        return std::string("5000");
    else if (val == "Expired")
        return std::string("30000");
    else if (val == "Keepalive")
        return std::string("30000");
    else if (val == "Evasive Refresh")
        return std::string("off");

    return std::string();
}

std::string ReadVar(const std::string& section, const std::string& key) {
    CHAR szBuf[MAX_STRING] = { 0 };
    GetPrivateProfileString(section.c_str(), key.c_str(), GetDefault(key).c_str(), szBuf, MAX_STRING, INIFileName);

    return std::string(szBuf);
}

std::string ReadVar(const std::string& key) {
    return ReadVar("General", key);
}

VOID SetVar(const std::string& section, const std::string& key, const std::string& val) {
    ::WritePrivateProfileStringA(section.c_str(), key.c_str(), val == GetDefault(val) ? NULL : val.c_str(), INIFileName);
}

bool ParseBool(const std::string& section, const std::string& key, const std::string& input, bool current) {
    std::string final_input = Node::init_string(input.c_str());
    if (final_input == "on" || final_input == "off")
        WriteChatf("\ax\atMQ2DanNet:\ax Turning \ao%s\ax to \ar%s\ax.", key.c_str(), input.c_str());
    else
        WriteChatf("\ax\atMQ2DanNet:\ax Turning \ao%s\ax to \ar%s\ax.", key.c_str(), current ? "off" : "on");

    if (final_input == "on") {
        SetVar(section, key, "on");
        return true;
    } else if (final_input == "off") {
        SetVar(section, key, "off");
        return false;
    } else if (final_input == "true") {
        SetVar(section, key, "true");
        return true;
    } else if (final_input == "false") {
        SetVar(section, key, "false");
        return false;
    } else {
        return !current;
    }
}

bool ReadBool(const std::string& section, const std::string& key) {
    return Node::init_string(ReadVar(section, key).c_str()) == "on" || Node::init_string(ReadVar(section, key).c_str()) == "true";
}

bool ReadBool(const std::string& key) {
    return ReadBool("General", key);
}

std::string CreateArray(const std::set<std::string>& members) {
    if (!members.empty()) {
        std::string delimiter = "|";
        auto accum = std::accumulate(members.cbegin(), members.cend(), std::string(),
            [delimiter](const std::string& s, const std::string& p) {
                return s + (s.empty() ? std::string() : delimiter) + p;
            });

        if (Node::get().front_delimiter())
            return delimiter + accum;
        else
            return accum + delimiter;
    }

    return std::string();
}

std::set<std::string> ParseArray(const std::string& arr) {
    std::set<std::string> tokens;
    std::string token;
    std::istringstream token_stream(arr);
    while (std::getline(token_stream, token, '|'))
        tokens.emplace(token);

    tokens.erase(""); // this is an artifact of creating the array to make it easy for macroers

    return tokens;
}

// leave all this here in case eqmule ever finds the cause for this to crash on live
class MQ2DanObservationType* pDanObservationType = nullptr;
class MQ2DanObservationType : public MQ2Type {
private:
public:
    enum Members {
        Received = 1
    };

    MQ2DanObservationType() : MQ2Type("DanObservation") {
        TypeMember(Received);
    }

    virtual bool GetMember(MQVarPtr VarPtr, const char* Member, char* Index, MQTypeVar& Dest) override {
        MQTypeMember* pMember = MQ2DanObservationType::FindMember(Member);
        if (!pMember)
            return false;

        Node::Observation* pObservation = ((Node::Observation*)VarPtr.Ptr);
        if (!pObservation)
            return false;

        switch ((Members)pMember->ID) {
        case Received:
            Dest.UInt64 = pObservation->received;
            Dest.Type = mq::datatypes::pInt64Type;
            return true;
        }

        return false;
    }

    bool ToString(MQVarPtr VarPtr, char* Destination) {
        Node::Observation* pObservation = ((Node::Observation*)VarPtr.Ptr);
        if (!pObservation)
            return false;

        strcpy_s(Destination, MAX_STRING, pObservation->data.c_str());
        return true;
    }

    void InitVariable(MQVarPtr& VarPtr) {
        VarPtr.Ptr = malloc(sizeof(Node::Observation));
        VarPtr.HighPart = 0;
        ZeroMemory(VarPtr.Ptr, sizeof(Node::Observation));
    }

    void FreeVariable(MQVarPtr& VarPtr) {
        free(VarPtr.Ptr);
    }

    bool FromData(MQVarPtr& VarPtr, const MQTypeVar& Source) override {
        if (Source.Type == pDanObservationType) {
            memcpy(VarPtr.Ptr, Source.Ptr, sizeof(Node::Observation));
            return true;
        }

        return false;
    }
};

class MQ2DanNetType* pDanNetType = nullptr;
class MQ2DanNetType : public MQ2Type {
private:
    std::string _peer;
    std::set<std::string> _peers;
    std::set<std::string> _groups;
    std::set<std::string> _joined;
    CHAR _buf[MAX_STRING];

    Node::Observation _current_observation;

public:
    enum Members {
        Name = 1,
        Version,
        Debug,
        LocalEcho,
        CommandEcho,
        FullNames,
        FrontDelim,
        ShowGroups,
        Timeout,
        ObserveDelay,
        Evasive,
		EvasiveRefresh,
        Expired,
        Keepalive,
        PeerCount,
        Peers,
        GroupCount,
        Groups,
        JoinedCount,
        Joined,
        O,
        Observe,
        OReceived,
        ObserveReceived,
        OCount,
        ObserveCount,
        OSet,
        ObserveSet,
        Q,
        Query,
        QReceived,
        QueryReceived
    };

    MQ2DanNetType() : MQ2Type("DanNet") {
        TypeMember(Name);
        TypeMember(Version);
        TypeMember(Debug);
        TypeMember(LocalEcho);
        TypeMember(CommandEcho);
        TypeMember(FullNames);
        TypeMember(FrontDelim);
        TypeMember(ShowGroups);
        TypeMember(Timeout);
        TypeMember(ObserveDelay);
        TypeMember(Evasive);
        TypeMember(EvasiveRefresh);
        TypeMember(Expired);
        TypeMember(Keepalive);
        TypeMember(PeerCount);
        TypeMember(Peers);
        TypeMember(GroupCount);
        TypeMember(Groups);
        TypeMember(JoinedCount);
        TypeMember(Joined);
        TypeMember(O);
        TypeMember(Observe);
        TypeMember(OCount);
        TypeMember(ObserveCount);
        TypeMember(OSet);
        TypeMember(ObserveSet);
        TypeMember(OReceived);
        TypeMember(ObserveReceived);
        TypeMember(Q);
        TypeMember(Query);
        TypeMember(QReceived);
        TypeMember(QueryReceived);
    }

    virtual bool GetMember(MQVarPtr VarPtr, const char* Member, char* Index, MQTypeVar& Dest) override {
        _buf[0] = '\0';

        std::string local_peer = _peer;
        _peer.clear();

        MQTypeMember* pMember = MQ2DanNetType::FindMember(Member);
        if (!pMember)
            return false;

        switch ((Members)pMember->ID) {
        case Name:
            strcpy_s(_buf, Node::get().get_name(Node::get().name()).c_str());
            Dest.Ptr = &_buf[0];
            Dest.Type = mq::datatypes::pStringType;
            return true;
        case Version:
            sprintf_s(_buf, "%1.4f", MQ2Version);
            Dest.Ptr = &_buf[0];
            Dest.Type = mq::datatypes::pStringType;
            return true;
        case Debug:
            Dest.DWord = Node::get().debugging();
            Dest.Type = mq::datatypes::pBoolType;
            return true;
        case LocalEcho:
            Dest.DWord = Node::get().local_echo();
            Dest.Type = mq::datatypes::pBoolType;
            return true;
        case CommandEcho:
            Dest.DWord = Node::get().command_echo();
            Dest.Type = mq::datatypes::pBoolType;
            return true;
        case FullNames:
            Dest.DWord = Node::get().full_names();
            Dest.Type = mq::datatypes::pBoolType;
            return true;
        case FrontDelim:
            Dest.DWord = Node::get().front_delimiter();
            Dest.Type = mq::datatypes::pBoolType;
            return true;
        case ShowGroups:
            Dest.DWord = Node::get().show_groups();
            Dest.Type = mq::datatypes::pBoolType;
            return true;
        case Timeout:
            strcpy_s(_buf, ReadVar("General", "Query Timeout").c_str());
            Dest.Ptr = &_buf[0];
            Dest.Type = mq::datatypes::pStringType;
            return true;
        case ObserveDelay:
            Dest.DWord = Node::get().observe_delay();
            Dest.Type = mq::datatypes::pIntType;
            return true;
        case Evasive:
            Dest.DWord = Node::get().evasive();
            Dest.Type = mq::datatypes::pIntType;
            return true;
        case EvasiveRefresh:
            Dest.DWord = Node::get().evasive_refresh();
            Dest.Type = mq::datatypes::pBoolType;
            return true;
        case Expired:
            Dest.DWord = Node::get().expired();
            Dest.Type = mq::datatypes::pIntType;
            return true;
        case Keepalive:
            Dest.DWord = Node::get().keepalive();
            Dest.Type = mq::datatypes::pIntType;
            return true;
        case PeerCount:
            if (IsNumber(Index)) {
                if (_groups.empty())
                    _groups = Node::get().get_all_groups();
                int idx = GetIntFromString(Index, 0) - 1;
                auto group_it = _groups.cbegin();
                std::advance(group_it, idx);
                if (group_it != _groups.cend())
                    Dest.DWord = static_cast<uint32_t>(Node::get().get_group_peers(*group_it).size());
                else
                    return false;
            } else if (Index && Index[0] != '\0') {
                Dest.DWord = static_cast<uint32_t>(Node::get().get_group_peers(Node::init_string(Index)).size());
            } else {
                Dest.DWord = static_cast<uint32_t>(Node::get().get_peers().size());
            }
            Dest.Type = mq::datatypes::pIntType;
            return true;
        case Peers:
            if (IsNumber(Index)) {
                if (_peers.empty())
                    _peers = Node::get().get_peers();
                int idx = GetIntFromString(Index, 0) - 1;
                auto peer_it = _peers.cbegin();
                std::advance(peer_it, idx);
                if (peer_it != _peers.cend()) {
                    std::string out = Node::get().get_name(*peer_it);
                    strcpy_s(_buf, out.c_str());
                } else
                    return false;
            } else if (Index && Index[0] != '\0') {
                auto peers = Node::get().get_group_peers(Node::init_string(Index));
                std::set<std::string> out;
                if (Node::get().full_names())
                    out = peers;
                else {
                    std::transform(peers.cbegin(), peers.cend(), std::inserter(out, out.begin()), [](std::string s) -> std::string {
                        return Node::get().get_short_name(s);
                    });
                }
                strcpy_s(_buf, CreateArray(out).c_str());
            } else {
                auto peers = Node::get().get_peers();
                std::set<std::string> out;
                if (Node::get().full_names())
                    out = peers;
                else {
                    std::transform(peers.cbegin(), peers.cend(), std::inserter(out, out.begin()), [](std::string s) -> std::string {
                        return Node::get().get_short_name(s);
                    });
                }
                strcpy_s(_buf, CreateArray(out).c_str());
            }

            Dest.Ptr = &_buf[0];
            Dest.Type = mq::datatypes::pStringType;
            return true;
        case GroupCount:
            _groups = Node::get().get_all_groups();
            Dest.DWord = static_cast<uint32_t>(_groups.size());
            Dest.Type = mq::datatypes::pIntType;
            return true;
        case Groups:
            if (IsNumber(Index)) {
                if (_groups.empty())
                    _groups = Node::get().get_all_groups();
                int idx = GetIntFromString(Index, 0) - 1;
                auto group_it = _groups.cbegin();
                std::advance(group_it, idx);
                if (group_it != _groups.cend())
                    strcpy_s(_buf, group_it->c_str());
                else
                    return false;
            } else {
                strcpy_s(_buf, CreateArray(Node::get().get_all_groups()).c_str());
            }
            Dest.Ptr = &_buf[0];
            Dest.Type = mq::datatypes::pStringType;
            return true;
        case JoinedCount:
            _joined = Node::get().get_own_groups();
            Dest.DWord = static_cast<uint32_t>(_groups.size());
            Dest.Type = mq::datatypes::pIntType;
            return true;
        case Joined:
            if (IsNumber(Index)) {
                if (_joined.empty())
                    _joined = Node::get().get_own_groups();
                int idx = GetIntFromString(Index, 0) - 1;
                auto group_it = _joined.cbegin();
                std::advance(group_it, idx);
                if (group_it != _joined.cend())
                    strcpy_s(_buf, group_it->c_str());
                else
                    return false;
            } else {
                strcpy_s(_buf, CreateArray(Node::get().get_own_groups()).c_str());
            }
            Dest.Ptr = &_buf[0];
            Dest.Type = mq::datatypes::pStringType;
            return true;
        case Q:
        case Query:
            if (!local_peer.empty() && Index && Index[0] != '\0') {
                // only allow indexed query access if both peer and query are specified
                _current_observation = Node::Observation(Node::get().query(local_peer, Index));
            } else {
                // ignore all indexing and silently just provide the last result
                _current_observation = Node::Observation(Node::get().query());
            }

            if (_current_observation.received != 0) {
                Dest.Ptr = &_current_observation;
                Dest.Type = pDanObservationType;
                return true;
            } else
                return false;
        case QReceived:
        case QueryReceived:
            if (!local_peer.empty() && Index && Index[0] != '\0') {
                // only allow indexed query access if both peer and query are specified
                _current_observation = Node::Observation(Node::get().query(local_peer, Index));
            } else {
                // ignore all indexing and silently just provide the last result
                _current_observation = Node::Observation(Node::get().query());
            }

            Dest.UInt64 = _current_observation.received;
            Dest.Type = mq::datatypes::pInt64Type;
            return true;
        case O:
        case Observe:
            if (!local_peer.empty()) {
                if (Index && Index[0] != '\0') {
                    _current_observation = Node::get().read(local_peer, Node::get().trim_query(Index));

                    if (_current_observation.received != 0) {
                        Dest.Ptr = &_current_observation;
                        Dest.Type = pDanObservationType;
                        return true;
                    } else
                        return false;
                } else {
                    auto observers = Node::get().observed_queries(local_peer);
                    strcpy_s(_buf, CreateArray(observers).c_str());

                    Dest.Ptr = &_buf[0];
                    Dest.Type = mq::datatypes::pStringType;
                    return true;
                }
            } else {
                if (Index && Index[0] != '\0') {
                    auto observers = Node::get().observers(Node::get().trim_query(Index));
                    std::set<std::string> out;
                    if (Node::get().full_names())
                        out = observers;
                    else {
                        std::transform(observers.cbegin(), observers.cend(), std::inserter(out, out.begin()), [](std::string s) -> std::string {
                            return Node::get().get_short_name(s);
                        });
					}
                    strcpy_s(_buf, CreateArray(out).c_str());
                } else {
                    auto observers = Node::get().observer_queries();
                    strcpy_s(_buf, CreateArray(observers).c_str());
                }

                Dest.Ptr = &_buf[0];
                Dest.Type = mq::datatypes::pStringType;
                return true;
            }
        case OCount:
        case ObserveCount:
            if (!local_peer.empty()) {
                Dest.DWord = static_cast<uint32_t>(Node::get().observed_count(local_peer));
            } else {
                Dest.DWord = static_cast<uint32_t>(Node::get().observer_count());
            }
            Dest.Type = mq::datatypes::pIntType;
            return true;
        case OSet:
        case ObserveSet:
            if (Index && Index[0] != '\0') {
                if (!local_peer.empty()) {
                    auto observed = Node::get().observed_queries(local_peer);
                    if (observed.find(Node::get().trim_query(Index)) != observed.end()) {
                        Dest.DWord = 1;
                    } else {
                        Dest.DWord = 0;
                    }
                } else {
                    auto observers = Node::get().observer_queries();
                    if (observers.find(Node::get().trim_query(Index)) != observers.end()) {
                        Dest.DWord = 1;
                    } else {
                        Dest.DWord = 0;
                    }
                }
                Dest.Type = mq::datatypes::pBoolType;
                return true;
            } else
                return false;
        case OReceived:
        case ObserveReceived:
            if (!local_peer.empty() && Index && Index[0] != '\0') {
                _current_observation = Node::get().read(local_peer, Node::get().trim_query(Index));
                Dest.UInt64 = _current_observation.received;
                Dest.Type = mq::datatypes::pInt64Type;
                return true;
            } else
                return false;
        }

        return false;
    }

    void SetPeer(const std::string& peer) {
        if (Node::get().debugging())
            WriteChatf("MQ2DanNetType::SetPeer setting peer from %s to %s", _peer.c_str(), peer.c_str());

        _peer = peer;
    }

    bool ToString(MQVarPtr VarPtr, char* Destination) {
        if (_peer.empty())
            return false;

        strcpy_s(Destination, MAX_STRING, _peer.c_str());
        _peer.clear();
        return true;
    }
};

bool dataDanNet(const char* Index, MQTypeVar& Dest) {
    Dest.DWord = 1;
    Dest.Type = pDanNetType;

    if (Node::get().debugging())
        WriteChatf("MQ2DanNetType::dataDanNet Index %s", Index);

    if (Index == nullptr || Index[0] == '\0')
        pDanNetType->SetPeer("");
    else if (!Node::get().has_peer(Index))
        return false;
    else
        pDanNetType->SetPeer(Node::get().get_full_name(Index));

    return true;
}

bool DoesVarExist(const Node::Observation& observation) {
    std::string output = observation.output;
    if (output.empty())
        return false;

    CHAR szOutput[MAX_STRING] = { 0 };
    strcpy_s(szOutput, output.c_str());
    return FindMQ2DataVariable(szOutput) == 0;
}

std::string unescape_string(const std::string& input) {
    char szOut[MAX_STRING] = { 0 };
    for (size_t old_pos = 0, new_pos = 0; old_pos < input.length(); ++old_pos, szOut[++new_pos] = 0) {
        if (input.at(old_pos) == '\\') {
            ++old_pos;
            if (input.at(old_pos)) {
                if (input.at(old_pos) == '\\')
                    szOut[new_pos] = input.at(old_pos);
                else if (input.at(old_pos) == 'n') {
                    szOut[new_pos++] = '\r';
                    szOut[new_pos] = '\n';
                } else if (input.at(old_pos) >= 'a' && input.at(old_pos) <= 'z')
                    szOut[new_pos] = input.at(old_pos) - 'a' + 7;
                else if (input.at(old_pos) >= 'A' && input.at(old_pos) <= 'Z')
                    szOut[new_pos] = input.at(old_pos) - 'a' + 7;
            }
        } else {
            szOut[new_pos] = input.at(old_pos);
        }
    }

    return std::string(szOut);
}

// The name must be send as server_name
extern "C" MQ2DANNET_NODE_API bool peer_connected(const std::string& name) {
    std::string lower_name = Node::init_string(name.c_str());
    auto peers = Node::get().get_peers();
    return std::find(peers.begin(), peers.end(), lower_name) != peers.end();
}

void show_dnet_commands() {
    WriteChatf("           \ayinterface [<iface_name>]\ax -- force interface to iface_name");
    WriteChatf("           \aydebug [on|off]\ax -- turn debug on or off");
    WriteChatf("           \aylocalecho [on|off]\ax -- turn localecho on or off");
    WriteChatf("           \aycommandecho [on|off]\ax -- turn commandecho on or off");
    WriteChatf("           \ayfullnames [on|off]\ax -- turn fullnames on or off");
    WriteChatf("           \ayfrontdelim [on|off]\ax -- turn front delimiters on or off");
    WriteChatf("           \ayshowgroups [on|off]\ax -- show groups in /dgtell receive messages");
    WriteChatf("           \aytimeout [new_timeout]\ax -- set the /dquery timeout");
    WriteChatf("           \ayobservedelay [new_delay]\ax -- set the delay between observe sends in ms");
    WriteChatf("           \ayevasive [new_evasive]\ax -- set the evasive timeout in ms");
    WriteChatf("           \ayevasiverefresh [on|off]\ax -- turn evasive refresh on or off");
    WriteChatf("           \ayexpired [new_expired]\ax -- set the expired timeout in ms");
    WriteChatf("           \aykeepalive [new_keepalive]\ax -- set the keepalive time for non-responding peers in ms");
    WriteChatf("           \ayinfo\ax -- output group/peer information");
}

PLUGIN_API VOID DNetCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szParam[MAX_STRING] = { 0 };
    GetArg(szParam, szLine, 1);

    if (szParam[0]) {
        if (ci_equals(szParam, "interface")) {
            GetArg(szParam, szLine, 2);
            if (szParam[0] && strlen(szParam) > 0) {
                if (!strcmp(szParam, "clear")) {
                    SetVar("General", "Interface", std::string());
                    WriteChatf("\ax\atMQ2DanNet:\ax Cleared interface setting.");
                } else {
                    SetVar("General", "Interface", szParam);
                    WriteChatf("\ax\atMQ2DanNet:\ax Set interface to \ay%s\ax", szParam);
                }
            } else {
                WriteChatf("\ax\atMQ2DanNet:\ax Interfaces --\r\n\ay%s\ax", Node::get().get_interfaces().c_str());
            }
        } else if (ci_equals(szParam, "debug")) {
            GetArg(szParam, szLine, 2);
            Node::get().debugging(ParseBool("General", "Debugging", szParam, Node::get().debugging()));
        } else if (ci_equals(szParam, "localecho")) {
            GetArg(szParam, szLine, 2);
            Node::get().local_echo(ParseBool("General", "Local Echo", szParam, Node::get().local_echo()));
        } else if (ci_equals(szParam, "commandecho")) {
            GetArg(szParam, szLine, 2);
            Node::get().command_echo(ParseBool("General", "Command Echo", szParam, Node::get().command_echo()));
        } else if (ci_equals(szParam, "fullnames")) {
            GetArg(szParam, szLine, 2);
            Node::get().full_names(ParseBool("General", "Full Names", szParam, Node::get().full_names()));
        } else if (ci_equals(szParam, "frontdelim")) {
            GetArg(szParam, szLine, 2);
            Node::get().front_delimiter(ParseBool("General", "Front Delimiter", szParam, Node::get().front_delimiter()));
        } else if (ci_equals(szParam, "timeout")) {
            GetArg(szParam, szLine, 2);
            if (szParam[0])
                SetVar("General", "Query Timeout", szParam);
            else
                SetVar("General", "Query Timeout", GetDefault("Query Timeout"));
        } else if (ci_equals(szParam, "showgroups")) {
            GetArg(szParam, szLine, 2);
            Node::get().show_groups(ParseBool("General", "Show Groups", szParam, Node::get().show_groups()));
        } else if (ci_equals(szParam, "observedelay")) {
            GetArg(szParam, szLine, 2);
            if (szParam[0] && IsNumber(szParam))
                SetVar("General", "Observe Delay", szParam);
            else
                SetVar("General", "Observe Delay", GetDefault("Observe Delay"));
            Node::get().observe_delay(GetIntFromString(ReadVar("Observe Delay").c_str(), 0));
        } else if (ci_equals(szParam, "evasive")) {
            GetArg(szParam, szLine, 2);
            if (szParam[0] && IsNumber(szParam))
                SetVar("General", "Evasive", szParam);
            else
                SetVar("General", "Evasive", GetDefault("Evasive"));
            Node::get().evasive(GetIntFromString(ReadVar("Evasive").c_str(), 0));
        } else if (ci_equals(szParam, "evasiverefresh")) {
            GetArg(szParam, szLine, 2);
            Node::get().evasive_refresh(ParseBool("General", "Evasive Refresh", szParam, Node::get().evasive_refresh()));
        } else if (ci_equals(szParam, "expired")) {
            GetArg(szParam, szLine, 2);
            if (szParam[0] && IsNumber(szParam))
                SetVar("General", "Expired", szParam);
            else
                SetVar("General", "Expired", GetDefault("Expired"));
            Node::get().expired(GetIntFromString(ReadVar("Expired").c_str(), 0));
        } else if (ci_equals(szParam, "keepalive")) {
            GetArg(szParam, szLine, 2);
            if (szParam[0] && IsNumber(szParam))
                SetVar("General", "Keepalive", szParam);
            else
                SetVar("General", "Keepalive", GetDefault("Keepalive"));
            Node::get().keepalive(GetIntFromString(ReadVar("Keepalive").c_str(), 0));
        } else if (ci_equals(szParam, "info")) {
            WriteChatf("\ax\atMQ2DanNet\ax :: \ayv%1.4f\ax", MQ2Version);
            for (std::string info : Node::get().get_info()) {
                WriteChatf("%s", info.c_str());
            }
        } else if (ci_equals(szParam, "version")) {
            WriteChatf("\ax\atMQ2DanNet\ax :: \ayv%1.4f\ax", MQ2Version);
        } else {
            WriteChatf("\ax\atMQ2DanNet:\ax unrecognized /dnet argument \ar%s\ax. Valid arguments are: ", szParam);
            show_dnet_commands();
        }
    } else {
        WriteChatf("\ax\atMQ2DanNet:\ax no /dnet argument specified. Valid arguments are: ");
        show_dnet_commands();
    }
}

PLUGIN_API VOID DJoinCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szGroup[MAX_STRING] = { 0 };
    GetArg(szGroup, szLine, 1);

    std::string group = Node::init_string(szGroup);

    if (group.empty())
        WriteChatColor("Syntax: /djoin <group> [all|save] -- join named group on peer network", USERCOLOR_DEFAULT);
    else {
        Node::get().join(group);

        GetArg(szGroup, szLine, 2);
        if (szGroup[0] && !strcmp(szGroup, "save")) {
            std::set<std::string> saved_groups = ParseArray(ReadVar(Node::get().name().c_str(), "Groups"));
            saved_groups.emplace(group);
            SetVar(Node::get().name().c_str(), "Groups", CreateArray(saved_groups));
        } else if (szGroup[0] && !strcmp(szGroup, "all")) {
            std::set<std::string> saved_groups = ParseArray(ReadVar("General", "Groups"));
            saved_groups.emplace(group);
            SetVar("General", "Groups", CreateArray(saved_groups));
        } else if (szGroup[0]) {
            WriteChatColor("Syntax: /djoin <group> [all|save] -- join named group on peer network", USERCOLOR_DEFAULT);
        }
    }
}

PLUGIN_API VOID DLeaveCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szGroup[MAX_STRING] = { 0 };
    GetArg(szGroup, szLine, 1);

    std::string group = Node::init_string(szGroup);

    if (group.empty())
        WriteChatColor("Syntax: /dleave <group> [all|save] -- leave named group on peer network", USERCOLOR_DEFAULT);
    else {
        Node::get().leave(group);

        GetArg(szGroup, szLine, 2);
        if (szGroup[0] && !strcmp(szGroup, "save")) {
            std::set<std::string> saved_groups = ParseArray(ReadVar(Node::get().name().c_str(), "Groups"));
            saved_groups.erase(group);
            SetVar(Node::get().name().c_str(), "Groups", CreateArray(saved_groups));
        } else if (szGroup[0] && !strcmp(szGroup, "all")) {
            std::set<std::string> saved_groups = ParseArray(ReadVar("General", "Groups"));
            saved_groups.erase(group);
            SetVar("General", "Groups", CreateArray(saved_groups));
        } else if (szGroup[0]) {
            WriteChatColor("Syntax: /djoin <group> [all|save] -- join named group on peer network", USERCOLOR_DEFAULT);
        }
    }
}

PLUGIN_API VOID DTellCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szName[MAX_STRING] = { 0 };
    GetArg(szName, szLine, 1);
    auto name = Node::init_string(szName);
    std::string message(szLine);
    std::string::size_type n = message.find_first_not_of(" \t", 0);
    n = message.find_first_of(" \t", n);
    message.erase(0, message.find_first_not_of(" \t", n));

    if (name.empty() || message.empty())
        WriteChatColor("Syntax: /dtell <name> <message> -- send message to name", USERCOLOR_DEFAULT);
    else {
        name = Node::get().get_name(name);

        std::string unescaped_message = unescape_string(message);
        WriteChatf("\ax\a-t[ \ax\at-->\ax\a-t(%s) ]\ax \aw%s\ax", name.c_str(), unescaped_message.c_str());
        Node::get().whisper<MQ2DanNet::Echo>(name, unescaped_message);
    }
}

PLUGIN_API VOID DGtellCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szGroup[MAX_STRING] = { 0 };
    GetArg(szGroup, szLine, 1);
    auto group = Node::init_string(szGroup);
    std::string message(szLine);

    std::set<std::string> groups = Node::get().get_all_groups();
    if (groups.find(group) != groups.end()) {
        std::string::size_type n = message.find_first_not_of(" \t", 0);
        n = message.find_first_of(" \t", n);
        message.erase(0, message.find_first_not_of(" \t", n));
    } else {
        // let's assume that if we can't find the group, we just meant all
        group = "all";
    }

    if (group.empty() || message.empty())
        WriteChatColor("Syntax: /dgtell <group> <message> -- broadcast message to group", USERCOLOR_DEFAULT);
    else {
        std::string unescaped_message = unescape_string(message);
        WriteChatf("\ax\a-t[\ax\at -->\ax\a-t(%s) ]\ax \aw%s\ax", group.c_str(), unescaped_message.c_str());
        Node::get().shout<MQ2DanNet::Echo>(group, unescaped_message);
    }
}

PLUGIN_API VOID DExecuteCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szName[MAX_STRING] = { 0 };
    GetArg(szName, szLine, 1);
    auto name = Node::init_string(szName);
    std::string command(szLine);
    std::string::size_type n = command.find_first_not_of(" \t", 0);
    n = command.find_first_of(" \t", n);
    command.erase(0, command.find_first_not_of(" \t", n));

    if (name.empty() || command.empty())
        WriteChatColor("Syntax: /dexecute <name> <command> -- direct name to execute command", USERCOLOR_DEFAULT);
    else {
        name = Node::get().get_name(name);

        if (Node::get().local_echo())
            WriteChatf("\ax\a-o[ \ax\ao-->\ax\a-o(%s) ]\ax \aw%s\ax", name.c_str(), command.c_str());
        Node::get().whisper<MQ2DanNet::Execute>(name, command);
    }
}

PLUGIN_API VOID DGexecuteCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szGroup[MAX_STRING] = { 0 };
    GetArg(szGroup, szLine, 1);
    auto group = Node::init_string(szGroup);
    std::string command(szLine);

    auto replace_qualifier = [&group, &command](const std::string& qualifier) {
        if (group == qualifier) {
            std::set<std::string> groups = Node::get().get_own_groups();
            auto group_it = std::find_if(groups.cbegin(), groups.cend(), [&qualifier](const std::string& group_name) {
                return group_name.find(qualifier + "_") == 0;
            });

            if (group_it != groups.cend()) {
                group = *group_it;
            } else {
                // we know that qualifier is the first argument, but we don't have any group that matches -- we still need to delete the argument before sending the command
                std::string::size_type n = command.find_first_not_of(" \t", 0);
                n = command.find_first_of(" \t", n);
                command.erase(0, command.find_first_not_of(" \t", n));
            }
        }
    };

    replace_qualifier("group");
    replace_qualifier("raid");
    replace_qualifier("zone");

    std::set<std::string> groups = Node::get().get_all_groups();
    if (group.find("/") == 0) {
        // we can assume that '/' signifies the start of a command, so we haven't specified a group
        group = "all";
    } else if (groups.find(group) != groups.end()) {
        std::string::size_type n = command.find_first_not_of(" \t", 0);
        n = command.find_first_of(" \t", n);
        command.erase(0, command.find_first_not_of(" \t", n));
    } else {
        SyntaxError("Could not find channel %s", group.c_str());
        return;
    }

    if (group.empty() || command.empty())
        WriteChatColor("Syntax: /dgexecute <group> <command> -- direct group to execute command", USERCOLOR_DEFAULT);
    else {
        if (Node::get().local_echo())
            WriteChatf("\ax\a-o[\ax\ao -->\ax\a-o(%s) ]\ax \aw%s\ax", group.c_str(), command.c_str());
        Node::get().shout<MQ2DanNet::Execute>(group, command);
    }
}

PLUGIN_API VOID DGGexecuteCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    char newLine[MAX_STRING] = { 0 };
    sprintf_s(newLine, "group %s", szLine);
    DGexecuteCommand(pSpawn, newLine);
}

PLUGIN_API VOID DGRexecuteCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    char newLine[MAX_STRING] = { 0 };
    sprintf_s(newLine, "raid %s", szLine);
    DGexecuteCommand(pSpawn, newLine);
}

PLUGIN_API VOID DGZexecuteCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    char newLine[MAX_STRING] = { 0 };
    sprintf_s(newLine, "zone %s", szLine);
    DGexecuteCommand(pSpawn, newLine);
}

PLUGIN_API VOID DGAexecuteCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szGroup[MAX_STRING] = { 0 };
    GetArg(szGroup, szLine, 1);
    auto group = Node::init_string(szGroup);
    std::string command(szLine);

    auto replace_qualifier = [&group, &command](const std::string& qualifier) {
        if (group == qualifier) {
            std::set<std::string> groups = Node::get().get_own_groups();
            auto group_it = std::find_if(groups.cbegin(), groups.cend(), [&qualifier](const std::string& group_name) {
                return group_name.find(qualifier + "_") == 0;
            });

            if (group_it != groups.cend()) {
                group = *group_it;
            } else {
                // we know that qualifier is the first argument, but we don't have any group that matches -- we still need to delete the argument before sending the command
                std::string::size_type n = command.find_first_not_of(" \t", 0);
                n = command.find_first_of(" \t", n);
                command.erase(0, command.find_first_not_of(" \t", n));
            }
        }
    };

    replace_qualifier("group");
    replace_qualifier("raid");
    replace_qualifier("zone");

    std::set<std::string> groups = Node::get().get_all_groups();
    if (group.find("/") == 0) {
        // we can assume that '/' signifies the start of a command, so we haven't specified a group
        group = "all";
    } else if (groups.find(group) != groups.end()) {
        std::string::size_type n = command.find_first_not_of(" \t", 0);
        n = command.find_first_of(" \t", n);
        command.erase(0, command.find_first_not_of(" \t", n));
    } else {
        SyntaxError("Could not find channel %s", group.c_str());
        return;
    }

    if (group.empty() || command.empty())
        WriteChatColor("Syntax: /dgaexecute <group> <command> -- direct group to execute command", USERCOLOR_DEFAULT);
    else {
        if (Node::get().local_echo())
            WriteChatf("\ax\a-o[\ax\ao -->\ax\a-o(%s) ]\ax \aw%s\ax", group.c_str(), command.c_str());
        Node::get().shout<MQ2DanNet::Execute>(group, command);

        std::string final_command = std::regex_replace(command, std::regex("\\$\\\\\\{"), "${");

        CHAR szCommand[MAX_STRING] = { 0 };
        strcpy_s(szCommand, final_command.c_str());
        EzCommand(szCommand);
    }
}

PLUGIN_API VOID DGGAexecuteCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    char newLine[MAX_STRING] = { 0 };
    sprintf_s(newLine, "group %s", szLine);
    DGAexecuteCommand(pSpawn, newLine);
}

PLUGIN_API VOID DGRAexecuteCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    char newLine[MAX_STRING] = { 0 };
    sprintf_s(newLine, "raid %s", szLine);
    DGAexecuteCommand(pSpawn, newLine);
}

PLUGIN_API VOID DGZAexecuteCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    char newLine[MAX_STRING] = { 0 };
    sprintf_s(newLine, "zone %s", szLine);
    DGAexecuteCommand(pSpawn, newLine);
}

PLUGIN_API VOID DObserveCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szName[MAX_STRING] = { 0 };
    CHAR szParam[MAX_STRING] = { 0 };
    GetArg(szName, szLine, 1);
    auto name = Node::init_string(szName);
    if (std::string::npos == name.find_last_of("_"))
        name = Node::get().get_full_name(name);

    std::string query;
    std::string output;
    bool drop = false;

    int current_param = 1;
    do {
        GetArg(szParam, szLine, ++current_param);
        if (!strncmp(szParam, "-q", 2)) {
            GetArg(szParam, szLine, ++current_param);
            if (szParam[0])
                query = szParam;
        } else if (!strncmp(szParam, "-o", 2)) {
            GetArg(szParam, szLine, ++current_param);
            if (szParam[0])
                output = szParam;
        } else if (!strncmp(szParam, "-d", 2)) {
            drop = true;
        } else if (szParam[0] == '-') {
            // don't understand the switch, let's just fast-forward
            ++current_param;
        }
    } while (szParam[0] || current_param > 10);

    if (drop && !name.empty()) {
        if (query.empty())
            Node::get().forget_all(name);
        else
            Node::get().forget(name, query);
    } else if (name.empty() || query.empty()) {
        WriteChatColor("Syntax: /dobserve <name> [-q <query>] [-o <result>] [-drop] -- add an observer on name and update values in result, or drop the observer", USERCOLOR_DEFAULT);
    } else {
        auto peers = Node::get().get_peers();
        if (peers.find(name) == peers.end()) {
            DebugSpewAlways("/dobserve: Can not find peer %s in %s!", name.c_str(), CreateArray(peers).c_str());
            return;
        }

        // also need to check here in case we set vars in a new macro before a pulse
        if (Node::get().last_macro_check() != gMacroBlock.get()) {
            Node::get().last_macro_check(gMacroBlock.get());
            Node::get().forget_if(DoesVarExist);
        }

        if (!Node::get().can_read(name, query))
            Node::get().whisper<Observe>(name, query, output);
    }
}

PLUGIN_API VOID DQueryCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szName[MAX_STRING] = { 0 };
    CHAR szParam[MAX_STRING] = { 0 };
    GetArg(szName, szLine, 1);
    auto name = Node::init_string(szName);
    if (std::string::npos == name.find_last_of("_"))
        name = Node::get().get_full_name(name);

    std::string query;
    std::string output;
    std::string timeout;

    int current_param = 1;
    do {
        GetArg(szParam, szLine, ++current_param);
        if (!strncmp(szParam, "-q", 2)) {
            GetArg(szParam, szLine, ++current_param);
            if (szParam[0])
                query = szParam;
        } else if (!strncmp(szParam, "-o", 2)) {
            GetArg(szParam, szLine, ++current_param);
            if (szParam[0])
                output = szParam;
        } else if (!strncmp(szParam, "-t", 2)) {
            GetArg(szParam, szLine, ++current_param);
            if (szParam[0])
                timeout = szParam;
        } else if (szParam[0] == '-') {
            // don't understand the switch, let's just fast-forward
            ++current_param;
        }
    } while (szParam[0] || current_param > 10);

    if (name.empty() || query.empty()) {
        WriteChatColor("Syntax: /dquery <name> [-q <query>] [-o <result>] [-t <timeout>] -- execute query on name and store return in result", USERCOLOR_DEFAULT);
    } else if (name == Node::get().name()) {
        // this is a self-query, let's just return the evaluation of the query
        MQTypeVar Result = Node::get().parse_response(output, Node::get().parse_query(query).c_str());
        CHAR szBuf[MAX_STRING] = { 0 };
        if (Result.Type)
            Result.Type->ToString(Result.VarPtr, szBuf);
        else
            strcpy_s(szBuf, "NULL");

        Node::get().query_result(name, query, Node::Observation(output, std::string(szBuf), MQGetTickCount64()));
    } else {
        // reset the result so we can tell when we get a response. Needs to be done before the delay call.
        Node::get().query_result(name, query, Node::Observation(output));

        auto peers = Node::get().get_peers();
        if (peers.find(name) == peers.end()) {
            DebugSpewAlways("/dquery: Can not find peer %s in %s!", name.c_str(), CreateArray(peers).c_str());
            return;
        }

        if (timeout.empty())
            timeout = ReadVar("General", "Query Timeout");

        PCHARINFO pChar = GetCharInfo();
        if (pChar) {
            CHAR szDelay[MAX_STRING] = { 0 };
            if (gParserVersion == 2)
                strcpy_s(szDelay, (timeout + " ${DanNet[" + name + "].QReceived[${Parse[0,\"" + query + "\"]}]}").c_str());
            else
                strcpy_s(szDelay, (timeout + " ${DanNet[" + name + "].QReceived[\"" + query + "\"]}").c_str());
            Delay(pChar->pSpawn, szDelay);

            Node::get().whisper<Query>(name, query);
        }
    }
}

// Called once, when the plugin is to initialize
PLUGIN_API VOID InitializePlugin() {
    DebugSpewAlways("Initializing MQ2DanNet");

    Node::get().startup();

    Node::get().register_command<MQ2DanNet::Echo>();
    Node::get().register_command<MQ2DanNet::Execute>();
    Node::get().register_command<MQ2DanNet::Query>();
    Node::get().register_command<MQ2DanNet::Observe>();
    Node::get().register_command<MQ2DanNet::Update>();
    Node::get().register_command<MQ2DanNet::Reupdate>();

    Node::get().debugging(ReadBool("General", "Debugging"));
    Node::get().local_echo(ReadBool("General", "Local Echo"));
    Node::get().command_echo(ReadBool("General", "Command Echo"));
    Node::get().full_names(ReadBool("General", "Full Names"));
    Node::get().front_delimiter(ReadBool("General", "Front Delimiter"));
    Node::get().show_groups(ReadBool("General", "Show Groups"));
    Node::get().evasive_refresh(ReadBool("General", "Evasive Refresh"));

    CHAR observe_delay[MAX_STRING] = { 0 };
    strcpy_s(observe_delay, ReadVar("Observe Delay").c_str());
    if (IsNumber(observe_delay)) {
        Node::get().observe_delay(GetIntFromString(observe_delay, 0));
    } else {
        Node::get().observe_delay(GetIntFromString(GetDefault("Observe Delay").c_str(), 0));
    }

    CHAR evasive[MAX_STRING] = { 0 };
    strcpy_s(evasive, ReadVar("Evasive").c_str());
    if (IsNumber(evasive)) {
        Node::get().evasive(GetIntFromString(evasive, 0));
    } else {
        Node::get().evasive(GetIntFromString(GetDefault("Evasive").c_str(), 0));
    }

    CHAR expired[MAX_STRING] = { 0 };
    strcpy_s(expired, ReadVar("Expired").c_str());
    if (IsNumber(expired)) {
        Node::get().expired(GetIntFromString(expired, 0));
    } else {
        Node::get().expired(GetIntFromString(GetDefault("Expired").c_str(), 0));
    }

    CHAR keepalive[MAX_STRING] = { 0 };
    strcpy_s(keepalive, ReadVar("Keepalive").c_str());
    if (IsNumber(keepalive)) {
        Node::get().keepalive(GetIntFromString(keepalive, 0));
    } else {
        Node::get().keepalive(GetIntFromString(GetDefault("Keepalive").c_str(), 0));
    }

    AddCommand("/dnet", DNetCommand);
    AddCommand("/djoin", DJoinCommand);
    AddCommand("/dleave", DLeaveCommand);
    AddCommand("/dtell", DTellCommand);
    AddCommand("/dgtell", DGtellCommand);
    AddCommand("/dexecute", DExecuteCommand);
    AddCommand("/dgexecute", DGexecuteCommand);
    AddCommand("/dggexecute", DGGexecuteCommand);
    AddCommand("/dgrexecute", DGRexecuteCommand);
    AddCommand("/dgzexecute", DGZexecuteCommand);
    AddCommand("/dgaexecute", DGAexecuteCommand);
    AddCommand("/dggaexecute", DGGAexecuteCommand);
    AddCommand("/dgraexecute", DGRAexecuteCommand);
    AddCommand("/dgzaexecute", DGZAexecuteCommand);
    AddCommand("/dobserve", DObserveCommand);
    AddCommand("/dquery", DQueryCommand);

    pDanNetType = new MQ2DanNetType;
    AddMQ2Data("DanNet", dataDanNet);

    pDanObservationType = new MQ2DanObservationType;

    WriteChatf("\ax\atMQ2DanNet\ax :: \ayv%1.4f\ax", MQ2Version);
}

// Called once, when the plugin is to shutdown
PLUGIN_API VOID ShutdownPlugin() {
    DebugSpewAlways("Shutting down MQ2DanNet");
    Node::get().exit();

    // this is Windows-specific and needs to be done to free some dangling select() threads
    Node::get().shutdown();

    Node::get().unregister_command<MQ2DanNet::Echo>();
    Node::get().unregister_command<MQ2DanNet::Execute>();
    Node::get().unregister_command<MQ2DanNet::Query>();
    Node::get().unregister_command<MQ2DanNet::Observe>();
    Node::get().unregister_command<MQ2DanNet::Update>();
    Node::get().unregister_command<MQ2DanNet::Reupdate>();

    RemoveCommand("/dnet");
    RemoveCommand("/djoin");
    RemoveCommand("/dleave");
    RemoveCommand("/dtell");
    RemoveCommand("/dgtell");
    RemoveCommand("/dexecute");
    RemoveCommand("/dgexecute");
    RemoveCommand("/dggexecute");
    RemoveCommand("/dgrexecute");
    RemoveCommand("/dgzexecute");
    RemoveCommand("/dgaexecute");
    RemoveCommand("/dggaexecute");
    RemoveCommand("/dgraexecute");
    RemoveCommand("/dgzaexecute");
    RemoveCommand("/dobserve");
    RemoveCommand("/dquery");

    RemoveMQ2Data("DanNet");
    delete pDanNetType;

    delete pDanObservationType;
}

// Called once directly after initialization, and then every time the gamestate changes
PLUGIN_API VOID SetGameState(DWORD GameState) {
    // TODO: Figure out why we can't re-use the instance through zoning
    // (it should be maintainable through the GAMESTATE_LOGGINGIN -> GAMESTATE_INGAME cycle, but causes my node instance to get memset to null)
    if (GameState == GAMESTATE_LOGGINGIN || GameState == GAMESTATE_UNLOADING) { // UNLOADING is /q
        Node::get().save_channels();                                            // these will get rejoined on actor load
        Node::get().exit();
        Node::get().shutdown();
    }

    if (GameState == GAMESTATE_CHARSELECT) {
        Node::get().clear_saved_channels();
    }

    // TODO: What about other gamestates? There is potential for messaging there, but the naming would be off without a character
    if (GameState == GAMESTATE_INGAME) {
        Node::get().enter();

        std::set<std::string> groups = ParseArray(ReadVar("General", "Groups"));
        for (const auto& group : groups)
            Node::get().join(group);

        groups = ParseArray(ReadVar(Node::get().name(), "Groups"));
        for (const auto& group : groups)
            Node::get().join(group);

        groups = { "all" };
        const auto pChar = GetCharInfo();
        if (pChar && pChar->pSpawn) {
            const std::string cls = Node::get().init_string(pEverQuest->GetClassThreeLetterCode(pChar->pSpawn->mActorClient.Class));
            groups.emplace(cls.c_str());
            for (auto category : { "Tank", "Priest", "Melee", "Caster" }) {
                std::set<std::string> arr = ParseArray(ReadVar("General", category));
                if (arr.find(cls) != arr.end())
                    groups.emplace(Node::get().init_string(category));
            }
        }

        for (const auto& group : groups)
            Node::get().join(group);
    }
}

PLUGIN_API VOID OnBeginZone() {
    // This stuff needs to be here to handle the thread getting closed (this is from quick-camping)
    Node::get().save_channels();
    Node::get().exit();
    Node::get().shutdown();
}

PLUGIN_API VOID OnCleanUI() {
    if (!GetCharInfo()) { // can potentially check game state here, too. 255 (GAMESTATE_UNLOADING) might work. For some reason, `SetGameState` doesn't always get called
        Node::get().save_channels();
        Node::get().exit();
        Node::get().shutdown();
    }
}

// This is called every time MQ pulses
PLUGIN_API VOID OnPulse() {
    if (GetGameState() == GAMESTATE_INGAME) {
        Node::get().recv();

        if (Node::get().last_group_check() + 1000 < MQGetTickCount64()) {
            // time to check our group!
            Node::get().last_group_check(MQGetTickCount64());

            // we need to get all channels we have joined that are group channels no matter what the case
            std::set<std::string> groups = ([]() {
                std::set<std::string> own_groups = Node::get().get_own_groups();
                std::set<std::string> filtered_groups;
                std::copy_if(own_groups.cbegin(), own_groups.cend(), std::inserter(filtered_groups, filtered_groups.begin()), [](const std::string& group) {
                    return group.find("group_") == 0 || group.find("raid_") == 0 || group.find("zone_") == 0;
                });

                return filtered_groups;
            })();

            auto check_and_join = [&groups](const std::string& prefix, const std::function<bool(std::string & name)>& get_name) {
                std::string name;
                if (get_name(name)) {
                    name = Node::get().get_full_name(name);
                    const auto group_it = groups.find(prefix + name);
                    if (group_it != groups.end()) {
                        // okay, we are already in the group we care about, but we need to leave all the other groups we don't care about.
                        groups.erase(group_it);
                    } else {
                        // we need to leave all the groups in groups, but we also need to join our new group
                        Node::get().join(prefix + name);
                    }
                }
            };

            check_and_join("group_", [](std::string& name) {
                const PCHARINFO pChar = GetCharInfo();
                if (pChar && pChar->pGroupInfo && pChar->pGroupInfo->pLeader) {
                    name = std::string(pChar->pGroupInfo->pLeader->Name);
                    return true;
                }

                return false;
            });

            check_and_join("raid_", [](std::string& name) {
                if (pRaid && pRaid->RaidLeaderName[0]) {
                    name = std::string(pRaid->RaidLeaderName);
                    return true;
                }
                return false;
            });

            check_and_join("zone_", [](std::string& name) {
                auto pZone = reinterpret_cast<PZONEINFO>(pZoneInfo);
                if (pZone) {
                    name = std::string(pZone->ShortName);
                    return true;
                }

                return false;
            });

            // at this point we are guaranteed that this only has bad groups in it
            for (const auto& group : groups) {
                Node::get().leave(group);
            }
        }

        if (Node::get().last_macro_check() != gMacroBlock.get()) {
            Node::get().last_macro_check(gMacroBlock.get());

            // if we have transitioned into not a macro then we need to loop through all
            // our observer variables and leave any of the groups that rely on macro variables.
            Node::get().forget_if(DoesVarExist);
        }

        Node::get().do_next();
        Node::get().publish<Update>();
    }
}

#pragma endregion
