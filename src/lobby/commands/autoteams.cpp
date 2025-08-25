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

#include "autoteams.hpp"
#include "lobby/server_lobby_commands.hpp"
#include "lobby/stk_command.hpp"
#include "lobby/stk_command_context.hpp"
#include "network/protocols/server_lobby.hpp"
#include "network/server_config.hpp"
#include "network/stk_host.hpp"
#include "network/stk_peer.hpp"
#include "utils/string_utils.hpp"
#include "utils/log.hpp"
#include "race/race_manager.hpp"
#include "online/http_request.hpp"
#include "online/request_manager.hpp"
#include <thread>
#include <cstdio>
#include <array>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <fstream>
#include <parser/argline_parser.hpp>
#include <string>

// NOTE: You need permission from the TierS Servers group to use this. If it is activated without permission, it will fail. Fallback: legacy version

static const char* const LOGNAME = "AutoteamsCommand";
static std::string join_names(const std::vector<std::string>& names)
{
    std::string s;
    for (size_t i = 0; i < names.size(); i++)
    {
        s += names[i];
        if (i + 1 < names.size()) s += ", ";
    }
    return s;
}



static bool run_external_balancer(const std::vector<std::string>& players,
    std::vector<std::string>& blue, std::vector<std::string>& red)
{
    if (!ServerConfig::m_autoteams_reg_requests)
    {
        Log::info(LOGNAME, "External autoteams requests disabled in config");
        return false;
    }


    // Create HTTP request to ServerConfig::m_autoteams_reg_requests
    std::string url = std::string(ServerConfig::m_autoteams_server_url.c_str()) + "/team_balancer";
    Online::HTTPRequest request(0); // Priority 0
    request.setURL(url);
    std::string players_str;
    for (size_t i = 0; i < players.size(); i++)
    {
        if (i > 0) players_str += ",";
        players_str += players[i];
    }
    request.addParameter("players", players_str);
    request.executeNow();
    if (request.hadDownloadError())
    {
        Log::warn(LOGNAME, "External balancer request failed");
        return false;
    }

    std::string response = request.getData();

    // Parse the response (expected format: "Blue Team: player1, player2\nRed Team: player3, player4")
    auto parse_line = [](const std::string& prefix, const std::string& line,
                         std::vector<std::string>& out_list) -> bool
    {
        if (line.rfind(prefix, 0) != 0)
            return false;
        std::string names = line.substr(prefix.size());
        names.erase(names.begin(), std::find_if(names.begin(), names.end(), [](unsigned char ch){ return !std::isspace(ch); }));
        size_t start = 0;
        while (start < names.size())
        {
            size_t comma = names.find(',', start);
            std::string token = names.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
            auto l = token.begin();
            auto r = token.end();
            while (l != r && std::isspace((unsigned char)*l)) ++l;
            while (r != l && std::isspace((unsigned char)*(r - 1))) --r;
            if (l != r) out_list.emplace_back(std::string(l, r));
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
        return true;
    };

    std::istringstream iss(response);
    std::string line;
    bool got_blue = false, got_red = false;
    while (std::getline(iss, line))
    {
        if (!got_blue)
            got_blue = parse_line("Blue Team:", line, blue);
        if (!got_red)
            got_red = parse_line("Red Team:", line, red);
        if (got_blue && got_red) break;
    }
    if (!(got_blue && got_red))
    {
        Log::warn(LOGNAME, "run_external_balancer: failed to parse Blue/Red Team lines from response");
        return false;
    }
    return true;
}


bool AutoteamsCommand::execute(nnwcli::CommandExecutorContext* const ctx, void* const data)
{
    STK_CTX(stk_ctx, ctx);

    auto parser = ctx->get_parser();
    std::string mode;
    parser->parse_string(mode, false /*mandatory?*/);
    parser->parse_finish();
    ServerLobby* const lobby = stk_ctx->get_lobby();

    if (ServerConfig::m_soccer_roulette)
    {
        ctx->write("Autoteams command is not available when soccer roulette is enabled.");
        ctx->flush();
        return false;
    }

    if (RaceManager::get()->getMinorMode() != RaceManager::MINOR_MODE_SOCCER)
    {
        ctx->write("This command is only for soccer mode.");
        ctx->flush();
        return false;
    }
    else if (lobby->getCurrentState() != ServerLobby::WAITING_FOR_START_GAME)
    {
        ctx->write("Auto team generation not possible during game.");
        ctx->flush();
        return false;
    }
    CMD_VOTABLE(data, true);
    CMD_SELFVOTE_PERMLOWER_CROWN(stk_ctx, data, m_min_veto, parser);
    auto elorank = std::make_pair(0U, 1500);
    std::string msg = "";
    auto peers = STKHost::get()->getPeers();
    std::vector <std::pair<std::string, int>> player_vec;
    std::vector <std::string> eligible_names;
    for (auto peer : peers)
    {
        if (peer->isEligibleForGame())
        {
            for (auto player : peer->getPlayerProfiles())
            {
                std::string username = StringUtils::wideToUtf8(player->getName());
                // TODO: use soccer_elo_ranking from ServerLobby from that,
                // which would be able to fetch ranking from the database
                std::ifstream file(ServerConfig::m_soccer_ranking_path.c_str());
                std::string line;
                bool found = false;
                int default_elo = 1500;
                while (std::getline(file, line))
                {
                    std::istringstream iss(line);
                    std::string name;
                    int games;
                    float team_size, goals_per_game, win_rate;
                    int elo;
                    iss >> name >> games >> team_size >> goals_per_game >> win_rate >> elo;
                    if (name == username)
                    {
                        player_vec.push_back(std::pair<std::string, int>(username, elo));
                        found = true;
                        break;
                    }
                }
                if (!found)
                    player_vec.push_back(std::pair<std::string, int>(username, default_elo));
                eligible_names.push_back(username);
            }
        }
	}

    int min = 0;
    
    // Require at least 2 eligible players
    if (eligible_names.size() < 2)
    {
        Log::warn(LOGNAME, "Not enough eligible players for autoteams (need at least 2)");
        lobby->sendStringToAllPeers("Not enough players to auto-team (need at least 2).");
        return false;
    }
    std::vector <std::pair<std::string, int>> player_copy = player_vec;
    // Drop lowest elo if odd number of players, unless using legacy
    if (mode != "legacy" && player_vec.size() % 2 == 1 && player_vec.size() >= 3)
    {
        for (size_t i3 = 0; i3 < player_copy.size(); i3++)
        {
            if (player_copy[i3].second <= player_copy[min].second)
            {
                min = static_cast<int>(i3);
            }
        }
        player_copy.erase(player_copy.begin() + min);
        int min_idx = std::min(min, (int)player_vec.size() - 1);
        msg = "Dropping lowest ELO due to odd player count: " + player_vec[min_idx].first;
        Log::info(LOGNAME, msg.c_str());
        if (min >= 0 && min < (int)eligible_names.size())
            eligible_names.erase(eligible_names.begin() + std::min(min, (int)eligible_names.size() - 1));
    }

    // Use external balancer by default, legacy only for "legacy" mode
    if (mode != "legacy")
    {
        // Check if external requests are enabled
        if (ServerConfig::m_autoteams_reg_requests)
        {
            lobby->sendStringToAllPeers("Generating teams.. please wait");
            std::vector<std::string> blue, red;
            bool ok = run_external_balancer(eligible_names, blue, red);
            if (ok && !blue.empty() && !red.empty())
            {
                lobby->m_current_teams = std::make_pair(blue, red);
            lobby->m_player_vec = player_vec;
            lobby->m_min_player_idx = -1; // ensure no legacy random assignment
            lobby->applyTeamSelection(true);
            lobby->sendStringToAllPeers("Teams have been automatically balanced! (external)");
            return true;
            }
        }
    }
    lobby->m_current_teams = lobby->createBalancedTeams(player_copy);
    lobby->m_min_player_idx = min;
    lobby->m_player_vec = player_vec;
    lobby->applyTeamSelection(true);
    lobby->sendStringToAllPeers("Teams have been automatically balanced! (legacy)");
    return true;
}
