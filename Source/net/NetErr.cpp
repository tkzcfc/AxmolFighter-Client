#include "NetErr.h"
#include "client_game.pb.h"
#include "net/NetLog.h"

namespace net
{

uint16_t getServerInternalErrorCode()
{
    return PB::Game::CommonErrorResp::Id;
}

std::optional<std::tuple<int32_t, std::string>> parseServerInternalError(uint16_t msgid, std::string_view payload)
{
    if (msgid == PB::Game::CommonErrorResp::Id)
    {
        PB::Game::CommonErrorResp resp;
        if (!resp.ParseFromArray(payload.data(), static_cast<int>(payload.size())))
        {
            NET_LOGE("failed to parse CommonErrorResp");
            return std::nullopt;
        }
        return std::make_tuple(resp.code(), resp.message());
    }

    return std::nullopt;
}

}  // namespace net
