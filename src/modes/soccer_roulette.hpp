#ifndef HEADER_SOCCER_ROULETTE_HPP
#define HEADER_SOCCER_ROULETTE_HPP

#include "lobby/stk_command_context.hpp"
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <memory>

class STKPeer;
class NetworkPlayerProfile;

/**
 * \brief Class that manages soccer roulette mode.
 * \ingroup modes
 */
class SoccerRoulette
{
private:
    static SoccerRoulette* m_soccer_roulette;
    std::vector<std::string> m_fields;
    std::map<std::string, std::string> m_player_teams;
    std::map<std::string, std::vector<std::string> > m_group_members;
    std::map<std::string, std::string> m_group_to_color;
    int m_current_field_index;
    int m_minimap_socket;
    std::atomic<bool> m_minimap_running;
    std::thread m_minimap_thread;
    int m_minimap_update_interval_ms;
    std::string m_minimap_server_ip;
    int m_minimap_server_port;
    std::atomic<bool> m_active{false};
    std::atomic<bool> m_golden_goal_active{false};
    // Accumulated times for last finished game (in seconds)
    float m_last_red_side_time = 0.0f;
    float m_last_blue_side_time = 0.0f;
    void loadFieldsFromConfig();
    void minimapExportThread();
    void exportMinimapData();
    SoccerRoulette();
    ~SoccerRoulette();

public:
    // Last game ID for linking ball side stats
    int m_last_game_id = -1;
    static void create();
    static void destroy();
    static SoccerRoulette* get();
    bool isEnabled() const;
    bool isActive() const { return m_active.load(); }
    void setActive(bool v) { m_active.store(v); }
    bool isGoldenGoalActive() const { return m_golden_goal_active.load(); }
    void activateGoldenGoal() { m_golden_goal_active.store(true); }
    void deactivateGoldenGoal() { m_golden_goal_active.store(false); }
    void reload();
    void resetIndex();
    std::string getNextField();
    std::string getCurrentField() const;
    void addField(const std::string& field);
    void removeField(const std::string& field);
    std::string getTeamForPlayer(const std::string& player_name) const;
    void assignTeamToPlayer(const std::string& player_name, const std::string& team);
    void assignTeamToPlayer(NetworkPlayerProfile* profile);
    std::string getTeamsInfo() const;
    bool checkRequiredAddons(STKPeer* peer, std::string& error_msg);
    void giveNitroToAll();
    const std::vector<std::string>& getFields() const { return m_fields; }
    void resetFieldIndex();
    void calculateGameResult();
    std::string getLastGameResults();
    void kickPlayer(const std::string& player_name, STKCommandContext* kicker);
    void reassignTeams(STKCommandContext* commander);
    void loadTeamsFromXML();
    void setRouletteTimeout(STKCommandContext* commander);
    bool isPlayerInTeam(const std::string& player_name);
    bool setGroupColor(const std::string& group, const std::string& color);
    std::string getGroupColor(const std::string& group) const;
    std::string getGroupForColor(const std::string& color) const;
    std::vector<std::string> listGroups() const;
    
    // Ball position tracking (persisted per finished game)
    void setBallTrackingData(float red_time, float blue_time);
    void clearBallTrackingData();
    void getBallSideTimes(float& red_time, float& blue_time) const
    {
        red_time = m_last_red_side_time;
        blue_time = m_last_blue_side_time;
    }
    bool hasBallTrackingData() const
    {
        return (m_last_red_side_time > 0.0f || m_last_blue_side_time > 0.0f);
    }
    std::string getBallPositionPercentages() const;
    std::string getFixedMixedBar() const;
    void writeBallSideStatsToDatabase();

    // Player stats tracking
    static void recordSwatterHit(const std::string& player_name);
    static void recordCakeHit(const std::string& player_name);
    static void recordBowlingHit(const std::string& player_name);
    static void recordBowlingUsed(const std::string& player_name);
    static void recordBowlingPuckHit(const std::string& player_name);
    static void recordBowlingPlayerHit(const std::string& player_name);
    static int getSwatterHits(const std::string& player_name);
    static int getCakeHits(const std::string& player_name);
    static int getBowlingHits(const std::string& player_name);
    static int getBowlingUsed(const std::string& player_name);
    static int getBowlingPuckHits(const std::string& player_name);
    static int getBowlingPlayerHits(const std::string& player_name);
    static void resetPlayerStats();
    static void writeStatsToDatabase(int game_id = -1);
};

#endif

