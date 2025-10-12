#include "modes/soccer_roulette.hpp"
#include "utils/log.hpp"
#include "utils/string_utils.hpp"
#include "utils/time.hpp"
#include <algorithm>
#include "items/powerup_manager.hpp"
#include "items/powerup.hpp"
#include "modes/world.hpp"
#include "modes/soccer_world.hpp"
#include "race/race_manager.hpp"
#include "karts/abstract_kart.hpp"
#include "network/network_string.hpp"
#include "network/stk_host.hpp"
#include "network/protocols/lobby_protocol.hpp"
#include "io/file_manager.hpp"
#include "io/xml_node.hpp"
#include "network/network_player_profile.hpp"
#include "network/stk_peer.hpp"
#include "network/server_config.hpp"
#include "network/protocols/lobby_protocol.hpp"
#include "network/database/sqlite_database.hpp"
#include "tracks/track.hpp"
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "karts/controller/controller.hpp"
#include "network/protocols/server_lobby.hpp"
#include "lobby/player_queue.hpp"
#include <set>
#ifdef ENABLE_SQLITE3
#include "network/database/abstract_database.hpp"
#endif

SoccerRoulette* SoccerRoulette::m_soccer_roulette = NULL;

// Static maps for tracking player stats
static std::map<std::string, int> s_swatter_hits_by_player;
static std::map<std::string, int> s_cake_hits_by_player;
static std::map<std::string, int> s_bowling_hits_by_player;
static std::map<std::string, int> s_bowling_used_by_player;
static std::map<std::string, int> s_bowling_puck_hits_by_player;
static std::map<std::string, int> s_bowling_player_hits_by_player;

// -----------------------------------------------------------------------------
void SoccerRoulette::create()
{
    if (m_soccer_roulette == NULL)
    {
        m_soccer_roulette = new SoccerRoulette();
    }
}
// -----------------------------------------------------------------------------
void SoccerRoulette::destroy()
{
    if (m_soccer_roulette != NULL)
    {
        delete m_soccer_roulette;
        m_soccer_roulette = NULL;
    }
}
// -----------------------------------------------------------------------------
SoccerRoulette* SoccerRoulette::get()
{
    if (m_soccer_roulette == NULL)
    {
        create();
    }
    return m_soccer_roulette;
}
// -----------------------------------------------------------------------------
SoccerRoulette::SoccerRoulette()
{
    m_current_field_index = 0;
    m_group_members.clear();
    m_group_to_color.clear();
    // Only load if soccer roulette is enabled
    if (ServerConfig::m_soccer_roulette)
    {
        loadFieldsFromConfig();
        loadTeamsFromXML();
    }
}
// -----------------------------------------------------------------------------
SoccerRoulette::~SoccerRoulette()
{
}
// -----------------------------------------------------------------------------
bool SoccerRoulette::isEnabled() const
{
    return ServerConfig::m_soccer_roulette;
}
// -----------------------------------------------------------------------------
void SoccerRoulette::loadFieldsFromConfig()
{
    if (!ServerConfig::m_soccer_roulette)
        return;
        
    m_fields.clear();   
    std::string fields_str = ServerConfig::m_soccer_roulette_fields;
    std::vector<std::string> fields = StringUtils::split(fields_str, ',');
    for (const std::string& field : fields)
    {
        std::string trimmed = StringUtils::removeWhitespaces(field);
        if (!trimmed.empty())
        {
            m_fields.push_back(trimmed);
        }
    }
    // if there are no fields specified, use those default fields
    if (m_fields.empty())
    {
        m_fields = {"icy_soccer_field", "soccer_field", "addon_green-field", "addon_huge"};
    }
    
    Log::info("SoccerRoulette", "Loaded %d fields from configuration", m_fields.size());
}

