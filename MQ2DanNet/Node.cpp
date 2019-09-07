#include <zyre.h> // IMPORTANT! This must be included first because it includes <winsock2.h>, which needs to come before <windows.h> -- we cannot guarantee no inclusion of <windows.h> in other headers
#include <archive.h>

#include <iterator>
#include <functional>
#include <numeric>
#include <sstream>
#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <string>

#include "Node.h"
#include <MQ2Plugin.h>

using namespace MQ2DanNet;

//auto libczmq = LoadLibrary("libczmq.dll");
//auto libzyre = LoadLibrary("libzyre.dll");

#define ZYRE_FUNC(Name, Ret, ...) Ret(__stdcall* Name)(__VA_ARGS__) = (Ret(__stdcall*)(__VA_ARGS__))GetProcAddress(libzyre, #Name)
#define CZMQ_FUNC(Name, Ret, ...) Ret(__stdcall* Name)(__VA_ARGS__) = (Ret(__stdcall*)(__VA_ARGS__))GetProcAddress(libczmq, #Name)

//CZMQ_FUNC(zlist_first, void*, zlist_t*);
//CZMQ_FUNC(zlist_next, void*, zlist_t*);
//CZMQ_FUNC(zlist_destroy, void, zlist_t**);
//CZMQ_FUNC(zsock_signal, int, void*, byte);
//CZMQ_FUNC(zpoller_new, zpoller_t*, void*, ...);
//CZMQ_FUNC(zpoller_wait, void*, zpoller_t*, int);
//CZMQ_FUNC(zpoller_expired, bool, zpoller_t*);
//CZMQ_FUNC(zpoller_terminated, bool, zpoller_t*);
//CZMQ_FUNC(zmsg_recv, zmsg_t*, void*);
//CZMQ_FUNC(zmsg_popstr, char*, zmsg_t*);
//CZMQ_FUNC(zmsg_pop, zframe_t*, zmsg_t*);
//CZMQ_FUNC(zframe_data, byte*, zframe_t*);
//CZMQ_FUNC(zframe_size, size_t, zframe_t*);
//CZMQ_FUNC(zframe_destroy, void, zframe_t**);
//CZMQ_FUNC(zmsg_destroy, void, zmsg_t**);
//CZMQ_FUNC(zpoller_set_nonstop, void, zpoller_t*, bool);
//CZMQ_FUNC(zclock_sleep, void, int);
//CZMQ_FUNC(zframe_new, zframe_t*, const void*, size_t);
//CZMQ_FUNC(zmsg_new, zmsg_t*, void);
//CZMQ_FUNC(zmsg_prepend, int, zmsg_t*, zframe_t**);
//CZMQ_FUNC(zmsg_pushstr, int, zmsg_t*, const char*);
//CZMQ_FUNC(ziflist_new, ziflist_t*, void);
//CZMQ_FUNC(ziflist_first, const char *, ziflist_t*);
//CZMQ_FUNC(ziflist_next, const char *, ziflist_t*);
//CZMQ_FUNC(ziflist_destroy, void, ziflist_t**);
//CZMQ_FUNC(zactor_new, zactor_t*, zactor_fn*, void*);
//CZMQ_FUNC(zactor_destroy, void, zactor_t**);
//CZMQ_FUNC(zsys_shutdown, int, void);
//ZYRE_FUNC(zyre_shout, int, zyre_t*, const char*, zmsg_t**);
//ZYRE_FUNC(zyre_whisper, int, zyre_t*, const char*, zmsg_t**);
//ZYRE_FUNC(zyre_uuid, const char*, zyre_t*);
//ZYRE_FUNC(zyre_destroy, void, zyre_t**);
//ZYRE_FUNC(zyre_join, int, zyre_t*, const char*);
//ZYRE_FUNC(zyre_leave, int, zyre_t*, const char*);
//ZYRE_FUNC(zyre_new, zyre_t*, const char*);
//ZYRE_FUNC(zyre_set_interface, void, zyre_t*, const char*);
//ZYRE_FUNC(zyre_set_header, void, zyre_t*, const char*, const char*, ...);
//ZYRE_FUNC(zyre_start, int, zyre_t*);
//ZYRE_FUNC(zyre_own_groups, zlist_t*, zyre_t*);
//ZYRE_FUNC(zyre_socket, zsock_t*, zyre_t*);
//ZYRE_FUNC(zyre_stop, void, zyre_t*);
//ZYRE_FUNC(zyre_destroy, void, zyre_t**);
//ZYRE_FUNC(zyre_event_peer_uuid, const char*, zyre_event_t*);
//ZYRE_FUNC(zyre_event_group, const char*, zyre_event_t*);
//ZYRE_FUNC(zyre_event_peer_name, const char*, zyre_event_t*);
//ZYRE_FUNC(zyre_event_type, const char*, zyre_event_t*);
//ZYRE_FUNC(zyre_event_new, zyre_event_t*, zyre_t*);
//ZYRE_FUNC(zyre_socket, zsock_t*, zyre_t*);

