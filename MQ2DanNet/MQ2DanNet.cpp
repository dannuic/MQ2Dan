// MQ2DanNet.cpp : Defines the entry point for the DLL application.
//

// PLUGIN_API is only to be used for callbacks.  All existing callbacks at this time
// are shown below. Remove the ones your plugin does not use.  Always use Initialize
// and Shutdown for setup and cleanup, do NOT do it in DllMain.

#include "Node.h"
#include "../MQ2Plugin.h"

#include <archive.h>
#include <regex>
#include <sstream>

PLUGIN_VERSION(0.1);
PreSetup("MQ2DanNet");

namespace MQ2DanNet {
    COMMAND(Echo, const std::string& message);

    COMMAND(Execute, const std::string& command);

    // NOTE: Query is asynchronous
    COMMAND(Query, const std::string& request);

    COMMAND(Observe, const std::string& query);

    COMMAND(Update, const std::string& query);
}

using namespace MQ2DanNet;

#pragma region Commands

const bool MQ2DanNet::Echo::callback(std::stringstream&& args) {
    Archive<std::stringstream> received(args);
    std::string from;
    std::string group;
    std::string text;

    try {
        received >> from >> group >> text;
        DebugSpewAlways("ECHO --> FROM: %s, GROUP: %s, TEXT: %s", from.c_str(), group.c_str(), text.c_str());

        if (group.empty())
            WriteChatf("\ax\a-t[\ax\at %s \ax\a-t]\ax \aw%s\ax", from.c_str(), text.c_str());
        else
            WriteChatf("\ax\a-t[\ax\at %s\ax\a-t (%s) ]\ax \aw%s\ax", from.c_str(), group.c_str(), text.c_str());

        return false;
    } catch (std::runtime_error&) {
        DebugSpewAlways("MQ2DanNet::Echo -- Failed to deserialize.");
        return false;
    }
}

