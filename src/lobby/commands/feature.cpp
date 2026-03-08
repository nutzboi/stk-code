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

#include "feature.hpp"
#include "lobby/server_lobby_commands.hpp"
#include "lobby/stk_command.hpp"
#include "lobby/stk_command_context.hpp"
#include "network/moderation_toolkit/player_restriction.hpp"
#include "network/protocols/server_lobby.hpp"
#include "network/server_config.hpp"
#include "network/database/abstract_database.hpp"
#include <parser/argline_parser.hpp>
#include <string>

bool FeatureCommand::execute(nnwcli::CommandExecutorContext* const ctx, void* const data)
{
    STK_CTX(stk_ctx, ctx);

    auto parser = ctx->get_parser();

    std::string message;
    parser->parse_full(message, true);

    if (stk_ctx->hasRestriction(PRF_NOCHAT))
    {
        stk_ctx->sendNoPermission();
        return false;
    }

    CMD_REQUIRE_PERM(stk_ctx, m_required_perm);

    if (message.length() < 5)
    {
        ctx->write("You need to specify the message that is at least 5 characters long.");
        ctx->flush();
        return false;
    }

    std::string player_name = stk_ctx->getProfileName();

    ServerLobby* lobby = stk_ctx->get_lobby();
    if (!lobby)
    {
        ctx->write("Failed to record a feature. Internal error (no lobby). Please inform the administrator.");
        ctx->flush();
        return false;
    }

    AbstractDatabase* db = lobby->getDatabase();
    if (!ServerConfig::m_sql_management || !db || !db->hasDatabase())
    {
        ctx->write("Failed to record a feature. Database is not configured. Please inform the administrator.");
        ctx->flush();
        return false;
    }

    if (!db->writeFeatureMessage(player_name, message))
    {
        ctx->write("Failed to record a feature. Database error. Please inform the administrator.");
        ctx->flush();
        return false;
    }

    ctx->write("Thanks for your suggestion! Your suggestion has been recorded in the database, and we will review it at some point.");
    ctx->flush();
    return true;
}