// -----------------------------------------------------------------------------
void SoccerRoulette::loadTeamsFromXML()
{
    if (!ServerConfig::m_soccer_roulette)
        return;
        
    m_player_teams.clear();
    m_group_members.clear();
    if (ServerConfig::m_teams_xml_path.c_str()[0] == '\0')
    {
        Log::info("SoccerRoulette", "No XML path specified");
        return;
    }
    try
    {
        // check if the file exists
        if (!file_manager->fileExists(ServerConfig::m_teams_xml_path))
        {
            Log::warn("SoccerRoulette", "Teams XML file %s does not exist.",
                      ServerConfig::m_teams_xml_path.c_str());
            return;
        }
        // load the file
        XMLNode* root = file_manager->createXMLTree(ServerConfig::m_teams_xml_path);
        if (!root || root->getName() != "teams")
        {
            Log::error("SoccerRoulette", "Invalid teams XML file format.");
            if (root) delete root;
            return;
        }
        // process teams
        for (unsigned int i = 0; i < root->getNumNodes(); i++)
        {
            const XMLNode* team_node = root->getNode(i);
            if (team_node->getName() != "team")
                continue;
            std::string team_name;
            team_node->get("name", &team_name);
            for (unsigned int j = 0; j < team_node->getNumNodes(); j++)
            {
                const XMLNode* player_node = team_node->getNode(j);
                if (player_node->getName() != "player")
                    continue;
                std::string player;
                player_node->get("name", &player);
                // store teams
                m_player_teams[player] = team_name;
                // store group membership
                m_group_members[team_name].push_back(player);
                // log the teams
                Log::info("SoccerRoulette", "Team assignment: %s -> %s",
                          player.c_str(), team_name.c_str());
            }
        }	
        delete root;
        Log::info("SoccerRoulette", "Teams loaded from %s",
                  ServerConfig::m_teams_xml_path.c_str());
    }
    catch (const std::exception& e)
    {
        Log::error("SoccerRoulette", "Error loading teams: %s", e.what());
    }
}
// -----------------------------------------------------------------------------
void SoccerRoulette::reload()
{
    if (!ServerConfig::m_soccer_roulette)
        return;
        
    m_current_field_index = 0;
    loadFieldsFromConfig();
    loadTeamsFromXML();
    
    auto sl = LobbyProtocol::get<ServerLobby>();
    if (sl)
    {
        reassignTeams(nullptr);
    }
}

// -----------------------------------------------------------------------------
void SoccerRoulette::resetIndex()
{
    m_current_field_index = 0;
    Log::info("SoccerRoulette", "Field index reset to 0");
}

// -----------------------------------------------------------------------------
std::string SoccerRoulette::getNextField()
{
    if (m_fields.empty())
    {
        return ""; // if no fields specified
    }
    
    std::string current_field = m_fields[m_current_field_index];
    m_current_field_index = (m_current_field_index + 1) % m_fields.size();
    
    Log::info("SoccerRoulette", "Next field: %s (index %d/%d)",
               current_field.c_str(), m_current_field_index, m_fields.size());
    
    return current_field;
}

// -----------------------------------------------------------------------------
std::string SoccerRoulette::getCurrentField() const
{
    if (m_fields.empty())
    {
        return "icy_soccer_field"; // icy_soccer_field is default track
    }
    
    return m_fields[m_current_field_index];
}

// -----------------------------------------------------------------------------
// add field
void SoccerRoulette::addField(const std::string& field)
{
    if (std::find(m_fields.begin(), m_fields.end(), field) == m_fields.end())
    {
        m_fields.push_back(field);
        // update config
        std::string fields_str;
        for (size_t i = 0; i < m_fields.size(); i++)
        {
            fields_str += m_fields[i];
            if (i < m_fields.size() - 1)
                fields_str += ",";
        }
        ServerConfig::m_soccer_roulette_fields = fields_str;
        Log::info("SoccerRoulette", "Added field %s to roulette", field.c_str());
    }
}

// -----------------------------------------------------------------------------
// remove field
void SoccerRoulette::removeField(const std::string& field)
{
    auto it = std::find(m_fields.begin(), m_fields.end(), field);
    if (it != m_fields.end())
    {
        m_fields.erase(it);
        // update config
        std::string fields_str;
        for (size_t i = 0; i < m_fields.size(); i++)
        {
            fields_str += m_fields[i];
            if (i < m_fields.size() - 1)
                fields_str += ",";
        }
        ServerConfig::m_soccer_roulette_fields = fields_str;
        // reset index
        if (m_current_field_index >= (int)m_fields.size())
        {
            m_current_field_index = 0;
        }
        
        Log::info("SoccerRoulette", "Removed field %s from roulette", field.c_str());
    }
}