std::stringstream MQ2DanNet::Echo::pack(const std::string& message) {
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
        DebugSpewAlways("EXECUTE --> FROM: %s, GROUP: %s, TEXT: %s", from.c_str(), group.c_str(), command.c_str());

        std::string final_command = std::regex_replace(command, std::regex("\\$\\\\\\{"), "${");

        if (group.empty()) {
            WriteChatf("\ax\a-o[\ax\ao %s \ax\a-o]\ax \aw%s\ax", from.c_str(), final_command.c_str());
        } else {
            WriteChatf("\ax\a-o[\ax\ao %s\ax\a-o (%s) ]\ax \aw%s\ax", from.c_str(), group.c_str(), final_command.c_str());
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

std::stringstream MQ2DanNet::Execute::pack(const std::string& command) {
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
        DebugSpewAlways("QUERY --> FROM: %s, GROUP: %s, REQUEST: %s", from.c_str(), group.c_str(), request.c_str());

        std::stringstream send_stream;
        Archive<std::stringstream> send(send_stream);

        PCHARINFO pChar = GetCharInfo();
        if (pChar) {
            std::string final_request = std::regex_replace(request, std::regex("\\$\\\\\\{"), "${");

            CHAR szQuery[MAX_STRING];
            strcpy_s(szQuery, final_request.c_str());
            MQ2TYPEVAR Result;

            // Since we don't surround the query with ${}, we want to parse all the variables specified inside
            ParseMacroData(szQuery, MAX_STRING);
            // Then after all of that, we need to Evaluate the entire thing as a single variable
            // Can retrieve this data with `FindMQ2DataType(Result.Type->GetName());` and then `Result.Type->FromString(Result.VarPtr, szBuf);`
            if (ParseMQ2DataPortion(szQuery, Result) && Result.Type) {
                CHAR szBuf[MAX_STRING] = { 0 };
                strcpy_s(szBuf, Result.Type->GetName());
                if (!szBuf) strcpy_s(szBuf, "NULL");
                send << szBuf;

                Result.Type->ToString(Result.VarPtr, szBuf);
                if (!szBuf) strcpy_s(szBuf, "NULL");
                send << szBuf;
            } else {
                send << "NULL" << "NULL";
            }

            Node::get().respond(from, key, std::move(send_stream));
        } else {
            DebugSpewAlways("MQ2DanNet::Query::callback -- failed to GetCharInfo(), sending empty response.");
            Node::get().respond(from, key, std::move(std::stringstream()));
        }

        return false;
    } catch (std::runtime_error&) {
        DebugSpewAlways("MQ2DanNet::Query -- Failed to deserialize.");
        return false;
    }
}

// we're going to generate a new command and register it with Node here in addition to packing
std::stringstream MQ2DanNet::Query::pack(const std::string& request) {
    std::stringstream send_stream;
    Archive<std::stringstream> send(send_stream);

    Node::get().query(""); // first clear -- we want a Query message to get a fresh response
    Node::get().query(request); // now set our most recent request (the return won't matter here)
    auto f = [](std::stringstream&& args) -> bool {
        Archive<std::stringstream> ar(args);
        std::string from;
        std::string group;
        std::string type;
        std::string data;

        try {
            ar >> from >> group >> type >> data;

            MQ2TYPEVAR Result;

            CHAR szBuf[MAX_STRING] = { 0 };
            strcpy_s(szBuf, type.c_str());
            Result.Type = FindMQ2DataType(szBuf);

            strcpy_s(szBuf, data.c_str());
            if (Result.Type && Result.Type->FromString(Result.VarPtr, szBuf)) {
                Node::get().query_result(Node::Observation(Result, MQGetTickCount64()));
                if (Node::get().debugging())
                    WriteChatf("%s : %s -- %llu (%llu)", type.c_str(), data.c_str(), Node::get().read(group).received, MQGetTickCount64());
            } else {
                if (Node::get().debugging())
                    WriteChatf("%s : %s -- Failed to read data %llu.", type.c_str(), data.c_str(), MQGetTickCount64());
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
        DebugSpewAlways("OBSERVE --> FROM: %s, GROUP: %s, QUERY: %s", from.c_str(), group.c_str(), query.c_str());

        std::string final_query = std::regex_replace(query, std::regex("\\$\\\\\\{"), "${");

        CHAR szQuery[MAX_STRING];
        strcpy_s(szQuery, final_query.c_str());
        MQ2TYPEVAR Result;

        std::stringstream args;
        Archive<std::stringstream> ar(args);

        // only install the observer if it is a valid query
        ParseMacroData(szQuery, MAX_STRING);
        if (ParseMQ2DataPortion(szQuery, Result) && Result.Type) {
            std::string return_group = Node::get().register_observer(from.c_str(), query.c_str());
            ar << return_group;
        } else {
            ar << "NULL";
        }

        Node::get().respond(from, key, std::move(args));
    } catch (std::runtime_error&) {
        DebugSpewAlways("MQ2DanNet::Observe -- Failed to deserialize.");
    }

    return false;
}

std::stringstream MQ2DanNet::Observe::pack(const std::string& query) {
    std::stringstream send_stream;
    Archive<std::stringstream> send(send_stream);

    // this is the callback to actually start observing. We can't just do it because the observed will come back with the right group 
    auto f = [query](std::stringstream&& args) -> bool {
        Archive<std::stringstream> ar(args);
        std::string from;
        std::string group;
        std::string new_group;

        try {
            ar >> from >> group >> new_group;
            if (new_group != "NULL") {
                Node::get().observe(new_group, from, query);
            }
        } catch (std::runtime_error&) {
            DebugSpewAlways("MQ2DanNet::Observe -- response -- Failed to deserialize.");
        }

        return true;
    };

    // this registers the response from the observed that responds with a group name
    std::string key = Node::get().register_response(f);
    send << key << query;
    return send_stream;
}

const bool MQ2DanNet::Update::callback(std::stringstream&& args) {
    Archive<std::stringstream> received(args);
    std::string from;
    std::string group;
    std::string type;
    std::string data;

    try {
        received >> from >> group >> type >> data;
        DebugSpewAlways("UPDATE --> FROM: %s, GROUP: %s, DATA: %s", from.c_str(), group.c_str(), data.c_str());

        MQ2TYPEVAR Result;

        CHAR szBuf[MAX_STRING] = { 0 };
        strcpy_s(szBuf, type.c_str());
        Result.Type = FindMQ2DataType(szBuf);
        
        strcpy_s(szBuf, data.c_str());
        if (Result.Type && Result.Type->FromString(Result.VarPtr, szBuf)) {
            Node::get().update(group, Result);
            if (Node::get().debugging())
                WriteChatf("%s : %s -- %llu (%llu)", type.c_str(), data.c_str(), Node::get().read(group).received, MQGetTickCount64());
        } else {
            if (Node::get().debugging())
                WriteChatf("%s : %s -- Failed to read data %llu.", type.c_str(), data.c_str(), MQGetTickCount64());
        }
    } catch (std::runtime_error&) {
        DebugSpewAlways("MQ2DanNet::Update -- failed to deserialize.");
    }

    return false;
}

std::stringstream MQ2DanNet::Update::pack(const std::string& query) {
    std::stringstream send_stream;
    Archive<std::stringstream> send(send_stream);

    PCHARINFO pChar = GetCharInfo();
    if (pChar) {
        std::string final_query = std::regex_replace(query, std::regex("\\$\\\\\\{"), "${");

        CHAR szQuery[MAX_STRING];
        strcpy_s(szQuery, final_query.c_str());
        MQ2TYPEVAR Result;

        // Since we don't surround the query with ${}, we want to parse all the variables specified inside
        ParseMacroData(szQuery, MAX_STRING);
        // Then after all of that, we need to Evaluate the entire thing as a single variable
        // Can retrieve this data with `FindMQ2DataType(Result.Type->GetName());` and then `Result.Type->FromString(Result.VarPtr, szBuf);`
        if (ParseMQ2DataPortion(szQuery, Result) && Result.Type) {
            CHAR szBuf[MAX_STRING] = { 0 };
            strcpy_s(szBuf, Result.Type->GetName());
            if (!szBuf) strcpy_s(szBuf, "NULL");
            send << szBuf;

            Result.Type->ToString(Result.VarPtr, szBuf);
            if (!szBuf) strcpy_s(szBuf, "NULL");
            send << szBuf;
        } else {
            send << "NULL" << "NULL";
        }

    } else {
        DebugSpewAlways("MQ2DanNet::Update::pack -- failed to GetCharInfo(), sending empty response.");
    }

    return send_stream;
}

#pragma endregion

#pragma region MainPlugin

class MQ2DanNetType *pDanNetType = nullptr;
class MQ2DanNetType : public MQ2Type {
private:
    std::string Peer;
    CHAR Buf[MAX_STRING];

public:
    enum Members {
        Name,
        PeerCount,
        Peers,
        O,
        Observe,
        Q,
        Query
    };

    MQ2DanNetType() : MQ2Type("DanNet") {
        TypeMember(Name);
        TypeMember(PeerCount);
        TypeMember(Peers);
        TypeMember(O);
        TypeMember(Observe);
        TypeMember(Q);
        TypeMember(Query);
    }

    bool GetMember(MQ2VARPTR VarPtr, char* Member, char* Index, MQ2TYPEVAR &Dest) {
        Buf[0] = '\0';

        std::string local_peer = Peer;
        Peer.clear();

        PMQ2TYPEMEMBER pMember = MQ2DanNetType::FindMember(Member);
        if (!pMember) return false;

        switch ((Members)pMember->ID) {
        case Name:
            strcpy_s(Buf, Node::get().name().c_str());
            Dest.Ptr = Buf;
            Dest.Type = pStringType;
            return true;
        case PeerCount:
            Dest.DWord = Node::get().peers();
            Dest.Type = pIntType;
            return true;
        case Peers:
            strcpy_s(Buf, Node::get().peers_arr().c_str());
            Dest.Ptr = Buf;
            Dest.Type = pStringType;
            return true;
        }

        if (!local_peer.empty()) {
            switch ((Members)pMember->ID) {
            case O:
            case Observe:
                if (Index && Index[0] != '\0') {
                    Dest = Node::get().read(local_peer, Index).data;
                    if (Dest.Type == 0) {
                        // we didn't have this observer in our map
                        // first, let's set the return
                        strcpy_s(Buf, "NULL");
                        Dest.Ptr = Buf;
                        Dest.Type = pStringType;

                        // next, let's submit a request for the observer
                        Node::get().whisper<MQ2DanNet::Observe>(local_peer, Index);
                    }
                    return true;
                } else
                    return false;
            case Q:
            case Query:
                Dest = Node::get().query(Index ? Index : "").data;
                if (Index && Index[0] != '\0' && Dest.Type == 0) {
                    strcpy_s(Buf, "NULL");
                    Dest.Ptr = Buf;
                    Dest.Type = pStringType;

                    Node::get().whisper<MQ2DanNet::Query>(local_peer, Index);
                }
                return true;
            }
        }

        // default case, don't have a definition for member
        strcpy_s(Buf, "NULL");
        Dest.Ptr = Buf;
        Dest.Type = pStringType;
        return false;
    }

    void SetPeer(const std::string& peer) {
        if (Node::get().debugging())
            WriteChatf("MQ2DanNetType::SetPeer setting peer from %s to %s", Peer.c_str(), peer.c_str());
        Peer = peer;
    }

    bool ToString(MQ2VARPTR VarPtr, char* Destination) {
        strcpy_s(Destination, MAX_STRING, Peer.empty() ? "NULL" : Peer.c_str());
        Peer.clear();
        return true;
    }

    bool FromData(MQ2VARPTR &VarPtr, MQ2TYPEVAR &Source) { return false; }
    bool FromString(MQ2VARPTR &VarPtr, char* Source) { return false; }
};

BOOL dataDanNet(PCHAR Index, MQ2TYPEVAR &Dest) {
    Dest.DWord = 1;
    Dest.Type = pDanNetType;

    if (Node::get().debugging())
        WriteChatf("MQ2DanNetType::dataDanNet Index %s", Index);
    if (!Index || Index[0] == '\0' || !Node::get().has_peer(Index))
        pDanNetType->SetPeer("");
    else
        pDanNetType->SetPeer(Node::get().get_full_name(Index));

    return true;
}

PLUGIN_API VOID DInfoCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szParam[MAX_STRING] = { 0 };
    GetArg(szParam, szLine, 1);

    if (szParam && !strcmp(szParam, "interface")) {
        GetArg(szParam, szLine, 2);
        if (szParam && strlen(szParam) > 0) {
            WritePrivateProfileString("MQ2DanNet", "Interface", szParam, INIFileName);
            WriteChatf("MQ2DanNet: Set interface to %s", szParam);
        } else {
            WriteChatf("MQ2DanNet: Interfaces --\r\n%s", Node::get().get_interfaces());
        }
    } else if (szParam && !strcmp(szParam, "debug")) {
        GetArg(szParam, szLine, 2);
        if (szParam && !strcmp(szParam, "on")) {
            Node::get().debugging(true);
            WritePrivateProfileString("MQ2DanNet", "Debugging", "1", INIFileName);
        } else if (szParam && !strcmp(szParam, "off")) {
            Node::get().debugging(false);
            WritePrivateProfileString("MQ2DanNet", "Debugging", "0", INIFileName);
        } else {
            Node::get().debugging(!Node::get().debugging());
            WritePrivateProfileString("MQ2DanNet", "Debugging", Node::get().debugging() ? "1" : "0", INIFileName);
        }
    } else {
        WriteChatf("MQ2DanNet: %s", Node::get().get_info().c_str());
    }
}

PLUGIN_API VOID DJoinCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szGroup[MAX_STRING] = { 0 };
    GetArg(szGroup, szLine, 1);

    std::string group = Node::init_string(szGroup);

    if (group.empty())
        WriteChatColor("Syntax: /djoin <group> -- join named group on peer network", USERCOLOR_DEFAULT);
    else
        Node::get().join(group);
}

PLUGIN_API VOID DLeaveCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szGroup[MAX_STRING] = { 0 };
    GetArg(szGroup, szLine, 1);

    std::string group = Node::init_string(szGroup);

    if (group.empty())
        WriteChatColor("Syntax: /dleave <group> -- leave named group on peer network", USERCOLOR_DEFAULT);
    else
        Node::get().leave(group);
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
        name = Node::get().get_full_name(name);

        WriteChatf("\ax\a-t[ \ax\at-->\ax\a-t(%s) ]\ax \aw%s\ax", name.c_str(), message.c_str());
        Node::get().whisper<MQ2DanNet::Echo>(name, message);
    }
}

