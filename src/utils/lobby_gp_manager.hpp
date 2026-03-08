//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2025 kimden
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

#ifndef LOBBY_GP_MANAGER_HPP
#define LOBBY_GP_MANAGER_HPP

#include "irrString.h"
#include "utils/track_filter.hpp"
#include "utils/lobby_context.hpp"
#include "utils/gp_scoring.hpp"

#include <memory>
#include <queue>

class NetworkPlayerProfile;
class NetworkString;
class GPScoring;

struct GPScore
{
    int score = 0;
    double time = 0.;
    bool operator < (const GPScore& rhs) const
    {
        return (score < rhs.score || (score == rhs.score && time > rhs.time));
    }
    bool operator > (const GPScore& rhs) const
    {
        return (score > rhs.score || (score == rhs.score && time < rhs.time));
    }
};

class LobbyGPManager: public LobbyContextComponent
{
public:
    LobbyGPManager(LobbyContext* context): LobbyContextComponent(context) {}
    
    void setupContextUser() OVERRIDE;

    void onStartSelection();

    void setScoresToPlayer(std::shared_ptr<NetworkPlayerProfile> player);

    std::string getGrandPrixStandings(bool showIndividual = false, bool showTeam = true);

    void resetGrandPrix();

    void shuffleGPScoresWithPermutation(const std::map<int, int>& permutation);

    void updateGPScores(std::vector<float>& gp_changes, NetworkString* ns);

    bool trySettingGPScoring(const std::string& input);

    void updateWorldScoring();

    std::string getScoringAsString();

    void cloneGPSlot(unsigned from, unsigned to) {
        gp_data.cloneGPSlot(from, to);
    }
    void setGPSlot(unsigned i) {
        gp_data.setGPSlot(i);
    }

private:
    struct GrandPrixSlot {
        std::map<std::string, GPScore> m_gp_scores;
        std::map<int, GPScore> m_gp_team_scores;
        std::shared_ptr<GPScoring> m_gp_scoring;
    };
    #define GP_CHECK(__x) if (__x >= SLOT_NUM) throw std::out_of_range("Invalid GP slot.");
    class GrandPrixData {
    friend LobbyGPManager;
    private:
        static const unsigned SLOT_NUM = 10;
        GrandPrixSlot m_slots[SLOT_NUM];
        unsigned m_current_slot = 0;
    protected:
        std::map<std::string, GPScore> &getGPPlayerScores(void) {
            GP_CHECK(m_current_slot);
            return m_slots[m_current_slot].m_gp_scores;
        }
        std::map<int, GPScore> &getGPTeamScores(void) {
            GP_CHECK(m_current_slot);
            return m_slots[m_current_slot].m_gp_team_scores;
        }
        std::shared_ptr<GPScoring> &getGPScoring(void) {
            GP_CHECK(m_current_slot);
            return m_slots[m_current_slot].m_gp_scoring;
        }
        void setGPPlayerScores(std::map<std::string, GPScore> &x) {
            GP_CHECK(m_current_slot);
            m_slots[m_current_slot].m_gp_scores = x;
        }
        void setGPTeamScores(std::map<int, GPScore> &x) {
            GP_CHECK(m_current_slot);
            m_slots[m_current_slot].m_gp_team_scores = x;
        }
        void setGPScoring(std::shared_ptr<GPScoring> x) {
            GP_CHECK(m_current_slot);
            m_slots[m_current_slot].m_gp_scoring = x;
        }
    public:
        void cloneGPSlot(unsigned from, unsigned to) {
            GP_CHECK(from);
            GP_CHECK(to);
            m_slots[to].m_gp_scores = m_slots[from].m_gp_scores;
            m_slots[to].m_gp_team_scores = m_slots[from].m_gp_team_scores;
            if (m_slots[from].m_gp_scoring) {
                m_slots[to].m_gp_scoring = GPScoring::createFromIntParamString(m_slots[from].m_gp_scoring->toString());
            }
        }
        void setGPSlot(unsigned i) {
            GP_CHECK(i);
            m_current_slot = i;
        }
    };
    GrandPrixData gp_data;
    #undef GP_CHECK

};

#endif // LOBBY_GP_MANAGER_HPP