MQ2DANNET_NODE_API Node& Node::get() {
    static Node instance;
    return instance;
}

MQ2DANNET_NODE_API void Node::join(const std::string& group) {
    if (_groups.emplace(group).second && _node)
        zyre_join(_node, group.c_str());
}

MQ2DANNET_NODE_API void Node::leave(const std::string& group) {
    if (_groups.erase(group) > 0 && _node)
        zyre_leave(_node, group.c_str());
}

MQ2DANNET_NODE_API void MQ2DanNet::Node::on_join(std::function<bool(const std::string&, const std::string&)> callback) {
    _join_callbacks.push_back(std::move(callback));
}

MQ2DANNET_NODE_API void MQ2DanNet::Node::on_leave(std::function<bool(const std::string&, const std::string&)> callback) {
    _leave_callbacks.push_back(std::move(callback));
}

MQ2DANNET_NODE_API void Node::publish(const std::string& group, const std::string& cmd, std::stringstream&& args) {
    if (!_node)
        return;

    args.seekg(0, args.end);
    size_t args_size = (size_t)args.tellg();
    args.seekg(0, args.beg);

    char *args_buf = new char[args_size];
    args.read(args_buf, args_size);

    zframe_t *args_frame = zframe_new(args_buf, args_size);

    zmsg_t *msg = zmsg_new();
    zmsg_prepend(msg, &args_frame);
    zmsg_pushstr(msg, cmd.c_str());

    zyre_shout(_node, group.c_str(), &msg);

    delete[] args_buf;
}

MQ2DANNET_NODE_API void Node::respond(const std::string& name, const std::string& cmd, std::stringstream&& args) {
    if (!_node)
        return;

    auto uuid_it = _peers.find(name);
    if (uuid_it != _peers.end()) {
        std::string uuid = uuid_it->second;
        std::transform(uuid.begin(), uuid.end(), uuid.begin(), ::toupper);

        args.seekg(0, args.end);
        size_t args_size = (size_t)args.tellg();
        args.seekg(0, args.beg);

        char *args_buf = new char[args_size];
        args.read(args_buf, args_size);

        zframe_t *args_frame = zframe_new(args_buf, args_size);

        zmsg_t *msg = zmsg_new();
        zmsg_prepend(msg, &args_frame);
        zmsg_pushstr(msg, cmd.c_str());

        zyre_whisper(_node, uuid.c_str(), &msg);

        delete[] args_buf;
    }
}

MQ2DANNET_NODE_API const std::string Node::get_info() {
    if (!_node)
        return "NONET";

    std::stringstream output;
    output << _node_name << " " << zyre_uuid(_node);

    output << std::endl << "PEERS: ";
    for (auto peer : _peers) {
        output << std::endl << " --> " << peer.first; // don't worry about the UUID, let's just keep that hidden.
    }

    output << std::endl << "GROUPS: ";
    for (auto group : _groups) {
        output << std::endl << " --> " << group;
    }

    output << std::endl << "GROUP PEERS: ";
    for (auto group : _group_peers) {
        output << std::endl << " :: " << group.first;
        for (auto peer : group.second) {
            output << std::endl << " --> " << peer;
        }
    }

    return output.str();
}

MQ2DANNET_NODE_API const std::string MQ2DanNet::Node::get_interfaces() {
    ziflist_t *l = ziflist_new();
    std::string ifaces = ziflist_first(l);
    while (auto iface = ziflist_next(l)) {
        ifaces += "\r\n";
        ifaces += iface;
    }
    
    ziflist_destroy(&l);

    return ifaces;
}

