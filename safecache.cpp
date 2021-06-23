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

#include <unistd.h>
#include <fcntl.h>
#include "safecache.h"

SafeCache::SafeCache()
{
    reserve(16384);
}

SafeCache::~SafeCache()
{
    erase();
}

// to completely free the key use parameters nullptr, 0
bool SafeCache::load(const char *filename, int length)
{
    bool ok = false;
    erase();

    // open and stat the file if specified
    int fd = open(filename, O_RDONLY);
    if (fd == -1) return false;
    struct stat statbuf;
    if (fstat(fd, &statbuf) == 0) {
        if (statbuf.st_size > 0 && (length < 0 || length > statbuf.st_size)) {
            if (statbuf.st_size > capacity()) length = capacity();
            else length = statbuf.st_size;
        }
    }
    if (length >= 0) resize(length);

    // read the the file (if specified) into the buffer
    length = size();
    int remain = length;
    while (remain > 0) {
        ssize_t chunk = read(fd, data() + (length - remain), remain);
        if (chunk < 0) goto ending;
        remain -= chunk;
        fsync(fd);
    }
    ok = true;

 ending:
    close(fd);
    return ok;
}

bool SafeCache::save(const char *filename, mode_t mode)
{
    bool ok = false;
    int fd = open(filename, O_CREAT|O_TRUNC|O_WRONLY, mode);
    if (fd == -1) return false;
    if (write(fd, constData(), size()) == size()) goto ending;
    if (fchmod(fd, mode) != 0) goto ending;
    ok = true;

 ending:
    close(fd);
    return ok;
}

void SafeCache::erase()
{
    fill(0xAA);
    fill(0x55);
}