// -----------------------------------------------------------------------------
// get team for player
std::string SoccerRoulette::getTeamForPlayer(const std::string& player_name) const
{
    auto it = m_player_teams.find(player_name);
    if (it != m_player_teams.end())
    {
        return it->second;
    }
    return "";
}

// -----------------------------------------------------------------------------
// assigns a team to a player
void SoccerRoulette::assignTeamToPlayer(const std::string& player_name, const std::string& team)
{
    m_player_teams[player_name] = team;
    Log::info("SoccerRoulette", "Assigned player %s to team %s", 
              player_name.c_str(), team.c_str());
}

// -----------------------------------------------------------------------------
// assigns a team to a player profile
void SoccerRoulette::assignTeamToPlayer(NetworkPlayerProfile* profile)
{
    if (!profile)
        return;
    
    std::string player_name = StringUtils::wideToUtf8(profile->getName());
    std::string team = getTeamForPlayer(player_name);
    
    if (team.empty())
    {
        // No team found, assign to KART_TEAM_NONE (spectator)
        profile->setTeam(KART_TEAM_NONE);
        Log::info("SoccerRoulette", "Player %s not found in teams XML, assigned as spectator",
                  player_name.c_str());
        return;
    }
    
    // If the team is a group name, check if there is a mapped color
    auto gtc = m_group_to_color.find(team);
    if (gtc != m_group_to_color.end())
    {
        team = gtc->second;
    }
    
    // assign a team when someone joins
    // team blue
    if (team == "blue")
    {
        profile->setTeam(KART_TEAM_BLUE);
        Log::info("SoccerRoulette", "Assigned %s to blue team", player_name.c_str());
    }
    // team red
    else if (team == "red")
    {
        profile->setTeam(KART_TEAM_RED);
        Log::info("SoccerRoulette", "Assigned %s to red team", player_name.c_str());
    }
    // if forced as a spectator
    else if (team == "spectator")
    {
        // Set the player as spectator
        profile->setTeam(KART_TEAM_NONE);
        Log::info("SoccerRoulette", "Assigned %s as spectator", player_name.c_str());
    }
    // Or else, KART_TEAM_NONE
    else
    {
        // Unknown team, assign as spectator
        profile->setTeam(KART_TEAM_NONE);
        Log::warn("SoccerRoulette", "Unknown team %s for player %s, assigned as spectator",
                  team.c_str(), player_name.c_str());
    }
}

// -----------------------------------------------------------------------------
// get players and teams from the xml file
std::string SoccerRoulette::getTeamsInfo() const
{
    std::string result = "Soccer Roulette Teams:\n";
    // group players
    std::map<std::string, std::vector<std::string>> teams_players;
    for (const auto& pair : m_player_teams)
    {
        teams_players[pair.second].push_back(pair.first);
    }
    for (const auto& team_pair : teams_players)
    {
        result += "Team " + team_pair.first + ": ";
        for (size_t i = 0; i < team_pair.second.size(); i++)
        {
            result += team_pair.second[i];
            if (i < team_pair.second.size() - 1)
                result += ", ";
        }
        result += "\n";
    }
    if (teams_players.empty())
    {
        result += "No teams defined in XML file.\n";
    }
    return result;
}

// -----------------------------------------------------------------------------
// This will give 1 nitro bottle.
// This is used at the start of each game.
void SoccerRoulette::giveNitroToAll()
{
    if (!World::getWorld())
        return;
    Log::info("SoccerRoulette", "Giving nitro to all players at game start");
   
    for (unsigned int i = 0; i < RaceManager::get()->getNumPlayers(); i++)
    {
        AbstractKart* kart = World::getWorld()->getKart(i);
        if (!kart || kart->isEliminated())
            continue;
        
        // give all the players a boost bottle 
        kart->setEnergy(3.0f);
    }
}