PLUGIN_API VOID DGtellCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szGroup[MAX_STRING] = { 0 };
    GetArg(szGroup, szLine, 1);
    auto group = Node::init_string(szGroup);
    std::string message(szLine);
    std::string::size_type n = message.find_first_not_of(" \t", 0);
    n = message.find_first_of(" \t", n);
    message.erase(0, message.find_first_not_of(" \t", n));

    if (group.empty() || message.empty())
        WriteChatColor("Syntax: /dgtell <group> <message> -- broadcast message to group", USERCOLOR_DEFAULT);
    else {
        WriteChatf("\ax\a-t[\ax\at -->\ax\a-t(%s) ]\ax \aw%s\ax", group.c_str(), message.c_str());
        Node::get().shout<MQ2DanNet::Echo>(group, message);
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
        name = Node::get().get_full_name(name);

        WriteChatf("\ax\a-o[ \ax\ao-->\ax\a-o(%s) ]\ax \aw%s\ax", name.c_str(), command.c_str());
        Node::get().whisper<MQ2DanNet::Execute>(name, command);
    }
}

PLUGIN_API VOID DGexecuteCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szGroup[MAX_STRING] = { 0 };
    GetArg(szGroup, szLine, 1);
    auto group = Node::init_string(szGroup);
    std::string command(szLine);
    std::string::size_type n = command.find_first_not_of(" \t", 0);
    n = command.find_first_of(" \t", n);
    command.erase(0, command.find_first_not_of(" \t", n));

    if (group.empty() || command.empty())
        WriteChatColor("Syntax: /dgexecute <group> <command> -- direct group to execute command", USERCOLOR_DEFAULT);
    else {
        WriteChatf("\ax\a-o[\ax\ao -->\ax\a-o(%s) ]\ax \aw%s\ax", group.c_str(), command.c_str());
        Node::get().shout<MQ2DanNet::Execute>(group, command);
    }
}