MQ2DANNET_NODE_API const std::string MQ2DanNet::Node::get_full_name(const std::string& name) {
    std::string ret = name;

    // this works because names and servers can't have underscores in them, therefore if 
    // there is no underscore in the string, we assume a local character name was passed
    if (std::string::npos == name.find_last_of("_")) {
        ret = EQADDR_SERVERNAME + std::string("_") + ret;
    }

    std::transform(ret.begin(), ret.end(), ret.begin(), ::tolower);
    return init_string(ret.c_str());
}

void Node::node_actor(zsock_t *pipe, void *args) {
    Node *node = reinterpret_cast<Node*>(args);
    if (!node) return;


    node->_node = zyre_new(node->_node_name.c_str());
    if (!node->_node) throw new std::invalid_argument("Could not create node");

    CHAR szInterface[MAX_STRING] = { 0 };
    GetPrivateProfileString("MQ2DanNet", "Interface", "", szInterface, MAX_STRING, INIFileName);
    if (szInterface && strlen(szInterface) > 0) {
        zyre_set_interface(node->_node, szInterface);
    }

    zyre_set_header(node->_node, "HEADER KEY", "%s", "HEADER VAL"); // this isn't necessary, just testing it
    zyre_start(node->_node);

    // guarantee that we are in the groups we think we are in
    zlist_t *groups = zyre_own_groups(node->_node);
    const char *group = reinterpret_cast<const char *>(zlist_first(groups));
    std::set<std::string> groups_set;
    while (group) {
        groups_set.emplace(group);
        group = reinterpret_cast<const char*>(zlist_next(groups));
    }
    zlist_destroy(&groups);

    std::set<std::string> groups_diff;
    std::set_difference(groups_set.begin(), groups_set.end(), node->_groups.begin(), node->_groups.end(), std::inserter(groups_diff, groups_diff.begin()));
    for (auto unjoined : groups_diff) {
        node->join(unjoined);
    }

    zsock_signal(pipe, 0); // ready signal, required by zactor contract
    
    auto my_sock = zyre_socket(node->_node);
    zpoller_t *poller = zpoller_new(pipe, my_sock, (void*)NULL);

    // TODO: This doesn't appear necessary, but experiment with it
    //zpoller_set_nonstop(poller, true);

    DebugSpewAlways("Starting actor loop for %s : %s", node->_node_name.c_str(), zyre_uuid(node->_node));

    bool terminated = false;
    while (!terminated) {
        void *which = zpoller_wait(poller, -1);

        bool did_expire = zpoller_expired(poller);
        bool did_terminate = zpoller_terminated(poller);
        if (which == pipe) {
            // we've got a command from the caller here
            //DebugSpewAlways("Got message from caller");
            zmsg_t *msg = zmsg_recv(which);
            if (!msg) break; // Interrupted

            // strings index commands because zeromq has the infrastructure and it's not time-critical
            // otherwise, we'd have to deal with byte streams, which is totally unnecessary
            char *command = zmsg_popstr(msg);
            zframe_t *body = zmsg_pop(msg);
            char *name = zmsg_popstr(msg);
            char *group = zmsg_popstr(msg);

            DebugSpewAlways("command: %s, name: %s, group: %s", command, name, group);

            if (streq(command, "$TERM")) // need to handle $TERM per zactor contract
                terminated = true;
            else {
                std::stringstream args;
                Archive<std::stringstream> args_ar(args);
                args_ar << std::string(name ? name : "") << std::string(group ? group : "");
                char *body_data = (char *)zframe_data(body);
                size_t body_size = zframe_size(body);

                args.write(body_data, body_size);

                node->queue_command(command, std::move(args));
            }

            if (group) free(group);
            if (name) free(name);
            if (body) zframe_destroy(&body);
            if (command) free(command);

            zmsg_destroy(&msg);
        } else if (which == zyre_socket(node->_node)) {
            // we've received something over our socket
            //DebugSpewAlways("Got a message over the socket");
            zyre_event_t *z_event = zyre_event_new(node->_node);
            if (!z_event) break;

            const char *szEventType = zyre_event_type(z_event);
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
                    // create or replace
                    node->_peers[name] = uuid;
                }
            } else if (event_type == "EXIT") {
                // remove from groups too, even though LEAVE should take care of this
                for (auto _peer_list : node->_group_peers) {
                    _peer_list.second.erase(name);
                }

                node->_peers.erase(name);
            } else if (event_type == "JOIN") {
                std::string group = init_string(zyre_event_group(z_event));

                if (group.empty()) {
                    DebugSpewAlways("MQ2DanNet: JOIN with empty group with name %s, will not add to lists.", name.c_str());
                } else {
                    node->_group_peers[group].emplace(name); // [] means that we will create or access
                    for (auto callback_it = node->_join_callbacks.begin(); callback_it != node->_join_callbacks.end(); ) {
                        if ((*callback_it)(name, group))
                            node->_join_callbacks.erase(callback_it);
                        else
                            ++callback_it;
                    }
                    DebugSpewAlways("JOIN %s : %s", group.c_str(), name.c_str());
                }
            } else if (event_type == "LEAVE") {
                std::string group = init_string(zyre_event_group(z_event));

                if (group.empty()) {
                    DebugSpewAlways("MQ2DanNet: LEAVE with empty group with name %s, will not remove from lists.", name.c_str());
                } else {
                    auto list_it = node->_group_peers.find(group);
                    if (list_it != node->_group_peers.end()) {
                        list_it->second.erase(name);
                        if (list_it->second.empty())
                            node->_group_peers.erase(list_it);
                    }

                    for (auto callback_it = node->_leave_callbacks.begin(); callback_it != node->_leave_callbacks.end(); ) {
                        if ((*callback_it)(name, group))
                            node->_leave_callbacks.erase(callback_it);
                        else
                            ++callback_it;
                    }
                }
                DebugSpewAlways("LEAVE %s : %s", group.c_str(), name.c_str());
            } else if (event_type == "WHISPER") {
                // use get_msg because we want ownership to pass the command up
//ZYRE_FUNC(zyre_event_get_msg, zmsg_t*, zyre_event_t*);
                zmsg_t *message = zyre_event_get_msg(z_event);
                if (!message) {
                    DebugSpewAlways("MQ2DanNet: Got NULL WHISPER message from %s", name.c_str());
                } else {
//CZMQ_FUNC(zmsg_addstr, int, zmsg_t*, const char*);
//CZMQ_FUNC(zmsg_send, int, zmsg_t**, void*);
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
                    zmsg_t *message = zyre_event_get_msg(z_event);
                    if (!message) {
                        DebugSpewAlways("MQ2DanNet: Got NULL SHOUT message from %s in %s", name.c_str(), group.c_str());
                    } else {
                        // note that this goes to the end of the message
                        zmsg_addstr(message, name.c_str());
                        zmsg_addstr(message, group.c_str()); 
                        zmsg_send(&message, node->_actor);
                    }
                }
            } else if (event_type == "EVASIVE") {
                // not sure if anything needs to be done here?
                // also, turns out this is done a lot so let's just mute it to reduce spam
                //DebugSpewAlways("%s is being evasive.", name.c_str());
            } else {
                DebugSpewAlways("MQ2DanNet: Got unhandled event type %s.", event_type.c_str());
            }

//ZYRE_FUNC(zyre_event_destroy, void, zyre_event_t**);
            zyre_event_destroy(&z_event);
        }
    }

