#pragma once

#include <crow.h>
#include <asio.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

namespace forensics{
    using json = nlohmann::json;
}