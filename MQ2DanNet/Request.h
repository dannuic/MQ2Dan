#pragma once
#include "Node.h"

namespace MQ2DanNet {
    class Query : public Command {
    public:
        Query() : Command() {}
        const std::function<void()> callback(std::shared_ptr<std::stringstream> arg_p) const;
    };

    class Request {
    public:
        Request(const std::string& query);
        ~Request() = default;

    private:
        const std::string query;
    };

    class Response : public Command {
    public:
        Response() : Command() {}
        const std::function<void()> callback(std::shared_ptr<std::stringstream> arg_p) const;
    };
}