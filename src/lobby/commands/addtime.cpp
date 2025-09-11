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

#include "addtime.hpp"
#include "lobby/server_lobby_commands.hpp"
#include "lobby/stk_command.hpp"
#include "lobby/stk_command_context.hpp"
#include "network/protocols/server_lobby.hpp"
#include "utils/log.hpp"
#include <exception>
#include <parser/argline_parser.hpp>

bool AddTimeCommand::execute(nnwcli::CommandExecutorContext* const ctx, void* const data)
{
    static const char* const LOGNAME = "AddTimeCommand";
    
    STKCommandContext* const stk_c = dynamic_cast<STKCommandContext*>(ctx);
    if (!stk_c)
        return false;

    auto parser = ctx->get_parser();

    std::string first_arg;
    parser->parse_string(first_arg);

    if (first_arg == "status")
    {
        parser->parse_finish();
        
        ServerLobby* const lobby = stk_c->get_lobby();
        int64_t current_timeout = lobby->getTimeout();
        
        if (current_timeout == std::numeric_limits<int64_t>::max())
        {
            *ctx << "Current timeout: Infinite";
        }
        else
        {
            float remaining_seconds = (current_timeout - (int64_t)StkTime::getMonoTimeMs()) / 1000.0f;
            std::string message = "Current timeout: " + std::to_string((int)remaining_seconds) + " seconds remaining";
            *ctx << message.c_str();
        }
        ctx->flush();
        return true;
    }

    unsigned short amount_sec = 0;
    try
    {
	    int parsed = std::stoi(first_arg);
	    if (parsed < 0)
		    throw std::out_of_range("negative");
	    amount_sec = static_cast<unsigned short>(parsed);
    }
    catch (const std::exception& e)
    {
	    CMD_VOTABLE(data, false);
	    *ctx << "Invalid value. Use a number between 1 and 3600 or 'status'.";
	    ctx->flush();
	    return false;
    }

    parser->parse_finish();

    if (amount_sec < 1 || (amount_sec > 3600 &&
                stk_c->getPermissionLevel() < m_perm_limitless))
    {
        CMD_VOTABLE(data, false)
        *ctx << "Seconds should be between 1 and 3600.";
        ctx->flush();
        return false;
    }

    // in general the command is votable
    CMD_VOTABLE(data, true)
    // when the player is below the specified veto level, emits the vote
    CMD_SELFVOTE_PERMLOWER_CROWN(stk_c, data, m_min_veto, parser)

    ServerLobbyCommands::DispatchData* dd =
        reinterpret_cast<ServerLobbyCommands::DispatchData*>(data);

    if (stk_c->getVeto() >= m_min_veto)
    {
        Log::info(LOGNAME, "%s adds %d to the timeout",
                stk_c->getProfileName().c_str(), amount_sec);
    }

    // when otherwise the command is run with higher privileges
    ServerLobby* const lobby = stk_c->get_lobby();
    lobby->changeTimeout(amount_sec);

    return true;
}