// -----------------------------------------------------------------------------
bool SoccerRoulette::checkRequiredAddons(STKPeer* peer, std::string& error_msg)
{
    if (!peer)
    {
        error_msg = "Invalid peer";
        return false;
    }
    // get client assets
    const auto& client_assets = peer->getClientAssets().second;
    std::vector<std::string> missing_required;
    for (const std::string& field : m_fields)
    {
        if (client_assets.find(field) == client_assets.end())
        {
            missing_required.push_back(field);
        }
    }
    if (!missing_required.empty())
    {
        std::string missing_fields_str;
        for (size_t i = 0; i < missing_required.size(); i++)
        {
            missing_fields_str += missing_required[i];
            if (i < missing_required.size() - 1)
                missing_fields_str += ", ";
        }
       
        error_msg = StringUtils::insertValues(
            "You need to install the following required soccer fields: %s",
            missing_fields_str.c_str());
        return false;
    }
    
    return true;
}

// ---------------------------------------------------------------------------------
void SoccerRoulette::resetFieldIndex()
{
	m_current_field_index = 0;
	Log::info("SoccerRoulette", "Field index resetted to 0");
}
// --------------------------------------------------------------------------------
void SoccerRoulette::calculateGameResult()
{
#ifdef ENABLE_SQLITE3
    try
    {
        auto sl = LobbyProtocol::get<ServerLobby>();
        if (!sl)
        {
            Log::error("SoccerRoulette", "ServerLobby not available");
            return;
        }
        auto db = sl->getDatabase();
        if (!db || !db->hasDatabase())
        {
            Log::error("SoccerRoulette", "Database not available");
            return;
        }
        
        const auto& goal_history = GoalHistory::getGoalHistory();
        if (goal_history.empty())
        {
            Log::info("SoccerRoulette", "No goals to process");
            return;
        }
        
        int red_score = 0;
        int blue_score = 0;
        float fastest_speed = 0;
        std::string fastest_player;
        std::string fastest_team;
        std::map<std::string, int> red_players_points;
        std::map<std::string, int> blue_players_points;
        std::map<std::string, int> red_players_goals;
        std::map<std::string, int> blue_players_goals;
        std::map<std::string, float> red_players_total_speed;
        std::map<std::string, float> blue_players_total_speed;
        std::map<std::string, float> red_players_fastest_speed;
        std::map<std::string, float> blue_players_fastest_speed;
        
        for (const auto& goal : goal_history)
        {
            std::string team_name = (goal.team == 0) ? "Red" : "Blue";
            std::string player_name = goal.player_name;
            float speed = goal.speed;
            
            if (goal.team == 0)
            {
                red_score++;
                red_players_points[player_name] += 1;
                red_players_goals[player_name]++;
                red_players_total_speed[player_name] += speed;
                if (red_players_fastest_speed[player_name] < speed)
                {
                    red_players_fastest_speed[player_name] = speed;
                }
            }
            else
            {
                blue_score++;
                blue_players_points[player_name] += 1;
                blue_players_goals[player_name]++;
                blue_players_total_speed[player_name] += speed;
                if (blue_players_fastest_speed[player_name] < speed)
                {
                    blue_players_fastest_speed[player_name] = speed;
                }
            }
            
            if (speed > fastest_speed)
            {
                fastest_speed = speed;
                fastest_player = player_name;
                fastest_team = team_name;
            }
        }
        
        int red_total_points = 0;
        int blue_total_points = 0;
        
        if (red_score > blue_score)
        {
            red_total_points += 10;
        }
        else if (blue_score > red_score)
        {
            blue_total_points += 10;
        }
        
        red_total_points += red_score;
        blue_total_points += blue_score;
        
        if (!fastest_team.empty())
        {
            if (fastest_team == "Red")
            {
                red_players_points[fastest_player] += 3;
                red_total_points += 3;
            }
            else if (fastest_team == "Blue")
            {
                blue_players_points[fastest_player] += 3;
                blue_total_points += 3;
            }
        }
        
        std::string red_team_name = getGroupForColor("red");
        std::string blue_team_name = getGroupForColor("blue");
        if (red_team_name.empty()) red_team_name = "red";
        if (blue_team_name.empty()) blue_team_name = "blue";
        
        std::string team_vs = red_team_name + " vs " + blue_team_name;
        
        uint64_t timestamp_ms = StkTime::getMonoTimeMs();
        std::string track_id = Track::getCurrentTrack() ? Track::getCurrentTrack()->getIdent() : "";
        
        int game_id = db->writeSoccerRouletteGameResult(
            timestamp_ms, track_id,
            red_score, blue_score, red_total_points, blue_total_points,
            red_team_name, blue_team_name, team_vs,
            fastest_player, fastest_team, fastest_speed, 0.0f
        );
        
        if (game_id <= 0)
        {
            Log::error("SoccerRoulette", "Failed to write game result to database");
            return;
        }
        
        m_last_game_id = game_id;
        
        for (const auto& goal : goal_history)
        {
            std::string team_name = (goal.team == 0) ? "Red" : "Blue";
            db->writeSoccerRouletteGoalDetail(
                game_id, goal.player_name, team_name,
                goal.speed, 0.0f, timestamp_ms
            );
        }
        
        for (const auto& pair : red_players_points)
        {
            const std::string& player_name = pair.first;
            int goals_scored = red_players_goals[player_name];
            float total_speed = red_players_total_speed[player_name];
            float fastest_speed = red_players_fastest_speed[player_name];
            int points_from_goals = goals_scored;
            int points_from_fastest = (player_name == fastest_player && fastest_team == "Red") ? 3 : 0;
            
            db->writeSoccerRoulettePlayerPerformance(
                game_id, player_name, 0, "red",
                goals_scored, total_speed, fastest_speed, 
                points_from_goals, points_from_fastest, pair.second, team_vs
            );
        }
        
        for (const auto& pair : blue_players_points)
        {
            const std::string& player_name = pair.first;
            int goals_scored = blue_players_goals[player_name];
            float total_speed = blue_players_total_speed[player_name];
            float fastest_speed = blue_players_fastest_speed[player_name];
            int points_from_goals = goals_scored;
            int points_from_fastest = (player_name == fastest_player && fastest_team == "Blue") ? 3 : 0;
            
            db->writeSoccerRoulettePlayerPerformance(
                game_id, player_name, 0, "blue",
                goals_scored, total_speed, fastest_speed,
                points_from_goals, points_from_fastest, pair.second, team_vs
            );
        }
        
        Log::info("SoccerRoulette", "Game results calculated and saved to database: %s vs %s (%d-%d)", 
                 red_team_name.c_str(), blue_team_name.c_str(), red_score, blue_score);
    }
    catch (const std::exception& e)
    {
        Log::error("SoccerRoulette", "Exception while calculating game results: %s", e.what());
    }
#else
    Log::error("SoccerRoulette", "SQLite not enabled, cannot save to database");
#endif
}
void SoccerRoulette::kickPlayer(const std::string& player_name, STKCommandContext* const commander)
{
    auto sl = LobbyProtocol::get<ServerLobby>();
    if (!sl)
    {
        Log::error("SoccerRoulette", "ServerLobby not available");
        return;
    }
    std::shared_ptr<STKPeer> player_peer = STKHost::get()->findPeerByName(
        StringUtils::utf8ToWide(player_name), true/*ignoreCase*/, true/*prefixOnly*/);

    if (player_name.empty() || !player_peer || player_peer->isAIPeer())
    {
        commander->write("Player '" + player_name + "' not found. Usage: /sr kick <player name>");
        commander->flush();
        return;
    }
    std::string kicker_name = commander->getProfileName();
    Log::info("SoccerRoulette", "Player %s kicked %s.",
              kicker_name.c_str(), player_name.c_str());
    std::string kick_msg = kicker_name + " kicked " + player_name;
    sl->sendStringToAllPeers(kick_msg);
    player_peer->kick();
    commander->nprintf("You kicked player '%s'", 512,
            player_name.c_str());
    commander->flush();
}
// -----------------------------------------------------------------------------
// Reassigns teams
void SoccerRoulette::reassignTeams(STKCommandContext* const commander)
{
    auto sl = LobbyProtocol::get<ServerLobby>();
    if (!sl)
    {
        Log::error("SoccerRoulette", "ServerLobby not available for team reassignment");
        return;
    }
    auto peers = STKHost::get()->getPeers();
    int reassigned_count = 0;
    for (auto& peer : peers)
    {
        if (!peer->isValidated() || peer->isAIPeer())
            continue;   
        for (auto& profile : peer->getPlayerProfiles())
        {
            std::string player_name = StringUtils::wideToUtf8(profile->getName());
            KartTeam previous_team = profile->getTeam();
            assignTeamToPlayer(profile.get());
            // check if team changed
            if (previous_team != profile->getTeam())
            {
                reassigned_count++;
                std::string team_str;
                switch (profile->getTeam())
                {
                    case KART_TEAM_RED:
                        team_str = "red";
                        break;
                    case KART_TEAM_BLUE:
                        team_str = "blue";
                        break;
                    default:
                        team_str = "spectator";
                        break;
                }
                Log::info("SoccerRoulette", "Reassigned player %s to team %s", 
                          player_name.c_str(), team_str.c_str());
            }
        }

        // Re-test eligibility and update queue after potential team changes
        const PeerEligibility old_el = peer->getEligibility();
        peer->testEligibility();
        LobbyPlayerQueue::get()->onPeerEligibilityChange(peer, old_el);
    }
    sl->updatePlayerList();
    std::string confirm_msg = "Reassigned teams";
	sl->sendStringToAllPeers(confirm_msg);
}
// -----------------------------------------------------------------------------
// Sets the game start timeout (not used for now)
void SoccerRoulette::setRouletteTimeout(STKCommandContext* const commander)
{
    auto sl = LobbyProtocol::get<ServerLobby>();
    if (!sl)
    {
        return;
    }
    // 5min
    const int TIME_OUT = 300;
    sl->changeTimeout(TIME_OUT, false, true);
    std::string confirm_msg = "Game start timeout set to 5 minutes";
    sl->sendStringToAllPeers(confirm_msg);
    Log::info("SoccerRoulette", "Game start timeout set to 5 minutes");
}

