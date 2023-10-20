/*
 *  Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#define ERRNO_SENTINEL 1000 /* An invalid error number */

#define ERR(msg) do { \
        printf("fatal: %s: (%d) %s\n", msg, errno, strerror(errno)); \
        exit(1); \
    } while (0)

int main(int argc, char **argv)
{
    bool first = true;
    assert(argc == 2);
    DIR *dirp = opendir(argv[1]);
    if (!dirp) {
        ERR("couldn't open dir");
    }

    while (1) {
        errno = ERRNO_SENTINEL;
        struct dirent *dp = readdir(dirp);
        if (!dp) {
            /* FIXME: this check is currently disabled due
             * to a bug in hexagon-libc (see QTOOL-106440).
             */
            /*
            if (errno != ERRNO_SENTINEL) {
                ERR("error reading directory");
            }
            */
            break;
        }
        printf("%s%s", first ? "" : " ", dp->d_name);
        first = false;
    }

    if (closedir(dirp)) {
        ERR("closedir error");
    }
    return 0;
}