//CZMQ_FUNC(zpoller_sdestroy, void, zpoller_t**);
    zpoller_destroy(&poller);

    for (auto _group : node->_groups)
        node->leave(_group);

    zyre_stop(node->_node);
    zclock_sleep(100);
    zyre_destroy(&node->_node);
}

std::string Node::init_string(const char *szStr) {
    if (szStr) {
        std::string str(szStr);
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    }

    return std::string();
}

MQ2DANNET_NODE_API std::string MQ2DanNet::Node::register_response(std::function<bool(std::stringstream&&)> callback) {
    // C99, 6.2.5p9 -- guarantees that this will wrap to 0 once we reach max value
    unsigned char next_val = *(_response_keys.crbegin()) + 1;
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
    for (auto observer : _observer_map) {
        if (observer.second.query == query)
            return observer_group(observer.first);
    }

    // didn't find anything, insert a new one
    Query obs(query);

    // C99, 6.2.5p9 -- guarantees that this will wrap to 0 once we reach max value
    unsigned int position = _observer_map.crbegin()->first + 1;

    _observer_map[position] = std::move(obs);
    return observer_group(position);
}

MQ2DANNET_NODE_API void MQ2DanNet::Node::observe(const std::string& group, const std::string& name, const std::string& query) {
    join(group);
    _observed_map[Observed(query, name)] = group;
}