// -----------------------------------------------------------------------------
bool SoccerRoulette::isPlayerInTeam(const std::string& player_name)
{
    if (!ServerConfig::m_soccer_roulette)
        return false;
        
    auto it = m_player_teams.find(player_name);
    if (it == m_player_teams.end())
        return false;
        
    // Check if player is in red or blue team (not spectator)
    const std::string& t = it->second;
    if (t == "red" || t == "blue") return true;
    auto gtc = m_group_to_color.find(t);
    return gtc != m_group_to_color.end() && (gtc->second == "red" || gtc->second == "blue");
}

// -----------------------------------------------------------------------------
void SoccerRoulette::setBallTrackingData(float red_time, float blue_time)
{
    m_last_red_side_time = std::max(0.0f, red_time);
    m_last_blue_side_time = std::max(0.0f, blue_time);
}

// -----------------------------------------------------------------------------
void SoccerRoulette::clearBallTrackingData()
{
    m_last_red_side_time = 0.0f;
    m_last_blue_side_time = 0.0f;
}

// -----------------------------------------------------------------------------
std::string SoccerRoulette::getBallPositionPercentages() const
{
    const float total_time = m_last_red_side_time + m_last_blue_side_time;
    if (total_time <= 0.0f)
        return ""; // nothing tracked
    const float red_percentage = (m_last_red_side_time / total_time) * 100.0f;
    const float blue_percentage = (m_last_blue_side_time / total_time) * 100.0f;
    std::ostringstream ss;
    ss.setf(std::ios::fixed);
    ss.precision(0);
    ss << "Red: " << red_percentage << "% Blue: " << blue_percentage << "%";
    return ss.str();
}