PLUGIN_API VOID DGAexecuteCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szGroup[MAX_STRING] = { 0 };
    GetArg(szGroup, szLine, 1);
    auto group = Node::init_string(szGroup);
    std::string command(szLine);
    std::string::size_type n = command.find_first_not_of(" \t", 0);
    n = command.find_first_of(" \t", n);
    command.erase(0, command.find_first_not_of(" \t", n));

    if (group.empty() || command.empty())
        WriteChatColor("Syntax: /dgaexecute <group> <command> -- direct group to execute command", USERCOLOR_DEFAULT);
    else {
        WriteChatf("\ax\a-o[\ax\ao -->\ax\a-o(%s) ]\ax \aw%s\ax", group.c_str(), command.c_str());
        Node::get().shout<MQ2DanNet::Execute>(group, command);

        std::string final_command = std::regex_replace(command, std::regex("\\$\\\\\\{"), "${");

        CHAR szCommand[MAX_STRING] = { 0 };
        strcpy_s(szCommand, final_command.c_str());
        EzCommand(szCommand);
    }
}

PLUGIN_API VOID DObserveCommand(PSPAWNINFO pSpawn, PCHAR szLine) {
    CHAR szName[MAX_STRING] = { 0 };
    GetArg(szName, szLine, 1);
    auto name = Node::init_string(szName);
    if (std::string::npos == name.find_last_of("_"))
        name = Node::get().get_full_name(name);

    CHAR szQuery[MAX_STRING] = { 0 };
    GetArg(szQuery, szLine, 2);
    std::string query(szQuery ? szQuery : "");

    if (name.empty() || query.empty()) {
        WriteChatColor("Syntax: /dobserve <name> <query> -- start observe query on name", USERCOLOR_DEFAULT);
        WriteChatColor("        /dobserve <name> <query> [drop] -- drop observe query on name", USERCOLOR_DEFAULT);
    } else {
        CHAR szParam[MAX_STRING] = { 0 };
        GetArg(szParam, szLine, 3);

        if (szParam && Node::init_string(szParam) == "drop") {
            Node::get().forget(name, query);
        } else {
            Node::get().whisper<Observe>(name, query);
        }
    }
}

