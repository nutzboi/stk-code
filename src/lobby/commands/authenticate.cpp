//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2013-2015 SuperTuxKart-Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "authenticate.hpp"
#include "lobby/server_lobby_commands.hpp"
#include "lobby/stk_command.hpp"
#include "lobby/stk_command_context.hpp"
#include "network/protocols/server_lobby.hpp"
#include "network/stk_host.hpp"
#include "network/stk_peer.hpp"
#include "utils/string_utils.hpp"
#include <parser/argline_parser.hpp>
#include <string>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool AuthenticateCommand::execute(nnwcli::CommandExecutorContext* const ctx, void* const data)
{
    STK_CTX(stk_ctx, ctx);
    auto parser = ctx->get_parser();
    std::string auth_code;
    
    if (!parser->parse_string(auth_code, false))
    {
        ctx->write("\nUsage: /authenticate <6-digit-code>\n");
        ctx->write("Get your authentication code from https://tiersservers.eu after email verification.\n");
        ctx->flush();
        CMD_VOTABLE(data, false);
        return false;
    }
    
    parser->parse_finish(); // no more arguments
    CMD_VOTABLE(data, false);
    CMD_REQUIRE_PERM(stk_ctx, m_required_perm);
    
    STKPeer* player_peer = stk_ctx->get_peer();
    if (!player_peer || !player_peer->hasPlayerProfiles())
    {
        ctx->write("Error: Could not get player information.\n");
        ctx->flush();
        return false;
    }
    std::string player_name = StringUtils::wideToUtf8(player_peer->getPlayerProfiles()[0]->getName());
    if (auth_code.length() != 6)
    {
        ctx->write("Error: Authentication code must be exactly 6 digits.\n");
        ctx->flush();
        return false;
    }
    for (char c : auth_code)
    {
        if (!std::isdigit(c))
        {
            ctx->write("Error: Authentication code must contain only digits.\n");
            ctx->flush();
            return false;
        }
    }
    
    CURL *curl;
    CURLcode res;
    std::string response_string;
    curl = curl_easy_init();
    if (!curl)
    {
        ctx->write("Error: Failed to initialize HTTP request.\n");
        ctx->flush();
        return false;
    }
    
    std::string post_data = "code=" + auth_code + "&username=" + player_name;
    curl_easy_setopt(curl, CURLOPT_URL, "https://tiersservers.eu/stk_authenticate");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 10 second timeout
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        ctx->nprintf("Error: Failed to connect to authentication server: %s\n", 512, curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        ctx->flush();
        return false;
    }
    
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_cleanup(curl);
    
    try
    {
        json json_response = json::parse(response_string);
        
        if (response_code == 200 && json_response.value("success", false))
        {
            ctx->write("Authentication successful! Your account is now fully verified.\n");
            ctx->write("You can now login to https://tiersservers.eu with your account.\n");
        }
        else
        {
            std::string error_msg = json_response.value("error", "Unknown error");
            ctx->nprintf("Authentication failed: %s\n", 512, error_msg.c_str());
        }
    }
    catch (const json::exception& e)
    {
        ctx->write("Error: Invalid response from authentication server.\n");
        ctx->flush();
        return false;
    }
    
    ctx->flush();
    return true;
}
