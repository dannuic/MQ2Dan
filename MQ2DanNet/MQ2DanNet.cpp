// MQ2DanNet.cpp : Defines the entry point for the DLL application.
//

// PLUGIN_API is only to be used for callbacks.  All existing callbacks at this time
// are shown below. Remove the ones your plugin does not use.  Always use Initialize
// and Shutdown for setup and cleanup, do NOT do it in DllMain.

#include "Node.h"
#include "Commands.h"
#include "../MQ2Plugin.h"

#include <archive.h>
#include <regex>
#include <sstream>

PLUGIN_VERSION(0.1);
PreSetup("MQ2DanNet");

using namespace MQ2DanNet;

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