// -----------------------------------------------------------------------------
std::string SoccerRoulette::getFixedMixedBar() const
{
    // Return "🟥🟥🟥🟥🟦🟦🟦🟦"
    std::string red4, blue4;
    for (int i = 0; i < 4; i++) red4  += "\xF0\x9F\x9F\xA5"; // 🟥
    for (int i = 0; i < 4; i++) blue4 += "\xF0\x9F\x9F\xA6"; // 🟦
    return red4 + blue4;
}
// -----------------------------------------------------------------------------
void SoccerRoulette::writeBallSideStatsToDatabase()
{
#ifdef ENABLE_SQLITE3
    if (!ServerConfig::m_sql_management)
        return;
    auto sl = LobbyProtocol::get<ServerLobby>();
    if (!sl)
        return;
    auto db = sl->getDatabase();
    if (!db || !db->hasDatabase())
        return;

    const float red_s = m_last_red_side_time;
    const float blue_s = m_last_blue_side_time;
    if (red_s <= 0.0f && blue_s <= 0.0f)
        return;

    const float total = red_s + blue_s;
    const float red_pct = total > 0.0f ? (red_s / total) * 100.0f : 0.0f;
    const float blue_pct = total > 0.0f ? (blue_s / total) * 100.0f : 0.0f;

    const std::string track_id = Track::getCurrentTrack() ? Track::getCurrentTrack()->getIdent() : std::string("");
    const uint64_t ts = StkTime::getMonoTimeMs();

    std::string team_vs;
    if (SoccerRoulette::get())
    {
        std::string red_group = SoccerRoulette::get()->getGroupForColor("red");
        std::string blue_group = SoccerRoulette::get()->getGroupForColor("blue");
        if (!red_group.empty() && !blue_group.empty())
            team_vs = red_group + " vs " + blue_group;
        else
            team_vs = "Red vs Blue";
    }
    else
        team_vs = "Red vs Blue";

    db->writeBallSideStats(m_last_game_id, ts, track_id, red_s, blue_s, red_pct, blue_pct, team_vs);
#endif
}