MQ2DANNET_NODE_API void MQ2DanNet::Node::forget(const std::string& group) {
    auto map_it = std::find_if(_observed_map.begin(), _observed_map.end(), [group](const std::pair<Observed, std::string> kv) { return kv.second == group; });
    if (map_it != _observed_map.end())
        _observed_map.erase(map_it);

    _observed_data.erase(group);

    leave(group);
}

MQ2DANNET_NODE_API void MQ2DanNet::Node::forget(const std::string& name, const std::string& query) {
    auto map_it = _observed_map.find(Observed(query, name));
    if (map_it != _observed_map.end()) {
        std::string group = map_it->second;
        _observed_data.erase(group);
        _observed_map.erase(map_it);

        leave(group);
    }
}

MQ2DANNET_NODE_API void MQ2DanNet::Node::update(const std::string& group, const MQ2TYPEVAR& data) {
    auto data_it = _observed_data.find(group);
    if (data_it != _observed_data.end()) {
        data_it->second.data = data;
        data_it->second.received = MQGetTickCount64();
    } else {
        Observation obs;
        obs.data = data;
        obs.received = MQGetTickCount64();
        _observed_data[group] = obs;
    }
}

MQ2DANNET_NODE_API const Node::Observation MQ2DanNet::Node::read(const std::string& group) {
    auto data_it = _observed_data.find(group);
    if (data_it != _observed_data.end()) {
        return data_it->second;
    } else {
        return Observation();
    }
}

MQ2DANNET_NODE_API const Node::Observation MQ2DanNet::Node::read(const std::string& name, const std::string& query) {
    auto map_it = _observed_map.find(Observed(query, name));
    if (map_it != _observed_map.end()) {
        return read(map_it->second);
    } else {
        return Observation();
    }
}

// stub these for now, nothing to do here since memory is managed elsewhere (and all registered commands will go away)
Node::Node() {}
Node::~Node() {}

std::string MQ2DanNet::Node::peers_arr() {
    std::string delimiter = "|";
    return std::accumulate(_peers.cbegin(), _peers.cend(), std::string(),
        [delimiter](const std::string& s, const std::pair<const std::string, std::string>& p) {
        return s + (s.empty() ? std::string() : delimiter) + p.first;
    });
}

Node::Observation MQ2DanNet::Node::query(const std::string& query) {
    if (query.empty() || query != _current_query) {
        _current_query = query;
        _query_result = Observation();
    }

    return _query_result;
}

void MQ2DanNet::Node::query_result(const Observation& obs) {
    _query_result = obs;
}

void Node::enter() {
    PCHARINFO pChar = GetCharInfo();
    if (!pChar)
        return;

    _node_name = get_full_name(pChar->Name);

    DebugSpewAlways("Spinning up actor for %s", _node_name.c_str());
    _actor = zactor_new(Node::node_actor, this);
}

void Node::exit() {
    if (_actor) {
        DebugSpewAlways("Destroying actor for %s", _node_name.c_str());
        zactor_destroy(&_actor);
    } else if (_node) {
        // in general destroying the zactor will do this, but just in case it's dangling, let's be safe
        DebugSpewAlways("WARNING: had a node without an actor in %s", _node_name.c_str());
        zyre_destroy(&_node);
    }

    _node_name = "";
}

void MQ2DanNet::Node::shutdown() {
    zsys_shutdown();
}

void Node::queue_command(const std::string& command, std::stringstream&& args) {
    // defer the actual lookup to the execution so we can handle commands that remove themselves
    _command_queue.emplace(std::make_pair(command, std::move(args)));
}

const std::string MQ2DanNet::Node::observer_group(const unsigned int key) {
    return _node_name + "_" + init_string(std::to_string(key).c_str());
}

void Node::do_next() {
    if (!_command_queue.empty()) {
        std::pair<const std::string, std::stringstream> command_pair = std::move(_command_queue.front());
        _command_queue.pop(); // go ahead and pop it off, we've moved it

        auto command_it = _command_map.find(command_pair.first);
        if (command_it != _command_map.end() && 
            command_it->second(std::move(command_pair.second))) // return true to remove the command from the map
            _command_map.erase(command_it); // this is safe because we aren't looping here
    }
}
