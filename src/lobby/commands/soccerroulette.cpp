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

#include "network/server_config.hpp"
#include "utils/log.hpp"
#include "soccerroulette.hpp"
#include "lobby/server_lobby_commands.hpp"
#include "lobby/stk_command.hpp"
#include "lobby/stk_command_context.hpp"
#include "modes/soccer_roulette.hpp"
#include "network/protocols/server_lobby.hpp"
#include "lobby/player_queue.hpp"
#include "network/stk_host.hpp"
#include "network/database/abstract_database.hpp"
#include <parser/argline_parser.hpp>
#include <string>

bool SoccerRouletteCommand::execute(nnwcli::CommandExecutorContext* const ctx, void* const data)
{
    STK_CTX(stk_ctx, ctx);

    auto parser = ctx->get_parser();

    std::string subcmd;
    std::string track_id;

    *parser >> subcmd; // Subcommand support will be added in future versions of nnwcli
    parser->parse_string(track_id, false);

    parser->parse_finish(); // Do not allow more arguments

    ServerLobby* const lobby = stk_ctx->get_lobby();

    if (!ServerConfig::m_soccer_roulette)
    {
        ctx->write("These soccerroulette commands are only possible in special roulette servers");
        ctx->flush();
        return false;
    }

    CMD_REQUIRE_PERM(stk_ctx, m_required_perm);

    if (subcmd == "status")
    {
        bool is_enabled = ServerConfig::m_soccer_roulette;
        std::string current = SoccerRoulette::get()->getCurrentField();
        ctx->write("Soccer Roulette is ");
        if (is_enabled)
        {
            ctx->write("enabled. Current field: ");
            ctx->write(current);
        }
        else
            ctx->write("disabled.");
        ctx->flush();
    }
    else if (subcmd == "add" && !track_id.empty())
    {
        SoccerRoulette::get()->addField(track_id);
        ctx->nprintf("Added field %s to Soccer Roulette", 512,
                track_id.c_str());
        ctx->flush();
    }
    else if (subcmd == "remove" && !track_id.empty())
    {
        SoccerRoulette::get()->removeField(track_id);
        ctx->nprintf("Removed field %s from Soccer Roulette", 512,
                track_id.c_str());
        ctx->flush();
    }
    else if (subcmd == "list")
    {
        const std::vector<std::string>& fields = SoccerRoulette::get()->getFields();
        ctx->write("Soccer Roulette fields: ");
        for (size_t i = 0; i < fields.size(); i++)
        {
            ctx->write(fields[i]);
            if (i < fields.size() - 1)
                ctx->write(", ");
        }
        ctx->flush();
    }
    else if (subcmd == "reload")
    {
        SoccerRoulette::get()->reload();
        ctx->write("Soccer Roulette fields and teams reloaded from configuration");
        ctx->flush();
    }
    else if (subcmd == "teams")
    {
        ctx->write(SoccerRoulette::get()->getTeamsInfo());
        ctx->flush();
    }
    else if (subcmd == "start")
    {
        std::string next_field = SoccerRoulette::get()->getNextField();
        bool success = lobby->setForcedTrack(next_field, 10, false, true, true);
        std::string msg;
        if (success)
        {
            msg = "Soccer Roulette started with field: " + next_field;
            SoccerRoulette::get()->setActive(true);
            // Recalculate eligibility so players not in teams.xml get a cross immediately
            auto peers = STKHost::get()->getPeers();
            for (auto& peer : peers)
            {
                if (!peer->isValidated() || peer->isAIPeer())
                    continue;
                const PeerEligibility old_el = peer->getEligibility();
                peer->testEligibility();
                LobbyPlayerQueue::get()->onPeerEligibilityChange(peer, old_el);
            }
            lobby->updatePlayerList();
        }
        else
        {
            msg = "Failed to set field: " + next_field;
        }
        lobby->sendStringToAllPeers(msg);
        lobby->setPoleEnabled(true);
    }
    else if (subcmd == "reset")
    {
        SoccerRoulette::get()->resetFieldIndex();
        SoccerRoulette::get()->setActive(false);
        std::string current_field = SoccerRoulette::get()->getCurrentField();
        ctx->write("Soccer Roulette field index reset. Next field will be: ");
        ctx->write(current_field);
        ctx->flush();
    }
    else if (subcmd == "golden-goal")
    {
        SoccerRoulette::get()->activateGoldenGoal();
        std::string who = stk_ctx->getProfileName();
        Log::warn("SoccerRoulette", "Golden goal activated by %s", who.c_str());
        if (stk_ctx->get_lobby() && stk_ctx->get_lobby()->getCurrentState() == ServerLobby::RACING)
        {
            stk_ctx->get_lobby()->sendStringToAllPeers("Golden goal activated. Next goal ends the match.");
        }
        else
        {
            ctx->write("Golden goal activated. Next goal of the upcoming match will end it instantly.");
            ctx->flush();
        }
    }
    else if (subcmd == "assign" && !track_id.empty())
    {
        // Expected format: <group>=<color>
        auto pos = track_id.find('=');
        if (pos == std::string::npos)
        {
            ctx->write("Usage: /soccerroulette assign <group>=<red|blue|spectator>");
            ctx->flush();
            return false;
        }
        std::string group = track_id.substr(0, pos);
        std::string color = track_id.substr(pos + 1);
        if (!SoccerRoulette::get()->setGroupColor(group, color))
        {
            ctx->write("Invalid color. Allowed: red, blue, spectator");
            ctx->flush();
            return false;
        }
        // Reassign teams based on the new mapping
        SoccerRoulette::get()->reassignTeams(stk_ctx);
        ctx->nprintf("Assigned group %s to %s", 256, group.c_str(), color.c_str());
        ctx->flush();
    }
    else if (subcmd == "groups")
    {
        const auto groups = SoccerRoulette::get()->listGroups();
        if (groups.empty())
        {
            ctx->write("No groups found in teams XML.");
            ctx->flush();
        }
        else
        {
            ctx->write("Groups (group=color): ");
            for (size_t i = 0; i < groups.size(); i++)
            {
                const std::string& g = groups[i];
                std::string c = SoccerRoulette::get()->getGroupColor(g);
                if (c.empty()) c = "(unset)";
                ctx->nprintf("%s=%s%s", 1024, g.c_str(), c.c_str(), (i + 1 < groups.size()) ? ", " : "");
            }
            ctx->flush();
        }
    }
    else if (subcmd == "stats")
    {
        std::string who = track_id;
        if (who.empty())
        {
            ctx->write("Usage: /soccerroulette stats <name>");
            ctx->flush();
            return false;
        }
        // Resolve to an exact in-game profile name (case-insensitive, prefix ok)
        std::string resolved = who;
        std::shared_ptr<STKPeer> target_peer = STKHost::get()->findPeerByName(
            core::stringw(who.c_str()), true/*ignoreCase*/, true/*prefixOnly*/);
        if (target_peer && target_peer->hasPlayerProfiles())
        {
            // If multiple profiles (splitscreen), sum them up
            int total_swatter = 0;
            int total_cake = 0;
            int total_bowling = 0;
            for (auto& prof : target_peer->getPlayerProfiles())
            {
                std::string n = core::stringc(prof->getName()).c_str();
                total_swatter += SoccerRoulette::getSwatterHits(n);
                total_cake += SoccerRoulette::getCakeHits(n);
                total_bowling += SoccerRoulette::getBowlingHits(n);
            }
            ctx->nprintf("%s stats this match: swattered %d time(s), caked %d time(s), bowled %d time(s).", 
                        256, who.c_str(), total_swatter, total_cake, total_bowling);
            ctx->flush();
            return true;
        }
        ctx->nprintf("%s stats this match: swattered %d time(s), caked %d time(s), bowled %d time(s).", 256, who.c_str(), 0, 0, 0);
        ctx->flush();
    }
    else if (subcmd == "bowling")
    {
        std::string who = track_id.empty() ? stk_ctx->getProfileName() : track_id;
        std::string resolved = who;
        std::shared_ptr<STKPeer> target_peer = STKHost::get()->findPeerByName(
            core::stringw(who.c_str()), true/*ignoreCase*/, true/*prefixOnly*/);
        if (target_peer && target_peer->hasPlayerProfiles())
        {
            // If multiple profiles (splitscreen), sum them up
            int total_used = 0;
            int total_puck = 0;
            int total_players = 0;
            for (auto& prof : target_peer->getPlayerProfiles())
            {
                std::string n = core::stringc(prof->getName()).c_str();
                total_used += SoccerRoulette::getBowlingUsed(n);
                total_puck += SoccerRoulette::getBowlingPuckHits(n);
                total_players += SoccerRoulette::getBowlingPlayerHits(n);
            }
            ctx->nprintf("%s bowling stats this match: used %d, hit puck %d, hit players %d", 
                        256, who.c_str(), total_used, total_puck, total_players);
            ctx->flush();
            return true;
        }
        ctx->nprintf("%s bowling stats this match: used %d, hit puck %d, hit players %d", 256, who.c_str(), 0, 0, 0);
        ctx->flush();
    }
    else if (subcmd == "kick" && !track_id.empty())
    {
        SoccerRoulette::get()->kickPlayer(track_id, stk_ctx);
    }
    else if (subcmd == "reassign" && track_id == "teams")
    {
        SoccerRoulette::get()->loadTeamsFromXML();
        SoccerRoulette::get()->reassignTeams(stk_ctx);
    }
    else if (subcmd == "empty" && track_id == "database")
    {
        auto db = lobby->getDatabase();
        if (db && db->hasDatabase())
        {
            db->emptySoccerRouletteDatabase();
            ctx->write("Soccer Roulette database tables have been emptied");
        }
        else
        {
            ctx->write("Database not available");
        }
        ctx->flush();
    }
    else
    {
        ctx->write("Unknown Soccer Roulette command. Format: /soccerroulette status|add <field>|remove <field>|list|reload|teams|start|reset|golden-goal|kick <player>|reassign teams|assign <group>=<color>|groups|stats [player]|bowling [player]|empty database");
        ctx->flush();
        return false;
    }
    return true;
}