// Called once, when the plugin is to initialize
PLUGIN_API VOID InitializePlugin(VOID) {
	DebugSpewAlways("Initializing MQ2DanNet");

    Node::get().register_command<MQ2DanNet::Echo>();
    Node::get().register_command<MQ2DanNet::Execute>();
    Node::get().register_command<MQ2DanNet::Query>();
    Node::get().register_command<MQ2DanNet::Observe>();
    Node::get().register_command<MQ2DanNet::Update>();

    Node::get().debugging(GetPrivateProfileInt("MQ2DanNet", "Debugging", 0, INIFileName) != 0);

    AddCommand("/dinfo", DInfoCommand);
    AddCommand("/djoin", DJoinCommand);
    AddCommand("/dleave", DLeaveCommand);
    AddCommand("/dtell", DTellCommand);
    AddCommand("/dgtell", DGtellCommand);
    AddCommand("/dexecute", DExecuteCommand);
    AddCommand("/dgexecute", DGexecuteCommand);
    AddCommand("/dgaexecute", DGAexecuteCommand);
    AddCommand("/dobserve", DObserveCommand);

    pDanNetType = new MQ2DanNetType;
    AddMQ2Data("DanNet", dataDanNet);
}

// Called once, when the plugin is to shutdown
PLUGIN_API VOID ShutdownPlugin(VOID) {
	DebugSpewAlways("Shutting down MQ2DanNet");
    Node::get().exit();

    // this is Windows-specific and needs to be done to free some dangling select() threads
    Node::get().shutdown();

    Node::get().unregister_command<MQ2DanNet::Echo>();
    Node::get().unregister_command<MQ2DanNet::Execute>();
    Node::get().unregister_command<MQ2DanNet::Query>();
    Node::get().unregister_command<MQ2DanNet::Observe>();
    Node::get().unregister_command<MQ2DanNet::Update>();

    RemoveCommand("/dinfo");
    RemoveCommand("/djoin");
    RemoveCommand("/dleave");
    RemoveCommand("/dtell");
    RemoveCommand("/dgtell");
    RemoveCommand("/dexecute");
    RemoveCommand("/dgexecute");
    RemoveCommand("/dgaexecute");
    RemoveCommand("/dobserve");

    RemoveMQ2Data("DanNet");
    delete pDanNetType;
}

// Called once directly after initialization, and then every time the gamestate changes
PLUGIN_API VOID SetGameState(DWORD GameState) {
    // TODO: Figure out why we can't re-use the instance through zoning 
    // (it should be maintainable through the GAMESTATE_LOGGINGIN -> GAMESTATE_INGAME cycle, but causes my node instance to get memset to null)
    Node::get().exit();

    // TODO: What about other gamestates? There is potential for messaging there, but the naming would be off without a character
    if (GameState == GAMESTATE_INGAME)
        Node::get().enter();
}

// This is called every time MQ pulses
PLUGIN_API VOID OnPulse(VOID) {
    Node::get().do_next();
    Node::get().publish<Update>();
}

#pragma endregion
