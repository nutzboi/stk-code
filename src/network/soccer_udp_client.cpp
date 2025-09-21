//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2004-2015 Ingo Ruhnke <grumbel@gmx.de>
//  Copyright (C) 2006-2015 SuperTuxKart-Team
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

#include "soccer_udp_client.hpp"
#include "utils/log.hpp"
#include "utils/time.hpp"
#include "network/server_config.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include "soccer_udp_client.hpp"
#include "utils/log.hpp"
#include "utils/time.hpp"
#include "network/server_config.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>

SoccerUDPClient* SoccerUDPClient::m_instance = nullptr;

SoccerUDPClient::SoccerUDPClient()
    : m_server_port(8765), m_socket(-1), m_connected(false)
{
}

SoccerUDPClient::~SoccerUDPClient()
{
    disconnect();
}

SoccerUDPClient* SoccerUDPClient::getInstance()
{
    if (!m_instance)
    {
        m_instance = new SoccerUDPClient();
    }
    return m_instance;
}

void SoccerUDPClient::destroy()
{
    if (m_instance)
    {
        delete m_instance;
        m_instance = nullptr;
    }
}

void SoccerUDPClient::connect(const std::string& ip_port_str, int default_port)
{
    if (m_connected.load())
    {
        Log::warn("SoccerUDP", "Already connected to UDP server");
        return;
    }
    
    // Parse IP:port string
    size_t colon_pos = ip_port_str.find(':');
    if (colon_pos != std::string::npos)
    {
        m_server_ip = ip_port_str.substr(0, colon_pos);
        m_server_port = std::stoi(ip_port_str.substr(colon_pos + 1));
    }
    else
    {
        m_server_ip = ip_port_str;
        m_server_port = default_port;
    }
    
    // Create UDP socket
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0)
    {
        Log::error("SoccerUDP", "Failed to create socket");
        return;
    }
    
    // Setup server address
    memset(&m_server_addr, 0, sizeof(m_server_addr));
    m_server_addr.sin_family = AF_INET;
    m_server_addr.sin_port = htons(m_server_port);
    
    if (inet_pton(AF_INET, m_server_ip.c_str(), &m_server_addr.sin_addr) <= 0)
    {
        Log::error("SoccerUDP", "Invalid server IP address: %s", m_server_ip.c_str());
        close(m_socket);
        m_socket = -1;
        return;
    }
    m_connected = true;
    Log::info("SoccerUDP", "Connected to UDP server %s:%d", m_server_ip.c_str(), m_server_port);
}

void SoccerUDPClient::disconnect()
{
    if (m_socket >= 0)
    {
        close(m_socket);
        m_socket = -1;
    }
    m_connected = false;
    Log::info("SoccerUDP", "Disconnected from UDP server");
}

void SoccerUDPClient::sendEvent(const SoccerEvent& event)
{
    if (!m_connected.load() || m_socket < 0)
        return;
        
    try
    {
        std::stringstream json;
        json << "{";
        json << "\"type\":\"" << event.type << "\",";
        json << "\"player_name\":\"" << event.player_name << "\",";
        json << "\"team\":" << event.team << ",";
        json << "\"red_score\":" << event.red_score << ",";
        json << "\"blue_score\":" << event.blue_score << ",";
        json << "\"game_time\":" << event.game_time << ",";
        json << "\"timestamp\":\"" << event.timestamp << "\",";
        json << "\"addon_info\":\"" << event.addon_info << "\"";
        json << "}";
        std::string json_str = json.str();
        ssize_t sent = sendto(m_socket, json_str.c_str(), json_str.length(), 0,
                             (struct sockaddr*)&m_server_addr, sizeof(m_server_addr));
        if (sent < 0)
        {
            Log::error("SoccerUDP", "Failed to send UDP packet");
        }
        else
        {
            Log::verbose("SoccerUDP", "Sent event: %s", event.type.c_str());
        }
    }
    catch (const std::exception& e)
    {
        Log::error("SoccerUDP", "Error sending event: %s", e.what());
    }
}

void SoccerUDPClient::sendPlayerJoin(const std::string& player_name, int team)
{
    SoccerEvent event;
    event.type = "player_join";
    event.player_name = player_name;
    event.team = team;
    event.game_time = -1.0f;  // Special value to indicate no game_time
			      // This is because these are not live joins, 
			      // but people who have been participating from the start.
    event.timestamp = std::to_string(StkTime::getTimeSinceEpoch());
    
    sendEvent(event);
}

void SoccerUDPClient::sendPlayerJoin(const std::string& player_name, int team, float game_time)
{
    SoccerEvent event;
    event.type = "player_join";
    event.player_name = player_name;
    event.team = team;
    event.game_time = game_time;
    event.timestamp = std::to_string(StkTime::getTimeSinceEpoch());
    
    sendEvent(event);
}

void SoccerUDPClient::sendPlayerLeave(const std::string& player_name, float game_time)
{
    SoccerEvent event;
    event.type = "player_leave";
    event.player_name = player_name;
    event.game_time = game_time;
    event.timestamp = std::to_string(StkTime::getTimeSinceEpoch());
    
    sendEvent(event);
}

void SoccerUDPClient::sendGoal(const std::string& player_name, int team, int red_score, int blue_score, float game_time)
{
    Log::info("SoccerUDP", "Sending goal event: %s team %d score %d-%d", 
              player_name.c_str(), team, red_score, blue_score);
    
    SoccerEvent event;
    event.type = "goal";
    event.player_name = player_name;
    event.team = team;
    event.red_score = red_score;
    event.blue_score = blue_score;
    event.game_time = game_time;
    event.timestamp = std::to_string(StkTime::getTimeSinceEpoch());
    
    sendEvent(event);
}

void SoccerUDPClient::sendGameStart()
{
    SoccerEvent event;
    event.type = "game_start";
    event.timestamp = std::to_string(StkTime::getTimeSinceEpoch());
    
    sendEvent(event);
}

void SoccerUDPClient::sendGameStart(const std::string& addon_info)
{
    SoccerEvent event;
    event.type = "game_start";
    event.timestamp = std::to_string(StkTime::getTimeSinceEpoch());
    event.addon_info = addon_info;
    
    sendEvent(event);
}

void SoccerUDPClient::sendGameEnd(float game_time)
{
    SoccerEvent event;
    event.type = "game_end";
    event.game_time = game_time;
    event.timestamp = std::to_string(StkTime::getTimeSinceEpoch());
    
    sendEvent(event);
}
