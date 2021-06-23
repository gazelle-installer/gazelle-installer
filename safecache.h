/***************************************************************************
 * SafeCache Class - for temporarily caching sensitive files
 ***************************************************************************
 *
 *   Copyright (C) 2019 by AK-47
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

#ifndef SAFECACHE_H
#define SAFECACHE_H

#include <sys/stat.h>
#include <QByteArray>

class SafeCache : public QByteArray
{
public:
    SafeCache();
    ~SafeCache();
    bool load(const char *filename, int length);
    bool save(const char *filename, mode_t mode = 0400);
    void erase();
};

#endif // SAFECACHE_H
