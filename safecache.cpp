/***************************************************************************
 * A managed buffer for sensitive information.
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

#include <new>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/random.h>
#include "safecache.h"

SafeCache::SafeCache(size_t nbytes)
{
    const size_t pagesize = sysconf(_SC_PAGESIZE);
    bufsize = pagesize * (1 + ((nbytes-1)/pagesize)); // Integer multiple of pagesize.
    buf = aligned_alloc(pagesize, bufsize);
    if (!buf) throw std::bad_alloc();
    if (mlock2(buf, bufsize, MLOCK_ONFAULT) != 0) throw std::bad_alloc();
}
SafeCache::~SafeCache()
{
    getrandom(buf, bufsize, 0);
    explicit_bzero(buf, bufsize);
    munlock(buf, bufsize);
    free(buf);
}

bool SafeCache::save(const char *filename, mode_t mode, bool generate) noexcept
{
    if (generate && getrandom(buf, bufsize, 0) != static_cast<ssize_t>(bufsize)) return false;

    const int fd = open(filename, O_CREAT|O_TRUNC|O_WRONLY|O_DIRECT|O_SYNC, mode);
    if (fd == -1) return false;
    const bool ok = (write(fd, buf, bufsize) == static_cast<ssize_t>(bufsize));
    close(fd);
    return ok;
}
