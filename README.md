# SuperTuxKart TierS Server Fork

> **An extensive fork of SuperTuxKart specifically developed for TierS servers with advanced multiplayer functionalities**

# About this fork:
This fork is specifically developed for **TierS servers** and is used by multiple STK servers worldwide. It contains extensive modifications for server hosting with advanced moderation tools, many commands.

### Goals
Creating a robust, scalable version of SuperTuxKart that is optimally suited for:
- **Server hosting**
- **Advanced moderation and management**
- **Custom game modes and features**

## Features
- **Advanced Moderation Tools** - Extensive management functionalities
- **Permission Levels** - Access control
- **Player Restrictions** - Comprehensive player management

### Live Join System
- **Flexible Live Join Modes:** (spectating is always possible)
  - `LIVE_JOIN_NONE` - No live join possible
  - `LIVE_JOIN_RECONNECT` - Only reconnect for players who were already in game
  - `LIVE_JOIN_ALL` - Everyone can join (default)
- **Player Session Tracking** - Intelligent reconnect validation

### Custom Content & Features
- **TierS Roulette System** - Rotating game modifiers

### Game Modes & Features
- **Soccer Mode Enhancements** - Improved soccer gameplay
- **Special Powerup Modifiers** - Bowl party, cake party, etc.
- **Kart Restriction Modes** - Heavy, medium, light kart restrictions
- **Item Chaos Mode** - Random item distribution
- **Pole Position System** - Team voting for starting positions
- **External Team Balancer** - Advanced team balancing via external server integration (requires permission from TierS Servers group)

## Installation

### Requirements
- **OS:** Linux, Windows, macOS, Android, iOS, Nintendo Switch
- **Graphics:** OpenGL 3.3+ compatible GPU
- **CPU:** Dual-core 1 GHz+
- **RAM:** 1 GB minimum (The more, the better)
- **Storage:** 700 MB free space

### Build Instructions
For detailed installation instructions, see [INSTALL.md](INSTALL.md).

### Server Configuration
```bash
# Start server with custom config (We assume that you have already linked your account.)
./supertuxkart --server-config=server_config.xml
```

### TierS Roulette Configuration
```xml
<!-- Roulette sequence for rotating features -->
<tiers-roulette-sequence value="kartheavy+itemchaos,kartmedium+itemchaos,kartlight+itemchaos,bowlparty,cakeparty" />
```

## Contributing

We welcome contributions from the community!
### Bug Reports
- Open an issue for bugs on our servers
- Provide detailed information about the problem
- Add logs if available

### Feature Requests
- Describe the desired functionality
- Explain why it would be useful
- Consider implementation complexity

### Code Contributions
- Fork the repository
- Create a feature branch
- Test your changes
- Submit a pull request

This fork is inspired by this fork:
[vinder-af-karting-spil/stk-code](https://github.com/vinder-af-karting-spil/stk-code)
He may have used code from other forks as well, which is noted in his README.md.

## Links
- **Website:** [tiersservers.eu](https://tiersservers.eu)
- **Original STK:** [supertuxkart.net](https://supertuxkart.net/)

---
