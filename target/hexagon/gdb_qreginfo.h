/*
 *  Copyright(c) 2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef HEXAGON_GDB_QREGINFO_H
#define HEXAGON_GDB_QREGINFO_H

const char * const hexagon_qreg_descs[] = {
    "name:r0;alt-name:r00;bitsize:32;offset=0;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:0;generic:r00;",
    "name:r1;alt-name:r01;bitsize:32;offset=4;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:1;generic:r01;",
    "name:r2;alt-name:r02;bitsize:32;offset=8;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:2;generic:r02;",
    "name:r3;alt-name:r03;bitsize:32;offset=12;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:3;generic:r03;",
    "name:r4;alt-name:r04;bitsize:32;offset=16;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:4;generic:r04;",
    "name:r5;alt-name:r05;bitsize:32;offset=20;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:5;generic:r05;",
    "name:r6;alt-name:r06;bitsize:32;offset=24;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:6;generic:r06;",
    "name:r7;alt-name:r07;bitsize:32;offset=28;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:7;generic:r07;",
    "name:r8;alt-name:r08;bitsize:32;offset=32;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:8;generic:r08;",
    "name:r9;alt-name:r09;bitsize:32;offset=36;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:9;generic:r09;",
    "name:r10;alt-name:;bitsize:32;offset=40;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:10;generic:;",
    "name:r11;alt-name:;bitsize:32;offset=44;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:11;generic:;",
    "name:r12;alt-name:;bitsize:32;offset=48;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:12;generic:;",
    "name:r13;alt-name:;bitsize:32;offset=52;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:13;generic:;",
    "name:r14;alt-name:;bitsize:32;offset=56;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:14;generic:;",
    "name:r15;alt-name:;bitsize:32;offset=60;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:15;generic:;",
    "name:r16;alt-name:;bitsize:32;offset=64;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:16;generic:;",
    "name:r17;alt-name:;bitsize:32;offset=68;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:17;generic:;",
    "name:r18;alt-name:;bitsize:32;offset=72;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:18;generic:;",
    "name:r19;alt-name:;bitsize:32;offset=76;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:19;generic:;",
    "name:r20;alt-name:;bitsize:32;offset=80;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:20;generic:;",
    "name:r21;alt-name:;bitsize:32;offset=84;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:21;generic:;",
    "name:r22;alt-name:;bitsize:32;offset=88;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:22;generic:;",
    "name:r23;alt-name:;bitsize:32;offset=92;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:23;generic:;",
    "name:r24;alt-name:;bitsize:32;offset=96;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:24;generic:;",
    "name:r25;alt-name:;bitsize:32;offset=100;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:25;generic:;",
    "name:r26;alt-name:;bitsize:32;offset=104;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:26;generic:;",
    "name:r27;alt-name:;bitsize:32;offset=108;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:27;generic:;",
    "name:r28;alt-name:;bitsize:32;offset=112;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:28;generic:;",
    "name:r29;alt-name:sp;bitsize:32;offset=116;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:29;generic:sp;",
    "name:r30;alt-name:fp;bitsize:32;offset=120;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:30;generic:fp;",
    "name:r31;alt-name:ra;bitsize:32;offset=124;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:31;generic:ra;",
    "name:sa0;alt-name:;bitsize:32;offset=128;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:32;generic:;",
    "name:lc0;alt-name:;bitsize:32;offset=132;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:33;generic:;",
    "name:sa1;alt-name:;bitsize:32;offset=136;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:34;generic:;",
    "name:lc1;alt-name:;bitsize:32;offset=140;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:35;generic:;",
    "name:p3_0;alt-name:;bitsize:32;offset=144;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:36;generic:;",
    "name:c5;alt-name:;bitsize:32;offset=148;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:37;generic:;",
    "name:m0;alt-name:;bitsize:32;offset=152;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:38;generic:;",
    "name:m1;alt-name:;bitsize:32;offset=156;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:39;generic:;",
    "name:usr;alt-name:;bitsize:32;offset=160;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:40;generic:;",
    "name:pc;alt-name:pc;bitsize:32;offset=164;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:41;generic:pc;",
    "name:ugp;alt-name:;bitsize:32;offset=168;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:42;generic:;",
    "name:gp;alt-name:;bitsize:32;offset=172;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:43;generic:;",
    "name:cs0;alt-name:;bitsize:32;offset=176;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:44;generic:;",
    "name:cs1;alt-name:;bitsize:32;offset=180;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:45;generic:;",
    "name:upcyclelo;alt-name:;bitsize:32;offset=184;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:46;generic:;",
    "name:upcyclehi;alt-name:;bitsize:32;offset=188;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:47;generic:;",
    "name:framelimit;alt-name:;bitsize:32;offset=192;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:48;generic:;",
    "name:framekey;alt-name:;bitsize:32;offset=196;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:49;generic:;",
    "name:pktcountlo;alt-name:;bitsize:32;offset=200;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:50;generic:;",
    "name:pktcounthi;alt-name:;bitsize:32;offset=204;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:51;generic:;",
    "name:pkt_cnt;alt-name:;bitsize:32;offset=208;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:52;generic:;",
    "name:insn_cnt;alt-name:;bitsize:32;offset=212;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:53;generic:;",
    "name:hvx_cnt;alt-name:;bitsize:32;offset=216;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:54;generic:;",
    "name:c23;alt-name:;bitsize:32;offset=220;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:55;generic:;",
    "name:c24;alt-name:;bitsize:32;offset=224;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:56;generic:;",
    "name:c25;alt-name:;bitsize:32;offset=228;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:57;generic:;",
    "name:c26;alt-name:;bitsize:32;offset=232;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:58;generic:;",
    "name:c27;alt-name:;bitsize:32;offset=236;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:59;generic:;",
    "name:c28;alt-name:;bitsize:32;offset=240;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:60;generic:;",
    "name:c29;alt-name:;bitsize:32;offset=244;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:61;generic:;",
    "name:utimerlo;alt-name:;bitsize:32;offset=248;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:62;generic:;",
    "name:utimerhi;alt-name:;bitsize:32;offset=252;variable-size:0;encoding:uint;format:hex;set:Thread Registers;dwarf:63;generic:;",
    "name:v0;alt-name:;bitsize:1024;offset=256;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:88;generic:;",
    "name:v1;alt-name:;bitsize:1024;offset=384;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:89;generic:;",
    "name:v2;alt-name:;bitsize:1024;offset=512;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:90;generic:;",
    "name:v3;alt-name:;bitsize:1024;offset=640;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:91;generic:;",
    "name:v4;alt-name:;bitsize:1024;offset=768;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:92;generic:;",
    "name:v5;alt-name:;bitsize:1024;offset=896;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:93;generic:;",
    "name:v6;alt-name:;bitsize:1024;offset=1024;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:94;generic:;",
    "name:v7;alt-name:;bitsize:1024;offset=1152;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:95;generic:;",
    "name:v8;alt-name:;bitsize:1024;offset=1280;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:96;generic:;",
    "name:v9;alt-name:;bitsize:1024;offset=1408;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:97;generic:;",
    "name:v10;alt-name:;bitsize:1024;offset=1536;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:98;generic:;",
    "name:v11;alt-name:;bitsize:1024;offset=1664;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:99;generic:;",
    "name:v12;alt-name:;bitsize:1024;offset=1792;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:100;generic:;",
    "name:v13;alt-name:;bitsize:1024;offset=1920;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:101;generic:;",
    "name:v14;alt-name:;bitsize:1024;offset=2048;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:102;generic:;",
    "name:v15;alt-name:;bitsize:1024;offset=2176;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:103;generic:;",
    "name:v16;alt-name:;bitsize:1024;offset=2304;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:104;generic:;",
    "name:v17;alt-name:;bitsize:1024;offset=2432;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:105;generic:;",
    "name:v18;alt-name:;bitsize:1024;offset=2560;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:106;generic:;",
    "name:v19;alt-name:;bitsize:1024;offset=2688;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:107;generic:;",
    "name:v20;alt-name:;bitsize:1024;offset=2816;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:108;generic:;",
    "name:v21;alt-name:;bitsize:1024;offset=2944;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:109;generic:;",
    "name:v22;alt-name:;bitsize:1024;offset=3072;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:110;generic:;",
    "name:v23;alt-name:;bitsize:1024;offset=3200;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:111;generic:;",
    "name:v24;alt-name:;bitsize:1024;offset=3328;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:112;generic:;",
    "name:v25;alt-name:;bitsize:1024;offset=3456;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:113;generic:;",
    "name:v26;alt-name:;bitsize:1024;offset=3584;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:114;generic:;",
    "name:v27;alt-name:;bitsize:1024;offset=3712;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:115;generic:;",
    "name:v28;alt-name:;bitsize:1024;offset=3840;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:116;generic:;",
    "name:v29;alt-name:;bitsize:1024;offset=3968;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:117;generic:;",
    "name:v30;alt-name:;bitsize:1024;offset=4096;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:118;generic:;",
    "name:v31;alt-name:;bitsize:1024;offset=4224;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:119;generic:;",
    "name:q0;alt-name:;bitsize:128;offset=4352;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:120;generic:;",
    "name:q1;alt-name:;bitsize:128;offset=4368;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:121;generic:;",
    "name:q2;alt-name:;bitsize:128;offset=4384;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:122;generic:;",
    "name:q3;alt-name:;bitsize:128;offset=4400;variable-size:1;encoding:vector;format:hex;set:HVX Vector Registers;dwarf:123;generic:;",
#ifndef CONFIG_USER_ONLY
    "name:sgp0;alt-name:;bitsize:32;offset=4416;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:135;generic:;",
    "name:sgp1;alt-name:;bitsize:32;offset=4420;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:136;generic:;",
    "name:stid;alt-name:;bitsize:32;offset=4424;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:137;generic:;",
    "name:elr;alt-name:;bitsize:32;offset=4428;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:138;generic:;",
    "name:badva0;alt-name:;bitsize:32;offset=4432;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:139;generic:;",
    "name:badva1;alt-name:;bitsize:32;offset=4436;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:140;generic:;",
    "name:ssr;alt-name:;bitsize:32;offset=4440;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:141;generic:;",
    "name:ccr;alt-name:;bitsize:32;offset=4444;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:142;generic:;",
    "name:htid;alt-name:;bitsize:32;offset=4448;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:143;generic:;",
    "name:badva;alt-name:;bitsize:32;offset=4452;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:144;generic:;",
    "name:imask;alt-name:;bitsize:32;offset=4456;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:145;generic:;",
    "name:gevb;alt-name:;bitsize:32;offset=4460;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:146;generic:;",
    "name:rsv12;alt-name:;bitsize:32;offset=4464;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:147;generic:;",
    "name:rsv13;alt-name:;bitsize:32;offset=4468;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:148;generic:;",
    "name:rsv14;alt-name:;bitsize:32;offset=4472;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:149;generic:;",
    "name:rsv15;alt-name:;bitsize:32;offset=4476;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:150;generic:;",
    "name:evb;alt-name:;bitsize:32;offset=4480;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:151;generic:;",
    "name:modectl;alt-name:;bitsize:32;offset=4484;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:152;generic:;",
    "name:syscfg;alt-name:;bitsize:32;offset=4488;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:153;generic:;",
    "name:free19;alt-name:;bitsize:32;offset=4492;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:154;generic:;",
    "name:ipendad;alt-name:;bitsize:32;offset=4496;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:155;generic:;",
    "name:vid;alt-name:;bitsize:32;offset=4500;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:156;generic:;",
    "name:vid1;alt-name:;bitsize:32;offset=4504;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:157;generic:;",
    "name:bestwait;alt-name:;bitsize:32;offset=4508;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:158;generic:;",
    "name:free24;alt-name:;bitsize:32;offset=4512;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:159;generic:;",
    "name:schedcfg;alt-name:;bitsize:32;offset=4516;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:160;generic:;",
    "name:free26;alt-name:;bitsize:32;offset=4520;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:161;generic:;",
    "name:cfgbase;alt-name:;bitsize:32;offset=4524;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:162;generic:;",
    "name:diag;alt-name:;bitsize:32;offset=4528;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:163;generic:;",
    "name:rev;alt-name:;bitsize:32;offset=4532;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:164;generic:;",
    "name:pcyclelo;alt-name:;bitsize:32;offset=4536;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:165;generic:;",
    "name:pcyclehi;alt-name:;bitsize:32;offset=4540;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:166;generic:;",
    "name:isdbst;alt-name:;bitsize:32;offset=4544;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:167;generic:;",
    "name:isdbcfg0;alt-name:;bitsize:32;offset=4548;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:168;generic:;",
    "name:isdbcfg1;alt-name:;bitsize:32;offset=4552;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:169;generic:;",
    "name:livelock;alt-name:;bitsize:32;offset=4556;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:170;generic:;",
    "name:brkptpc0;alt-name:;bitsize:32;offset=4560;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:171;generic:;",
    "name:brkptccfg0;alt-name:;bitsize:32;offset=4564;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:172;generic:;",
    "name:brkptpc1;alt-name:;bitsize:32;offset=4568;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:173;generic:;",
    "name:brkptcfg1;alt-name:;bitsize:32;offset=4572;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:174;generic:;",
    "name:isdbmbxin;alt-name:;bitsize:32;offset=4576;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:175;generic:;",
    "name:isdbmbxout;alt-name:;bitsize:32;offset=4580;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:176;generic:;",
    "name:isdben;alt-name:;bitsize:32;offset=4584;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:177;generic:;",
    "name:isdbgpr;alt-name:;bitsize:32;offset=4588;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:178;generic:;",
    "name:pmucnt4;alt-name:;bitsize:32;offset=4592;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:179;generic:;",
    "name:pmucnt5;alt-name:;bitsize:32;offset=4596;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:180;generic:;",
    "name:pmucnt6;alt-name:;bitsize:32;offset=4600;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:181;generic:;",
    "name:pmucnt7;alt-name:;bitsize:32;offset=4604;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:182;generic:;",
    "name:pmucnt0;alt-name:;bitsize:32;offset=4608;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:183;generic:;",
    "name:pmucnt1;alt-name:;bitsize:32;offset=4612;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:184;generic:;",
    "name:pmucnt2;alt-name:;bitsize:32;offset=4616;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:185;generic:;",
    "name:pmucnt3;alt-name:;bitsize:32;offset=4620;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:186;generic:;",
    "name:pmuevtcfg;alt-name:;bitsize:32;offset=4624;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:187;generic:;",
    "name:pmustid0;alt-name:;bitsize:32;offset=4628;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:188;generic:;",
    "name:pmuevtcfg1;alt-name:;bitsize:32;offset=4632;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:189;generic:;",
    "name:pmustid1;alt-name:;bitsize:32;offset=4636;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:190;generic:;",
    "name:timerlo;alt-name:;bitsize:32;offset=4640;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:191;generic:;",
    "name:timerhi;alt-name:;bitsize:32;offset=4644;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:192;generic:;",
    "name:pmucfg;alt-name:;bitsize:32;offset=4648;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:193;generic:;",
    "name:rsv59;alt-name:;bitsize:32;offset=4652;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:194;generic:;",
    "name:rsv60;alt-name:;bitsize:32;offset=4656;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:195;generic:;",
    "name:rsv61;alt-name:;bitsize:32;offset=4660;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:196;generic:;",
    "name:rsv62;alt-name:;bitsize:32;offset=4664;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:197;generic:;",
    "name:rsv63;alt-name:;bitsize:32;offset=4668;variable-size:0;encoding:uint;format:hex;set:System Registers;dwarf:198;generic:;",
    "name:g0;alt-name:;bitsize:32;offset=4672;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:179;generic:;",
    "name:g1;alt-name:;bitsize:32;offset=4676;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:180;generic:;",
    "name:g2;alt-name:;bitsize:32;offset=4680;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:181;generic:;",
    "name:g3;alt-name:;bitsize:32;offset=4684;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:182;generic:;",
    "name:rsv4;alt-name:;bitsize:32;offset=4688;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:183;generic:;",
    "name:rsv5;alt-name:;bitsize:32;offset=4692;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:184;generic:;",
    "name:rsv6;alt-name:;bitsize:32;offset=4696;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:185;generic:;",
    "name:rsv7;alt-name:;bitsize:32;offset=4700;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:186;generic:;",
    "name:rsv8;alt-name:;bitsize:32;offset=4704;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:187;generic:;",
    "name:rsv9;alt-name:;bitsize:32;offset=4708;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:188;generic:;",
    "name:rsv10;alt-name:;bitsize:32;offset=4712;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:189;generic:;",
    "name:rsv11;alt-name:;bitsize:32;offset=4716;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:190;generic:;",
    "name:rsv12;alt-name:;bitsize:32;offset=4720;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:191;generic:;",
    "name:rsv13;alt-name:;bitsize:32;offset=4724;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:192;generic:;",
    "name:rsv14;alt-name:;bitsize:32;offset=4728;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:193;generic:;",
    "name:rsv15,;alt-name:;bitsize:32;offset=4732;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:194;generic:;",
    "name:isdbmbxin;alt-name:;bitsize:32;offset=4736;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:195;generic:;",
    "name:isdbmbxout;alt-name:;bitsize:32;offset=4740;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:196;generic:;",
    "name:rsv18;alt-name:;bitsize:32;offset=4744;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:197;generic:;",
    "name:rsv19;alt-name:;bitsize:32;offset=4748;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:198;generic:;",
    "name:rsv20;alt-name:;bitsize:32;offset=4752;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:199;generic:;",
    "name:rsv21;alt-name:;bitsize:32;offset=4756;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:200;generic:;",
    "name:rsv22;alt-name:;bitsize:32;offset=4760;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:201;generic:;",
    "name:rsv23;alt-name:;bitsize:32;offset=4764;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:202;generic:;",
    "name:gpcyclelo;alt-name:;bitsize:32;offset=4768;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:203;generic:;",
    "name:gpcyclehi;alt-name:;bitsize:32;offset=4772;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:204;generic:;",
    "name:gpmucnt0;alt-name:;bitsize:32;offset=4776;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:205;generic:;",
    "name:gpmucnt1;alt-name:;bitsize:32;offset=4780;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:206;generic:;",
    "name:gpmucnt2;alt-name:;bitsize:32;offset=4784;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:207;generic:;",
    "name:gpmucnt3;alt-name:;bitsize:32;offset=4788;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:208;generic:;",
    "name:rsv30;alt-name:;bitsize:32;offset=4792;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:209;generic:;",
    "name:rsv31;alt-name:;bitsize:32;offset=4796;variable-size:0;encoding:uint;format:hex;set:Guest Registers;dwarf:210;generic:;",
#endif /* !CONFIG_USER_ONLY */
};

#endif /* HEXAGON_GDB_QREGINFO_H */
