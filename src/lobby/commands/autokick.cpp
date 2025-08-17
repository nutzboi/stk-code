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

#include "autokick.hpp"
#include "lobby/stk_command.hpp"
#include "lobby/stk_command_context.hpp"
#include "lobby/server_lobby_commands.hpp"
#include "network/protocols/server_lobby.hpp"
#include "network/server_config.hpp"
#include <parser/argline_parser.hpp>

bool AutokickCommand::execute(nnwcli::CommandExecutorContext* const ctx, void* const data)
{
    STK_CTX(stk_ctx, ctx);

    if (ServerConfig::m_ranked)
    {
        ctx->write("This command does not apply to ranked servers.");
        ctx->flush();
        return false;
    }

    auto parser = ctx->get_parser();

    bool state;
    *parser >> state; 
    parser->parse_finish();

    ServerLobby* const lobby = stk_ctx->get_lobby();

    if (lobby->getAutokickEnabled() == state)
    {
        ctx->write("Autokick is already ");
        if (state)
            ctx->write("enabled.");
        else
            ctx->write("disabled.");

        ctx->flush();
        CMD_VOTABLE(data, false);
        return false;
    }

    if (lobby->getCurrentState() != ServerLobby::WAITING_FOR_START_GAME)
    {
        ctx->write("Game is currently active.");
        ctx->flush();
        return false;
    }

    CMD_VOTABLE(data, true);
    CMD_SELFVOTE_PERMLOWER_CROWN(stk_ctx, data, m_min_veto, parser);
    lobby->setAutokickEnabled(state);

    return true;
}
