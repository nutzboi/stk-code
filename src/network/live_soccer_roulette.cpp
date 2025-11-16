//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2015 Joerg Henrichs
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

#include "network/live_soccer_roulette.hpp"
#include "modes/world.hpp"
#include "modes/soccer_world.hpp"
#include "modes/soccer_roulette.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/controller/controller.hpp"
#include "utils/log.hpp"
#include "utils/string_utils.hpp"
#include "utils/time.hpp"
#include "network/server_config.hpp"
#include "tracks/track.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <chrono>

// ------------------------------------------------------------------------------
LiveSoccerRoulette* LiveSoccerRoulette::m_instance = nullptr;

// ------------------------------------------------------------------------------
LiveSoccerRoulette::LiveSoccerRoulette()
{
    m_server_ip = ServerConfig::m_server_liveroulette_ip;
    m_server_port = std::stoi(ServerConfig::m_server_liveroulette_port.c_str());
    m_update_interval_ms = 2000;
    m_socket = -1;
    m_export_running = false;
    m_last_red_score = 0;
    m_last_blue_score = 0;
    m_is_active = false;
}

// ------------------------------------------------------------------------------
LiveSoccerRoulette::~LiveSoccerRoulette()
{
    stopExport();
}

// ------------------------------------------------------------------------------
LiveSoccerRoulette* LiveSoccerRoulette::getInstance()
{
    if (!m_instance)
    {
        m_instance = new LiveSoccerRoulette();
    }
    return m_instance;
}

// ------------------------------------------------------------------------------
void LiveSoccerRoulette::destroy()
{
    if (m_instance)
    {
        delete m_instance;
        m_instance = nullptr;
    }
}

// ------------------------------------------------------------------------------
void LiveSoccerRoulette::startExport(const std::string& server_ip, int server_port, int update_interval_ms)
{
    stopExport();
    
    m_server_ip = server_ip;
    m_server_port = server_port;
    m_update_interval_ms = update_interval_ms;
    
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0)
    {
        Log::error("LiveSoccerRoulette", "Failed to create socket");
        return;
    }
    
    m_export_running = true;
    m_export_thread = std::thread(&LiveSoccerRoulette::liveExportThread, this);
    m_is_active = true;
    
    Log::info("LiveSoccerRoulette", "Live roulette export started to %s:%d", m_server_ip.c_str(), m_server_port);
}

// ------------------------------------------------------------------------------
void LiveSoccerRoulette::stopExport()
{
    if (m_export_running)
    {
        m_export_running = false;
        if (m_export_thread.joinable())
            m_export_thread.join();
        
        if (m_socket >= 0)
        {
            close(m_socket);
            m_socket = -1;
        }
        
        m_is_active = false;
        Log::info("LiveSoccerRoulette", "Live roulette export stopped");
    }
}

// ------------------------------------------------------------------------------
void LiveSoccerRoulette::resetGame()
{
    m_last_red_score = 0;
    m_last_blue_score = 0;
    
    Log::info("LiveSoccerRoulette", "Game reset - scores back to 0-0");
}

// ------------------------------------------------------------------------------
void LiveSoccerRoulette::sendResetEvent()
{
    if (!m_is_active || m_socket < 0)
        return;

    try
    {
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(m_server_port);
        inet_pton(AF_INET, m_server_ip.c_str(), &server_addr.sin_addr);

        auto send_json = [&](const std::string& payload)
        {
            sendto(m_socket, payload.c_str(), payload.length(), 0,
                   (struct sockaddr*)&server_addr, sizeof(server_addr));
        };

        {
            std::stringstream json;
            json << "{";
            json << "\"timestamp\":" << StkTime::getTimeSinceEpoch() << ",";
            json << "\"type\":\"reset\",";
            json << "\"score\":{\"red\":0,\"blue\":0},";
            json << "\"time\":0,";
            json << "\"ball_possession\":{\"red_seconds\":0,\"blue_seconds\":0,\"red_percent\":0,\"blue_percent\":0},";
            json << "\"players\":[ ]";
            json << "}";
            send_json(json.str());
            Log::info("LiveSoccerRoulette", "Reset event sent (no peers left)");
        }
        {
            std::stringstream json;
            json << "{";
            json << "\"timestamp\":" << StkTime::getTimeSinceEpoch() << ",";
            json << "\"type\":\"update\",";
            json << "\"score\":{\"red\":0,\"blue\":0},";
            json << "\"time\":0,";
            json << "\"ball_possession\":{\"red_seconds\":0,\"blue_seconds\":0,\"red_percent\":0,\"blue_percent\":0},";
            json << "\"players\":[ ]";
            json << "}";
            send_json(json.str());
        }

        m_is_active = false;
        stopExport();
    }
    catch (const std::exception& e)
    {
        Log::error("LiveSoccerRoulette", "Error sending reset event: %s", e.what());
    }
}

