#include "Commands.h"
#include "../MQ2Plugin.h"

#include <archive.h>

using namespace MQ2DanNet;

const bool Echo::callback(std::stringstream&& args) {
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

std::stringstream Echo::pack(const std::string& message) {
    std::stringstream send_stream;
    Archive<std::stringstream> send(send_stream);
    send << message;
    
    return send_stream;
}

const bool Query::callback(std::stringstream&& args) {
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
            CHAR szQuery[MAX_STRING];
            strcpy_s(szQuery, request.c_str());
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
std::stringstream Query::pack(const std::string& request) {
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
const bool Observe::callback(std::stringstream&& args) {
    Archive<std::stringstream> received(args);
    std::string from;
    std::string group;
    std::string key;
    std::string query;

    try {
        received >> from >> group >> key >> query;
        DebugSpewAlways("OBSERVE --> FROM: %s, GROUP: %s, QUERY: %s", from.c_str(), group.c_str(), query.c_str());

        CHAR szQuery[MAX_STRING];
        strcpy_s(szQuery, query.c_str());
        MQ2TYPEVAR Result;

        std::stringstream args;
        Archive<std::stringstream> ar(args);

        // only install the observer if it is a valid query
        ParseMacroData(szQuery, MAX_STRING);
        if (ParseMQ2DataPortion(szQuery, Result) && Result.Type) {
            //std::string final_query = "${Me." + query + "}";
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

std::stringstream Observe::pack(const std::string& query) {
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

const bool Update::callback(std::stringstream&& args) {
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

std::stringstream Update::pack(const std::string& query) {
    std::stringstream send_stream;
    Archive<std::stringstream> send(send_stream);

    PCHARINFO pChar = GetCharInfo();
    if (pChar) {
        CHAR szQuery[MAX_STRING];
        strcpy_s(szQuery, query.c_str());
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
