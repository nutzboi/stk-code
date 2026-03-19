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

#include "utils/git_version.hpp"

namespace
{
#ifndef GIT_VERSION_STRING
#   define GIT_VERSION_STRING "unknown"
#endif

#ifndef GIT_SUBJECT_STRING
#   define GIT_SUBJECT_STRING ""
#endif

#ifndef GIT_BRANCH_STRING
#   define GIT_BRANCH_STRING ""
#endif
}

namespace GitVersion
{
    std::string version()
    {
        return std::string(GIT_VERSION_STRING);
    }

    std::string subject()
    {
        return std::string(GIT_SUBJECT_STRING);
    }

    std::string branch()
    {
        return std::string(GIT_BRANCH_STRING);
    }

    std::string pretty()
    {
        std::string v = version();
        const std::string s = subject();
        const std::string b = branch();

        if (!s.empty())
        {
            v += " - ";
            v += s;
        }
        if (!b.empty())
        {
            v += " (branch ";
            v += b;
            v += ")";
        }
        return v;
    }
}