// ------------------------------------------------------------------------------
void LiveSoccerRoulette::updateGoal(const std::string& scorer_name, int team, int red_score, int blue_score, float game_time)
{
    if (!m_is_active || m_socket < 0)
        return;
    
    try
    {
        std::stringstream json;
        json << "{";
        json << "\"timestamp\":" << StkTime::getTimeSinceEpoch() << ",";
        json << "\"type\":\"goal\",";
        std::string escaped_name = "";
        for (char c : scorer_name) {
            if (c == '\\' || c == '"') {
                escaped_name += '\\';
            }
            escaped_name += c;
        }
        json << "\"scorer\":\"" << escaped_name << "\",";
        json << "\"team\":" << team << ",";
        json << "\"score\":{\"red\":" << red_score << ",\"blue\":" << blue_score << "},";
        json << "\"time\":" << game_time;
        
        // Add ball possession at goal time
        SoccerWorld* soccer_world = dynamic_cast<SoccerWorld*>(World::getWorld());
        if (soccer_world && soccer_world->isBallTrackingEnabled())
        {
            float red_time = 0.0f;
            float blue_time = 0.0f;
            soccer_world->getBallPossessionTimes(red_time, blue_time);
            float total_time = red_time + blue_time;
            
            if (total_time > 0.0f)
            {
                float red_pct = (red_time / total_time) * 100.0f;
                float blue_pct = (blue_time / total_time) * 100.0f;
                json << ",\"ball_possession\":{";
                json << "\"red_seconds\":" << red_time << ",";
                json << "\"blue_seconds\":" << blue_time << ",";
                json << "\"red_percent\":" << red_pct << ",";
                json << "\"blue_percent\":" << blue_pct;
                json << "}";
            }
        }
        
        json << "}";
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(m_server_port);
        inet_pton(AF_INET, m_server_ip.c_str(), &server_addr.sin_addr);
        
        std::string json_str = json.str();
        sendto(m_socket, json_str.c_str(), json_str.length(), 0,
               (struct sockaddr*)&server_addr, sizeof(server_addr));
        
        // Update internal scores
        m_last_red_score = red_score;
        m_last_blue_score = blue_score;
        
        Log::info("LiveSoccerRoulette", "Goal event sent: %s (Team %d) - Score: %d-%d", 
                 scorer_name.c_str(), team, red_score, blue_score);
    }
    catch (const std::exception& e)
    {
        Log::error("LiveSoccerRoulette", "Error sending goal event: %s", e.what());
    }
}

// ------------------------------------------------------------------------------
void LiveSoccerRoulette::updateGameState(int red_score, int blue_score, float game_time)
{
    m_last_red_score = red_score;
    m_last_blue_score = blue_score;
}

// ------------------------------------------------------------------------------
void LiveSoccerRoulette::liveExportThread()
{
    while (m_export_running)
    {
        exportLiveData();
        std::this_thread::sleep_for(std::chrono::milliseconds(m_update_interval_ms));
    }
}

// ------------------------------------------------------------------------------
void LiveSoccerRoulette::exportLiveData()
{
    if (!World::getWorld() || !m_is_active)
        return;

    SoccerWorld* soccer_world = dynamic_cast<SoccerWorld*>(World::getWorld());
    if (!soccer_world)
        return;
    try
    {
        std::stringstream json;
        json << "{";
        json << "\"timestamp\":" << StkTime::getTimeSinceEpoch() << ",";
        json << "\"type\":\"update\",";
        int red_score = soccer_world->getScore(KART_TEAM_RED);
        int blue_score = soccer_world->getScore(KART_TEAM_BLUE);
        json << "\"score\":{\"red\":" << red_score << ",\"blue\":" << blue_score << "},";
        json << "\"time\":" << soccer_world->getTime() << ",";
        
        // Add ball possession data
        if (soccer_world->isBallTrackingEnabled())
        {
            float red_time = 0.0f;
            float blue_time = 0.0f;
            soccer_world->getBallPossessionTimes(red_time, blue_time);
            float total_time = red_time + blue_time;
            
            if (total_time > 0.0f)
            {
                float red_pct = (red_time / total_time) * 100.0f;
                float blue_pct = (blue_time / total_time) * 100.0f;
                json << "\"ball_possession\":{";
                json << "\"red_seconds\":" << red_time << ",";
                json << "\"blue_seconds\":" << blue_time << ",";
                json << "\"red_percent\":" << red_pct << ",";
                json << "\"blue_percent\":" << blue_pct;
                json << "},";
            }
            else
            {
                json << "\"ball_possession\":{\"red_seconds\":0,\"blue_seconds\":0,\"red_percent\":0,\"blue_percent\":0},";
            }
        }
        
        // Add team names (groups) if available
        SoccerRoulette* sr = SoccerRoulette::get();
        if (sr)
        {
            std::string red_group = sr->getGroupForColor("red");
            std::string blue_group = sr->getGroupForColor("blue");
            if (!red_group.empty() || !blue_group.empty())
            {
                json << "\"teams\":{";
                json << "\"red\":\"" << (red_group.empty() ? "Red" : red_group) << "\",";
                json << "\"blue\":\"" << (blue_group.empty() ? "Blue" : blue_group) << "\"";
                json << "},";
            }
            
            std::string current_field = sr->getCurrentField();
            if (!current_field.empty())
            {
                json << "\"field\":\"" << current_field << "\",";
            }
        }
        
        json << "\"players\":[";
        bool first_player = true;
        for (unsigned int i = 0; i < World::getWorld()->getNumKarts(); i++)
        {
            AbstractKart* kart = World::getWorld()->getKart(i);
            if (!kart || kart->isEliminated())
                continue;
            
            if (!first_player)
                json << ",";
            first_player = false;
            int team = (int)soccer_world->getKartTeam(kart->getWorldKartId());
            std::string player_name = StringUtils::wideToUtf8(kart->getController()->getName());
            std::string escaped_name = "";
            for (char c : player_name) 
            {
                if (c == '\\' || c == '"') 
                {
                    escaped_name += '\\';
                }
                escaped_name += c;
            }   
            json << "{\"name\":\"" << escaped_name << "\",\"team\":" << team << "}";
        }
        json << "]}";
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(m_server_port);
        inet_pton(AF_INET, m_server_ip.c_str(), &server_addr.sin_addr);
        
        std::string json_str = json.str();
        sendto(m_socket, json_str.c_str(), json_str.length(), 0,
               (struct sockaddr*)&server_addr, sizeof(server_addr));
    }
    catch (const std::exception& e)
    {
        Log::error("LiveSoccerRoulette", "Error exporting live data: %s", e.what());
    }
}
