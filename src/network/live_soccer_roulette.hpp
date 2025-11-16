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

#ifndef HEADER_LIVE_SOCCER_ROULETTE_HPP
#define HEADER_LIVE_SOCCER_ROULETTE_HPP

#include <string>
#include <thread>
#include <atomic>

class AbstractKart;

class LiveSoccerRoulette
{
private:
    std::string m_server_ip;
    int m_server_port;
    int m_update_interval_ms;
    int m_socket;

    std::atomic<bool> m_export_running;
    std::thread m_export_thread;
    int m_last_red_score;
    int m_last_blue_score;
    std::atomic<bool> m_is_active;
    void liveExportThread();
    void exportLiveData();
    static LiveSoccerRoulette* m_instance;
    
public:
    LiveSoccerRoulette();
    ~LiveSoccerRoulette();
    
    static LiveSoccerRoulette* getInstance();
    static void destroy();
    void startExport(const std::string& server_ip = "127.0.0.1", int server_port = 9878, int update_interval_ms = 2000);
    void stopExport();
    void resetGame();
    void sendResetEvent();
    void updateGoal(const std::string& scorer_name, int team, int red_score, int blue_score, float game_time);
    void updateGameState(int red_score, int blue_score, float game_time);
    bool isActive() const { return m_is_active.load(); }
};

#endif
