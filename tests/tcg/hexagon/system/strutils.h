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

#ifndef STRUTILS_H
#define STRUTILS_H

#include <stdlib.h>
#include <string.h>

int compare_str(const void *a, const void *b)
{
    return strcmp(*(char **)a, *(char **)b);
}

void sort_str_arr(char **arr, size_t n)
{
    qsort(arr, n, sizeof(char *), compare_str);
}

#endif