// -----------------------------------------------------------------------------
bool SoccerRoulette::setGroupColor(const std::string& group, const std::string& color)
{
    if (group.empty()) return false;
    if (!(color == "red" || color == "blue" || color == "spectator"))
        return false;
    m_group_to_color[group] = color;
    Log::info("SoccerRoulette", "Group %s set to color %s", group.c_str(), color.c_str());
    return true;
}

// -----------------------------------------------------------------------------
std::string SoccerRoulette::getGroupColor(const std::string& group) const
{
    auto it = m_group_to_color.find(group);
    if (it == m_group_to_color.end()) return "";
    return it->second;
}

// -----------------------------------------------------------------------------
std::vector<std::string> SoccerRoulette::listGroups() const
{
    std::vector<std::string> res;
    res.reserve(m_group_members.size());
    for (const auto& kv : m_group_members)
        res.push_back(kv.first);
    return res;
}

// -----------------------------------------------------------------------------
std::string SoccerRoulette::getGroupForColor(const std::string& color) const
{
    // Find the first group mapped to this color
    for (const auto& kv : m_group_to_color)
    {
        if (kv.second == color)
            return kv.first;
    }
    return std::string();
}

// -----------------------------------------------------------------------------
void SoccerRoulette::recordSwatterHit(const std::string& player_name)
{
    s_swatter_hits_by_player[player_name]++;
}

// -----------------------------------------------------------------------------
void SoccerRoulette::recordCakeHit(const std::string& player_name)
{
    s_cake_hits_by_player[player_name]++;
}

// -----------------------------------------------------------------------------
void SoccerRoulette::recordBowlingHit(const std::string& player_name)
{
    s_bowling_hits_by_player[player_name]++;
}

// -----------------------------------------------------------------------------
void SoccerRoulette::recordBowlingUsed(const std::string& player_name)
{
    s_bowling_used_by_player[player_name]++;
}

// -----------------------------------------------------------------------------
void SoccerRoulette::recordBowlingPuckHit(const std::string& player_name)
{
    s_bowling_puck_hits_by_player[player_name]++;
}

// -----------------------------------------------------------------------------
void SoccerRoulette::recordBowlingPlayerHit(const std::string& player_name)
{
    s_bowling_player_hits_by_player[player_name]++;
}

// -----------------------------------------------------------------------------
int SoccerRoulette::getSwatterHits(const std::string& player_name)
{
    auto it = s_swatter_hits_by_player.find(player_name);
    return (it != s_swatter_hits_by_player.end()) ? it->second : 0;
}

// -----------------------------------------------------------------------------
int SoccerRoulette::getCakeHits(const std::string& player_name)
{
    auto it = s_cake_hits_by_player.find(player_name);
    return (it != s_cake_hits_by_player.end()) ? it->second : 0;
}

