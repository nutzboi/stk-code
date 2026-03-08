//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2018 SuperTuxKart-Team
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

// Ported from database_connector.hpp: splitting databases into an abstraction layer.

#ifndef ABSTRACT_DATABASE_HPP
#define ABSTRACT_DATABASE_HPP

#include "utils/time.hpp"
#include <memory>
#include <vector>
#include <irrString.h>

class SocketAddress;
class STKPeer;
class NetworkPlayerProfile;

// The method signatures are kept from database_connector.hpp to keep
// reasonable compatibility for different database backends.
// For now, synchronous API abstraction is here. TODO: Asynchronous API
//
class AbstractDatabase
{

    // Member declarations
public:
    /** Corresponds to the row of IPv4 ban table. */
    struct IpBanTableData
    {
        int row_id;
        uint32_t ip_start;
        uint32_t ip_end;
        std::string reason;
        std::string description;
    };
    /** Corresponds to the row of IPv6 ban table. */
    struct Ipv6BanTableData
    {
        int row_id;
        std::string ipv6_cidr;
        std::string reason;
        std::string description;
    };
    /** Corresponds to the row of online id ban table. */
    struct OnlineIdBanTableData
    {
        int row_id;
        uint32_t online_id;
        std::string reason;
        std::string description;
    };

    // Implementation-independent data
protected:
    std::string m_server_stats_table;
    std::string m_game_stats_table;
    bool m_ip_ban_table_exists;
    bool m_ipv6_ban_table_exists;
    bool m_online_id_ban_table_exists;
    bool m_ip_geolocation_table_exists;
    bool m_ipv6_geolocation_table_exists;
    bool m_player_reports_table_exists;
    /* Moderation toolkit */
    bool m_permissions_table_exists;
    bool m_restrictions_table_exists;
    /* /Moderation toolkit */
    uint64_t m_last_poll_db_time;

public:

    // Done at initialization, in the ServerLobby class or somewhere else.
    // The class is a singleton, and such can be accessed anywhere.
    // Instantiates the implmentation which can be get with getInstance()

    virtual void init() = 0;
    virtual void finalize() = 0;
    virtual ~AbstractDatabase() {};

    // bool easySQLQuery(const std::string& query,
    //                    std::vector<std::vector<std::string>>* output = nullptr,
    //            std::function<void(sqlite3_stmt* stmt)> bind_function = nullptr,
    //                                         std::string null_value = "") const;

    virtual void checkTableExists(const std::string& table, bool& result) = 0;

    virtual std::string ip2Country(const SocketAddress& addr) const = 0;

    virtual std::string ipv62Country(const SocketAddress& addr) const = 0;

