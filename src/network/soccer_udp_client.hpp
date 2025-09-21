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

#ifndef HEADER_SOCCER_UDP_CLIENT_HPP
#define HEADER_SOCCER_UDP_CLIENT_HPP

#include <string>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct SoccerEvent {
    std::string type;           // "game_start", "game_end", "goal", "player_join", "player_leave"
    std::string player_name;
    int team;                   // 0=red, 1=blue
    int red_score;
    int blue_score;
    float game_time;
    std::string timestamp;
    std::string addon_info;
};

class SoccerUDPClient
{
private:
    std::string m_server_ip;
    int m_server_port;
    int m_socket;
    struct sockaddr_in m_server_addr;
    std::atomic<bool> m_connected;
    static SoccerUDPClient* m_instance;
    void sendEvent(const SoccerEvent& event);
    
public:
    SoccerUDPClient();
    ~SoccerUDPClient();
    
    static SoccerUDPClient* getInstance();
    static void destroy();
    void connect(const std::string& ip_port_str, int default_port = 8765);
    void disconnect();
    bool isConnected() const { return m_connected.load(); }
    // Event sending methods
    void sendPlayerJoin(const std::string& player_name, int team);
    void sendPlayerJoin(const std::string& player_name, int team, float game_time);
    void sendPlayerLeave(const std::string& player_name, float game_time);
    void sendGoal(const std::string& player_name, int team, int red_score, int blue_score, float game_time);
    void sendGameStart();
    void sendGameStart(const std::string& addon_info);
    void sendGameEnd(float game_time);
};

#endif