// -----------------------------------------------------------------------------
int SoccerRoulette::getBowlingHits(const std::string& player_name)
{
    auto it = s_bowling_hits_by_player.find(player_name);
    return (it != s_bowling_hits_by_player.end()) ? it->second : 0;
}

// -----------------------------------------------------------------------------
int SoccerRoulette::getBowlingUsed(const std::string& player_name)
{
    auto it = s_bowling_used_by_player.find(player_name);
    return (it != s_bowling_used_by_player.end()) ? it->second : 0;
}

// -----------------------------------------------------------------------------
int SoccerRoulette::getBowlingPuckHits(const std::string& player_name)
{
    auto it = s_bowling_puck_hits_by_player.find(player_name);
    return (it != s_bowling_puck_hits_by_player.end()) ? it->second : 0;
}

// -----------------------------------------------------------------------------
int SoccerRoulette::getBowlingPlayerHits(const std::string& player_name)
{
    auto it = s_bowling_player_hits_by_player.find(player_name);
    return (it != s_bowling_player_hits_by_player.end()) ? it->second : 0;
}

// -----------------------------------------------------------------------------
void SoccerRoulette::resetPlayerStats()
{
    s_swatter_hits_by_player.clear();
    s_cake_hits_by_player.clear();
    s_bowling_hits_by_player.clear();
    s_bowling_used_by_player.clear();
    s_bowling_puck_hits_by_player.clear();
    s_bowling_player_hits_by_player.clear();
}

// -----------------------------------------------------------------------------
void SoccerRoulette::writeStatsToDatabase(int game_id)
{
#ifdef ENABLE_SQLITE3
    if (!ServerConfig::m_sql_management)
        return;
        
    // Get database instance
    auto server_lobby = LobbyProtocol::get<ServerLobby>();
    if (!server_lobby)
        return;
    auto db = server_lobby->getDatabase();
    if (!db || !db->hasDatabase())
        return;
    
    
    // Generate team vs team string
    std::string team_vs = "";
    if (SoccerRoulette::get())
    {
        // Find which groups are mapped to red and blue
        std::string red_group = SoccerRoulette::get()->getGroupForColor("red");
        std::string blue_group = SoccerRoulette::get()->getGroupForColor("blue");
        
        if (!red_group.empty() && !blue_group.empty())
            team_vs = red_group + " vs " + blue_group;
        else
            team_vs = "Red vs Blue";
    }
    else
    {
        team_vs = "Red vs Blue";
    }
    
    // Collect all unique player names from all maps
    std::set<std::string> all_players;
    for (const auto& kv : s_swatter_hits_by_player)
        all_players.insert(kv.first);
    for (const auto& kv : s_cake_hits_by_player)
        all_players.insert(kv.first);
    for (const auto& kv : s_bowling_hits_by_player)
        all_players.insert(kv.first);
    for (const auto& kv : s_bowling_used_by_player)
        all_players.insert(kv.first);
    for (const auto& kv : s_bowling_puck_hits_by_player)
        all_players.insert(kv.first);
    for (const auto& kv : s_bowling_player_hits_by_player)
        all_players.insert(kv.first);
    
    // Write stats for each player
    for (const std::string& player_name : all_players)
    {
        int swatter_hits = getSwatterHits(player_name);
        int cake_hits = getCakeHits(player_name);
        int bowling_hits = getBowlingHits(player_name);
        int bowling_used = getBowlingUsed(player_name);
        int bowling_puck = getBowlingPuckHits(player_name);
        int bowling_players = getBowlingPlayerHits(player_name);
        
        // Get online_id by looking through connected peers
        uint32_t online_id = 0;
        auto peers = STKHost::get()->getPeers();
        for (auto& peer : peers)
        {
            auto profiles = peer->getPlayerProfiles();
            for (auto& profile : profiles)
            {
                std::string profile_name = core::stringc(profile->getName()).c_str();
                if (profile_name == player_name)
                {
                    online_id = profile->getOnlineId();
                    break;
                }
            }
            if (online_id != 0) break;
        }
        
        // Write to database
        db->writeGameStats(game_id, player_name, online_id, "", "",
                          swatter_hits, cake_hits, bowling_hits, 
                          bowling_used, bowling_puck, bowling_players, team_vs, "");
    }
#endif
}