    virtual void writeDisconnectInfoTable(STKPeer* peer) = 0;
    virtual void initServerStatsTable() = 0;
    virtual void initGameStatsTable() = 0;
    // Initialize ball-side stats table for soccer roulette
    virtual void initBallSideStatsTable() = 0;
    virtual void initSoccerRouletteGameResultsTable() = 0;
    virtual void initSoccerRouletteGoalDetailsTable() = 0;
    virtual void initSoccerRoulettePlayerPerformanceTable() = 0;
    virtual void writeGameStats(int game_id, const std::string& player_name, uint32_t online_id,
                               const std::string& game_mode, const std::string& track_name,
                               int swatter_hits, int cake_hits, int bowling_hits = 0,
                               int bowling_used = 0, int bowling_puck = 0, int bowling_players = 0,
                               const std::string& team_color = "", const std::string& team_group = "") = 0;
    // Write ball side statistics (roulette): stores both epoch ms and formatted datetime
    virtual void writeBallSideStats(int game_id, uint64_t timestamp_ms, const std::string& track_id,
                                    float red_seconds, float blue_seconds,
                                    float red_pct, float blue_pct,
                                    const std::string& team_vs) = 0;
    virtual int writeSoccerRouletteGameResult(
        uint64_t timestamp_ms, const std::string& track_id,
        int red_score, int blue_score, int red_total_points, int blue_total_points,
        const std::string& red_team_name, const std::string& blue_team_name,
        const std::string& team_vs, const std::string& fastest_player,
        const std::string& fastest_team, float fastest_speed, float game_duration) = 0;
    virtual void writeSoccerRouletteGoalDetail(
        int game_id, const std::string& player_name, const std::string& team,
        float speed, float goal_time, uint64_t timestamp_ms) = 0;
    virtual void writeSoccerRoulettePlayerPerformance(
        int game_id, const std::string& player_name, uint32_t online_id,
        const std::string& team, int goals_scored, float total_speed,
        float fastest_speed, int points_from_goals, int points_from_fastest,
        int total_points, const std::string& team_vs) = 0;
    virtual void emptySoccerRouletteDatabase() = 0;
    virtual bool writeReport(
         STKPeer* reporter, std::shared_ptr<NetworkPlayerProfile> reporter_npp,
       STKPeer* reporting, std::shared_ptr<NetworkPlayerProfile> reporting_npp,
                                                     irr::core::stringw& info) = 0;
    virtual bool writeFeatureMessage(const std::string& player_name,
                                     const std::string& message) = 0;
    virtual bool writeTextReport(const std::string& player_name,
                                 const std::string& message) = 0;
    virtual bool hasDatabase() const = 0;
    bool hasServerStatsTable() const  { return !m_server_stats_table.empty(); }
    bool hasPlayerReportsTable() const
                                      { return m_player_reports_table_exists; }
    bool hasIpBanTable() const                { return m_ip_ban_table_exists; }
    bool hasIpv6BanTable() const            { return m_ipv6_ban_table_exists; }
    bool hasOnlineIdBanTable() const   { return m_online_id_ban_table_exists; }
    bool hasPermissionsTable() const     { return m_permissions_table_exists; }
    bool hasRestrictionsTable() const    { return m_permissions_table_exists; }
    bool isTimeToPoll() const
            { return StkTime::getMonoTimeMs() >= m_last_poll_db_time + 60000; }
    void updatePollTime()   { m_last_poll_db_time = StkTime::getMonoTimeMs(); }
    virtual std::vector<IpBanTableData> getIpBanTableData(uint32_t ip = 0) const = 0;
    virtual std::vector<Ipv6BanTableData> getIpv6BanTableData(std::string ipv6 = "") const = 0;
    virtual std::vector<OnlineIdBanTableData> getOnlineIdBanTableData(uint32_t online_id = 0) const = 0;
    virtual void increaseIpBanTriggerCount(uint32_t ip_start, uint32_t ip_end) const = 0;
    virtual void increaseIpv6BanTriggerCount(const std::string& ipv6_cidr) const = 0;
    virtual void increaseOnlineIdBanTriggerCount(uint32_t online_id) const = 0;
    virtual void clearOldReports() = 0;
    virtual void setDisconnectionTimes(std::vector<uint32_t>& present_hosts) = 0;
    virtual void saveAddressToIpBanTable(const SocketAddress& addr) = 0;
    virtual void onPlayerJoinQueries(std::shared_ptr<STKPeer> peer, uint32_t online_id,
        unsigned player_count, const std::string& country_code) = 0;
    virtual void listBanTable(std::stringstream& out) = 0;

    /* Moderation toolkit */
    virtual int loadPermissionLevelForOID(uint32_t online_id) = 0;
    virtual int loadPermissionLevelForUsername(const irr::core::stringw& name) = 0;
    virtual void writePermissionLevelForOID(uint32_t online_id, int lvl) = 0;
    virtual void writePermissionLevelForUsername(const irr::core::stringw& name, int lvl) = 0;
    virtual std::tuple<uint32_t, std::string> loadRestrictionsForOID(uint32_t online_id) = 0;
    virtual std::tuple<uint32_t, std::string> loadRestrictionsForUsername(const irr::core::stringw& name) = 0;
    virtual void writeRestrictionsForOID(uint32_t online_id, uint32_t flags) = 0;
    virtual void writeRestrictionsForOID(uint32_t online_id, uint32_t flags, const std::string& set_kart) = 0;
    virtual void writeRestrictionsForOID(uint32_t online_id, const std::string& set_kart) = 0;
    virtual void writeRestrictionsForUsername(const irr::core::stringw& name, uint32_t flags) = 0;
    virtual void writeRestrictionsForUsername(const irr::core::stringw& name, uint32_t flags, const std::string& set_kart) = 0;
    virtual void writeRestrictionsForUsername(const irr::core::stringw& name, const std::string& set_kart) = 0;
    virtual uint32_t lookupOID(const std::string& name) = 0;
    virtual uint32_t lookupOID(const irr::core::stringw& name) = 0;
    virtual int banPlayer(const std::string& name, const std::string& reason, int days = -1) = 0;
    virtual int unbanPlayer(const std::string& name) = 0;
    virtual const std::string formatBanList(unsigned int page, unsigned int psize) = 0;
    virtual const std::string formatBanInfo(const std::string& name) = 0;
    /* /Moderation toolkit */
};

#endif // ABSTRACT_DATABASE_HPP
