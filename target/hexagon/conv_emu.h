/*
 *  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef HEXAGON_CONV_EMU_H
#define HEXAGON_CONV_EMU_H

extern uint64_t conv_sf_to_8u(float in, float_status *fp_status);
extern uint32_t conv_sf_to_4u(float in, float_status *fp_status);
extern int64_t conv_sf_to_8s(float in, float_status *fp_status);
extern int32_t conv_sf_to_4s(float in, float_status *fp_status);

extern uint64_t conv_df_to_8u(double in, float_status *fp_status);
extern uint32_t conv_df_to_4u(double in, float_status *fp_status);
extern int64_t conv_df_to_8s(double in, float_status *fp_status);
extern int32_t conv_df_to_4s(double in, float_status *fp_status);

extern double conv_8u_to_df(uint64_t in, float_status *fp_status);
extern double conv_4u_to_df(uint32_t in, float_status *fp_status);
extern double conv_8s_to_df(int64_t in, float_status *fp_status);
extern double conv_4s_to_df(int32_t in, float_status *fp_status);

extern float conv_8u_to_sf(uint64_t in, float_status *fp_status);
extern float conv_4u_to_sf(uint32_t in, float_status *fp_status);
extern float conv_8s_to_sf(int64_t in, float_status *fp_status);
extern float conv_4s_to_sf(int32_t in, float_status *fp_status);

extern float conv_df_to_sf(double in, float_status *fp_status);

static inline double conv_sf_to_df(float in, float_status *fp_status)
{
    return in;
}

#endif
