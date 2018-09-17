#pragma once

#include "Node.h"

namespace MQ2DanNet {
    COMMAND(Echo, const std::string& message);

    // NOTE: Query is asynchronous
    COMMAND(Query, const std::string& request);

    COMMAND(Observe, const std::string& query);

    COMMAND(Update, const std::string& query);
}