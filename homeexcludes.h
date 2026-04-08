/***************************************************************************
 * Home directory exclude patterns for rsync operations.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 * This file is part of the gazelle-installer.
 ***************************************************************************/
#ifndef HOMEEXCLUDES_H
#define HOMEEXCLUDES_H

#include <array>
#include <QLatin1StringView>

// Files and directories excluded when copying user home directories.
// Session-specific runtime data, caches, and sensitive history files.
inline constexpr std::array homeExcludes {
    // Session runtime
    QLatin1StringView(".cache"),
    QLatin1StringView(".dbus"),
    QLatin1StringView(".gvfs"),
    QLatin1StringView(".Xauthority"),
    QLatin1StringView(".ICEauthority"),
    // Shell and tool history
    QLatin1StringView(".bash_history"),
    QLatin1StringView(".zsh_history"),
    QLatin1StringView(".python_history"),
    QLatin1StringView(".local/share/mc/history"),
    QLatin1StringView(".local/state/lesshst"),
    // Clipboard and recent files
    QLatin1StringView(".local/share/xfce4/clipman"),
    QLatin1StringView(".local/share/recently-used.xbel"),
    // Optional live-media persistence storage
    QLatin1StringView("Live-usb-storage"),
};

#endif // HOMEEXCLUDES_H
