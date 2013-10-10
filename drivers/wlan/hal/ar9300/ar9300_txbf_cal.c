/*
 * Copyright (c) 2008-2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR9300

#include    "ah.h"
#include    "ah_internal.h"
#include    "ar9300reg.h"

#ifdef ATH_SUPPORT_TxBF
#include    "ar9300_txbf.h"

#ifdef TXBF_TODO
const int pn_value[8][8] = {
    {1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1,-1,-1, 0, 0},
    {1, 1, 1,-1, 0,-1, 0,-1},
    {1, 1, 0, 0,-1, 0,-1,-1},
    {1, 1, 0,-1,-1,-1,-1, 0},
    {1, 1,-1,-1, 0, 0, 0, 0},
    {1, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0,-1,-1,-1, 0}};

COMPLEX const W3_Normal[3][3] = {
    {{128, 0}, {128,    0}, {128,    0}},
    {{128, 0}, {-64,  111}, {-64, -111}},
    {{128, 0}, {-64, -111}, {-64,  111}}};

COMPLEX const W3_Modify_2[3][3] = {
    {{91,  0}, { 91,    0}, { 91,    0}}, // modified for NESS=2 osprey 1.0
    {{128, 0}, {-64,  111}, {-64, -111}},
    {{128, 0}, {-64, -111}, {-64,  111}}};

COMPLEX const W3_Modify_1[3][3] = {
    {{128, 0}, {128,   0}, {128,    0}}, // modified for NESS=1 osprey 1.0
    {{128, 0}, {-64, 111}, {-64, -111}},
    {{ 91, 0}, {-45, -78}, {-45,   78}}};

COMPLEX const W3_None[3][3] = {
    {{256, 0}, {  0, 0}, {  0, 0}},
    {{  0, 0}, {256, 0}, {  0, 0}},
    {{  0, 0}, {  0, 0}, {256, 0}}};

COMPLEX const W2_Normal[2][2] = {
    {{ 128, 0}, {128, 0}},
    {{-128, 0}, {128, 0}}};

COMPLEX const W2_None[2][2] = {
    {{256, 0}, {  0, 0}},
    {{  0, 0}, {256, 0}}};

COMPLEX const ZERO = {0, 0};

COMPLEX const CSD2Tx40M[2][128] = {{
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0}
  }, {
    { 128, 0}, { 118, -49}, { 91, -91}, { 49,-118},
    {0, -128}, {-49, -118}, {-91, -91}, {-118, -49},
    {-128, 0}, {-118,  49}, {-91,  91}, {-49, 118},
    {0,  128}, { 49,  118}, { 91,  91}, { 118,  49},
    { 128, 0}, { 118, -49}, { 91, -91}, { 49,-118},
    {0, -128}, {-49, -118}, {-91, -91}, {-118, -49},
    {-128, 0}, {-118,  49}, {-91,  91}, {-49, 118},
    {0,  128}, { 49,  118}, { 91,  91}, { 118,  49},
    { 128, 0}, { 118, -49}, { 91, -91}, { 49,-118},
    {0, -128}, {-49, -118}, {-91, -91}, {-118, -49},
    {-128, 0}, {-118,  49}, {-91,  91}, {-49, 118},
    {0,  128}, { 49,  118}, { 91,  91}, { 118,  49},
    { 128, 0}, { 118, -49}, { 91, -91}, { 49,-118},
    {0, -128}, {-49, -118}, {-91, -91}, {-118, -49},
    {-128, 0}, {-118,  49}, {-91,  91}, {-49, 118},
    {0,  128}, { 49,  118}, { 91,  91}, { 118,  49},
    { 128, 0}, { 118, -49}, { 91, -91}, { 49,-118},
    {0, -128}, {-49, -118}, {-91, -91}, {-118, -49},
    {-128, 0}, {-118,  49}, {-91,  91}, {-49, 118},
    {0,  128}, { 49,  118}, { 91,  91}, { 118,  49},
    { 128, 0}, { 118, -49}, { 91, -91}, { 49,-118},
    {0, -128}, {-49, -118}, {-91, -91}, {-118, -49},
    {-128, 0}, {-118,  49}, {-91,  91}, {-49, 118},
    {0,  128}, { 49,  118}, { 91,  91}, { 118,  49},
    { 128, 0}, { 118, -49}, { 91, -91}, { 49,-118},
    {0, -128}, {-49, -118}, {-91, -91}, {-118, -49},
    {-128, 0}, {-118,  49}, {-91,  91}, {-49, 118},
    {0,  128}, { 49,  118}, { 91,  91}, { 118,  49},
    { 128, 0}, { 118, -49}, { 91, -91}, { 49,-118},
    {0, -128}, {-49, -118}, {-91, -91}, {-118, -49},
    {-128, 0}, {-118,  49}, {-91,  91}, {-49, 118},
    {0,  128}, { 49,  118}, { 91,  91}, { 118,  49}
}};

COMPLEX const CSD2Tx20M[2][64] = {{
    { 128, 0}, {128, 0}, {128, 0}, {128, 0},
    { 128, 0}, {128, 0}, {128, 0}, {128, 0},
    { 128, 0}, {128, 0}, {128, 0}, {128, 0},
    { 128, 0}, {128, 0}, {128, 0}, {128, 0},
    { 128, 0}, {128, 0}, {128, 0}, {128, 0},
    { 128, 0}, {128, 0}, {128, 0}, {128, 0},
    { 128, 0}, {128, 0}, {128, 0}, {128, 0},
    { 128, 0}, {128, 0}, {128, 0}, {128, 0},
    { 128, 0}, {128, 0}, {128, 0}, {128, 0},
    { 128, 0}, {128, 0}, {128, 0}, {128, 0},
    { 128, 0}, {128, 0}, {128, 0}, {128, 0},
    { 128, 0}, {128, 0}, {128, 0}, {128, 0},
    { 128, 0}, {128, 0}, {128, 0}, {128, 0},
    { 128, 0}, {128, 0}, {128, 0}, {128, 0},
    { 128, 0}, {128, 0}, {128, 0}, {128, 0},
    { 128, 0}, {128, 0}, {128, 0}, {128, 0}
  }, {
    { 128,    0}, { 118, -49}, { 91, -91}, { 49, -118},
    {   0, -128}, {-49, -118}, {-91, -91}, {-118, -49},
    {-128,    0}, {-118,  49}, {-91,  91}, {-49,  118},
    {   0,  128}, { 49,  118}, { 91,  91}, { 118,  49},
    { 128,    0}, { 118, -49}, { 91, -91}, { 49, -118},
    {   0, -128}, {-49, -118}, {-91, -91}, {-118, -49},
    {-128,    0}, {-118,  49}, {-91,  91}, {-49,  118},
    {   0,  128}, { 49,  118}, { 91,  91}, { 118,  49},
    { 128,    0}, { 118, -49}, { 91, -91}, { 49, -118},
    {   0, -128}, {-49, -118}, {-91, -91}, {-118, -49},
    {-128,    0}, {-118,  49}, {-91,  91}, {-49,  118},
    {   0,  128}, { 49,  118}, { 91,  91}, { 118,  49},
    { 128,    0}, { 118, -49}, { 91, -91}, { 49, -118},
    {   0, -128}, {-49, -118}, {-91, -91}, {-118, -49},
    {-128,    0}, {-118,  49}, {-91,  91}, {-49,  118},
    {   0,  128}, { 49,  118}, { 91,  91}, { 118,  49}
}};

COMPLEX const CSD3Tx40M[3][128] = {{
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0},
    {128,0}, {128,0}, {128,0}, {128,0}
  }, {
    { 128,    0}, { 126,  -25}, { 118,  -49}, { 106,  -71},
    {  91,  -91}, {  71, -106}, {  49, -118}, {  25, -126},
    {   0, -128}, { -25, -126}, { -49, -118}, { -71, -106},
    { -91,  -91}, {-106,  -71}, {-118,  -49}, {-126,  -25},
    {-128,    0}, {-126,   25}, {-118,   49}, {-106,   71},
    { -91,   91}, { -71,  106}, { -49,  118}, { -25,  126},
    {   0,  128}, {  25,  126}, {  49,  118}, {  71,  106},
    {  91,   91}, { 106,   71}, { 118,   49}, { 126,   25},
    { 128,    0}, { 126,  -25}, { 118,  -49}, { 106,  -71},
    {  91,  -91}, {  71, -106}, {  49, -118}, {  25, -126},
    {   0, -128}, { -25, -126}, { -49, -118}, { -71, -106},
    { -91,  -91}, {-106,  -71}, {-118,  -49}, {-126,  -25},
    {-128,    0}, {-126,   25}, {-118,   49}, {-106,   71},
    { -91,   91}, { -71,  106}, { -49,  118}, { -25,  126},
    {   0,  128}, {  25,  126}, {  49,  118}, {  71,  106},
    {  91,   91}, { 106,   71}, { 118,   49}, { 126,   25},
    { 128,    0}, { 126,  -25}, { 118,  -49}, { 106,  -71},
    {  91,  -91}, {  71, -106}, {  49, -118}, {  25, -126},
    {   0, -128}, { -25, -126}, { -49, -118}, { -71, -106},
    { -91,  -91}, {-106,  -71}, {-118,  -49}, {-126,  -25},
    {-128,    0}, {-126,   25}, {-118,   49}, {-106,   71},
    { -91,   91}, { -71,  106}, { -49,  118}, { -25,  126},
    {   0,  128}, {  25,  126}, {  49,  118}, {  71,  106},
    {  91,   91}, { 106,   71}, { 118,   49}, { 126,   25},
    { 128,    0}, { 126,  -25}, { 118,  -49}, { 106,  -71},
    {  91,  -91}, {  71, -106}, {  49, -118}, {  25, -126},
    {   0, -128}, { -25, -126}, { -49, -118}, { -71, -106},
    { -91,  -91}, {-106,  -71}, {-118,  -49}, {-126,  -25},
    {-128,    0}, {-126,   25}, {-118,   49}, {-106,   71},
    { -91,   91}, { -71,  106}, { -49,  118}, { -25,  126},
    {   0,  128}, {  25,  126}, {  49,  118}, {  71,  106},
    {  91,   91}, { 106,   71}, { 118,   49}, { 126,   25}
  }, {
    { 128,    0}, { 118,  -49}, {  91, -91}, {  49, -118},
    {   0, -128}, { -49, -118}, { -91, -91}, {-118,  -49},
    {-128,    0}, {-118,   49}, { -91,  91}, { -49,  118},
    {   0,  128}, {  49,  118}, {  91,  91}, { 118,   49},
    { 128,    0}, { 118,  -49}, {  91, -91}, {  49, -118},
    {   0, -128}, { -49, -118}, { -91, -91}, {-118,  -49},
    {-128,    0}, {-118,   49}, { -91,  91}, { -49,  118},
    {   0,  128}, {  49,  118}, {  91,  91}, { 118,   49},
    { 128,    0}, { 118,  -49}, {  91, -91}, {  49, -118},
    {   0, -128}, { -49, -118}, { -91, -91}, {-118,  -49},
    {-128,    0}, {-118,   49}, { -91,  91}, { -49,  118},
    {   0,  128}, {  49,  118}, {  91,  91}, { 118,   49},
    { 128,    0}, { 118,  -49}, {  91, -91}, {  49, -118},
    {   0, -128}, { -49, -118}, { -91, -91}, {-118,  -49},
    {-128,    0}, {-118,   49}, { -91,  91}, { -49,  118},
    {   0,  128}, {  49,  118}, {  91,  91}, { 118,   49},
    { 128,    0}, { 118,  -49}, {  91, -91}, {  49, -118},
    {   0, -128}, { -49, -118}, { -91, -91}, {-118,  -49},
    {-128,    0}, {-118,   49}, { -91,  91}, { -49,  118},
    {   0,  128}, {  49,  118}, {  91,  91}, { 118,   49},
    { 128,    0}, { 118,  -49}, {  91, -91}, {  49, -118},
    {   0, -128}, { -49, -118}, { -91, -91}, {-118,  -49},
    {-128,    0}, {-118,   49}, { -91,  91}, { -49,  118},
    {   0,  128}, {  49,  118}, {  91,  91}, { 118,   49},
    { 128,    0}, { 118,  -49}, {  91, -91}, {  49, -118},
    {   0, -128}, { -49, -118}, { -91, -91}, {-118,  -49},
    {-128,    0}, {-118,   49}, { -91,  91}, { -49,  118},
    {   0,  128}, {  49,  118}, {  91,  91}, { 118,   49},
    { 128,    0}, { 118,  -49}, {  91, -91}, {  49, -118},
    {   0, -128}, { -49, -118}, { -91, -91}, {-118,  -49},
    {-128,    0}, {-118,   49}, { -91,  91}, { -49,  118},
    {   0,  128}, {  49,  118}, {  91,  91}, { 118,   49}
}};

COMPLEX const CSD3Tx20M[3][64] = {{
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0},
    {128, 0}, {128, 0}, {128, 0}, {128, 0}
  }, {
    { 128,    0}, { 126,  -25}, { 118,  -49}, { 106,  -71},
    {  91,  -91}, {  71, -106}, {  49, -118}, {  25, -126},
    {   0, -128}, { -25, -126}, { -49, -118}, { -71, -106},
    { -91,  -91}, {-106,  -71}, {-118,  -49}, {-126,  -25},
    {-128,    0}, {-126,   25}, {-118,   49}, {-106,   71},
    { -91,   91}, { -71,  106}, { -49,  118}, { -25,  126},
    {   0,  128}, {  25,  126}, {  49,  118}, {  71,  106},
    {  91,   91}, { 106,   71}, { 118,   49}, { 126,   25},
    { 128,    0}, { 126,  -25}, { 118,  -49}, { 106,  -71},
    {  91,  -91}, {  71, -106}, {  49, -118}, {  25, -126},
    {   0, -128}, { -25, -126}, { -49, -118}, { -71, -106},
    { -91,  -91}, {-106,  -71}, {-118,  -49}, {-126,  -25},
    {-128,    0}, {-126,   25}, {-118,   49}, {-106,   71},
    { -91,   91}, { -71,  106}, { -49,  118}, { -25,  126},
    {   0,  128}, {  25,  126}, {  49,  118}, {  71,  106},
    {  91,   91}, { 106,   71}, { 118,   49}, { 126,   25}
  }, {
    { 128,    0}, { 118,  -49}, { 91, -91}, {  49, -118},
    {   0, -128}, { -49, -118}, {-91, -91}, {-118,  -49},
    {-128,    0}, {-118,   49}, {-91,  91}, { -49,  118},
    {   0,  128}, {  49,  118}, { 91,  91}, { 118,   49},
    { 128,    0}, { 118,  -49}, { 91, -91}, {  49, -118},
    {   0, -128}, { -49, -118}, {-91, -91}, {-118,  -49},
    {-128,    0}, {-118,   49}, {-91,  91}, { -49,  118},
    {   0,  128}, {  49,  118}, { 91,  91}, { 118,   49},
    { 128,    0}, { 118,  -49}, { 91, -91}, {  49, -118},
    {   0, -128}, { -49, -118}, {-91, -91}, {-118,  -49},
    {-128,    0}, {-118,   49}, {-91,  91}, { -49,  118},
    {   0,  128}, {  49,  118}, { 91,  91}, { 118,   49},
    { 128,    0}, { 118,  -49}, { 91, -91}, {  49, -118},
    {   0, -128}, { -49, -118}, {-91, -91}, {-118,  -49},
    {-128,    0}, {-118,   49}, {-91,  91}, { -49,  118},
    {   0,  128}, {  49,  118}, { 91,  91}, { 118,   49}
}};

const char sin_table[64] = {
    127,  18, 90,  -91, 120, -44,  37, -122,
    127, -14, 65, -110, 106, -72,   6, -128,
    127,   2, 78, -101, 114, -58,  22, -126,
    125, -29, 51, -117,  96, -85, -10, -128,
    127,  10, 85,  -96, 117, -51,  29, -125,
    126, -22, 58, -114, 101, -78,  -2, -128,
    127,  -6, 72, -106, 110, -65,  14, -127,
    122, -37, 44, -120,  91, -90, -18, -127};

const char cos_table[64] = {
    -18, 127,  91,  90,  44, 120, 122,  37,
     14, 127, 110,  65,  72, 106, 127,   6,
     -2, 127, 101,  78,  58, 114, 126,  22,
     29, 125, 117,  51,  85,  96, 127, -10,
    -10, 127,  96,  85,  51, 117, 125,  29,
     22, 126, 114,  58,  78, 101, 127,  -2,
      6, 127, 106,  72,  65, 110, 127,  14,
     37, 122, 120,  44,  90,  91, 127, -18};

const char PH_LUT[6] = {16, 9, 5, 3, 1, 1}; //LUT for atan(2^-k)


COMPLEX Complex_saturate(COMPLEX input, int upper_bound, int low_bound)
{
    if (input.real > upper_bound) {
        input.real = upper_bound;
    }
    if (input.real < low_bound) {
        input.real = low_bound;
    }
    if (input.imag > upper_bound) {
        input.imag = upper_bound;
    }
    if (input.imag < low_bound) {
        input.imag = low_bound;
    }
    return input;
}

int Int_saturate(int input, int upper_bound, int low_bound)
{

    if (input > upper_bound) {
        input = upper_bound;
    }
    if (input < low_bound) {
        input = low_bound;
    }
    return input;
}

int Int_Div_int_Floor(int input, int number)
{
    int output;

    if (number == 0) {
        return (input * 10);  // temp for test.
    }
    output = input / number;

    if (input < 0) {
        if ((output*number) != input) {
            output -= 1;
        }
    }

    return output;
}

COMPLEX Complex_Div_int_Floor(COMPLEX input, int number)
{
    COMPLEX output;

    output.real = Int_Div_int_Floor(input.real, number);
    output.imag = Int_Div_int_Floor(input.imag, number);

    return output;
}

COMPLEX Complex_Add(COMPLEX summand, COMPLEX addend)
{
    COMPLEX result;

    result.real = summand.real + addend.real;
    result.imag = summand.imag + addend.imag;

    return result;
}

COMPLEX Complex_Multiply(COMPLEX multiplier, COMPLEX multiplicant)
{
    COMPLEX result;

    result.real =
        multiplier.real * multiplicant.real -
        multiplier.imag * multiplicant.imag;
    result.imag =
        multiplier.real * multiplicant.imag +
        multiplier.imag * multiplicant.real;

    return result;
}

int Find_mag_approx(COMPLEX input)
{
    int result;
    int abs_i, abs_q, X, Y;

    abs_i = abs(input.real);
    abs_q = abs(input.imag);

    if (abs_i > abs_q) {
        X = abs_i;
    } else {
        X = abs_q;
    }
    if (abs_i > abs_q) {
        Y = abs_q;
    } else {
        Y = abs_i;
    }

    result = X;

    result = X
      - Int_Div_int_Floor(X, 32)
      + Int_Div_int_Floor(Y, 8)
      + Int_Div_int_Floor(Y,4);

    return result;
}

COMPLEX Complex_Div_Fixpt(COMPLEX numerator, COMPLEX denominator)
{
    COMPLEX result;
    int n1, d1, n2, d2;
    int div;
    int mag_num, mag_den, maxmag;

    mag_num = Find_mag_approx(numerator);
    mag_den = Find_mag_approx(denominator);

    numerator.real *= (1 << 9);
    numerator.imag *= (1 << 9);

    if (mag_num > mag_den) {
        maxmag = mag_num;
    } else {
        maxmag = mag_den;
    }
    numerator = Complex_Div_int_Floor(numerator, maxmag);

    denominator.real *= (1 << 9);
    denominator.imag *= (1 << 9);
    denominator = Complex_Div_int_Floor(denominator, maxmag);

    n1 = numerator.real;
    n2 = numerator.imag;
    d1 = denominator.real;
    d2 = denominator.imag;

    div = d1 * d1 + d2 * d2;
    if (div == 0) {
        div = 0;
        result.real = result.imag = (1 << 30) - 1; //force to a maximum value
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "==>%s:divide by 0\n", __func__);
    } else {
        // result.real=((n1*d1+n2*d2)*(1<<10))/div;
        result.real =
            Int_Div_int_Floor(((n1 * d1 + n2 * d2) * (1 << 10)), div);
        // result.imag=(((-n1)*d2+n2*d1)*(1<<10))/div;
        result.imag =
            Int_Div_int_Floor((((-n1) * d2 + n2 * d1) * (1 << 10)), div);
    }
    return result;
}

void
Remove_3Walsh(
    int NRx,
    int NTx,
    int k,
    COMPLEX (*H)[3][114],
    COMPLEX const (*W3)[3])
{
    COMPLEX Htmp[3][3];
    int NrIdx, NcIdx, i;

    OS_MEMZERO(Htmp, sizeof(Htmp));

    for (NrIdx = 0; NrIdx < NRx; NrIdx++) {
        for (NcIdx = 0; NcIdx < NTx; NcIdx++) {
            for (i = 0; i < NTx; i++) {
                Htmp[NrIdx][NcIdx] = Complex_Add(
                    Htmp[NrIdx][NcIdx],
                    Complex_Multiply(H[NrIdx][i][k], W3[i][NcIdx]));
            }
        }
    }
    for (NrIdx = 0; NrIdx < NRx; NrIdx++) {
        for (NcIdx = 0; NcIdx < NTx; NcIdx++) {
            H[NrIdx][NcIdx][k].real = Htmp[NrIdx][NcIdx].real;
            H[NrIdx][NcIdx][k].imag = Htmp[NrIdx][NcIdx].imag;
        }
    }
}

void
Remove_2Walsh(
    int NRx,
    int NTx,
    int k,
    COMPLEX (*H)[3][114],
    COMPLEX const (*W2)[2])
{
    COMPLEX Htmp[3][3];
    int NrIdx, NcIdx, i;

    OS_MEMZERO(Htmp, sizeof(Htmp));

    for (NrIdx = 0; NrIdx < NRx; NrIdx++) {
        for (NcIdx = 0; NcIdx < NTx; NcIdx++) {
            for (i = 0; i < NTx; i++) {
                Htmp[NrIdx][NcIdx] = Complex_Add(
                    Htmp[NrIdx][NcIdx],
                    Complex_Multiply(H[NrIdx][i][k], W2[i][NcIdx]));
            }
        }
    }
    for (NrIdx = 0; NrIdx < NRx; NrIdx++) {
        for (NcIdx = 0; NcIdx < NTx; NcIdx++) {
            H[NrIdx][NcIdx][k].real = Htmp[NrIdx][NcIdx].real;
            H[NrIdx][NcIdx][k].imag = Htmp[NrIdx][NcIdx].imag;
        }
    }
}

// RC interpolation
HAL_BOOL
RC_interp(COMPLEX (*Ka)[Tone_40M], int8_t Ntone)

{
    int8_t i, j, nullcnt, validcnt;
    int8_t valid[114], null[114], valididx, nullidx, previdx;
    COMPLEX temp;

    for (i = 0; i < 3; i++) {
        nullcnt = validcnt = 0;
        for (j = 0; j < Ntone; j++) {
            if ((Ka[i][j].real == 0) && (Ka[i][j].imag == 0)) {
                null[nullcnt] = j;
                nullcnt++;
            } else {
                valid[validcnt] = j;
                validcnt++;
            }
        }
        if (nullcnt != 0) {
            if (validcnt == 0) {
                return AH_FALSE;
            } else {
                valididx = nullidx = 0;
                for (j = 0; j < Ntone; j++) {
                    if (null[nullidx] == j) {
                        if (valididx < validcnt) {
                            temp.real = valid[valididx] - j;
                            temp.imag = 0;
                            previdx = j - 1;
                            if (previdx < 0) {
                                previdx = 0;
                            }
                            Ka[i][j] = Complex_Add(
                                Complex_Multiply(Ka[i][previdx], temp),
                                Ka[i][valid[valididx]]);
                            Ka[i][j] = Complex_Div_int_Floor(
                                Ka[i][j], (valid[valididx] - j + 1));
                        } else {
                            Ka[i][j] = Ka[i][valid[valididx-1]];
                        }

                        nullidx++;
                        if (nullidx == nullcnt) {
                            break;    // no next null
                        }
                    } else {
                        valididx++;
                    }
                }
            }
        }
    }
    return AH_TRUE;
}

// converlution filter
void
conv_fir(struct ath_hal *ah, COMPLEX *In, int8_t length)
{
    int8_t const filter_coef[7] = {-8, 16, 36, 40, 36, 16, -8};
    int i,j;
    COMPLEX *in_tmp;

    in_tmp = ath_hal_malloc(ah->ah_osdev, (length + 12) * sizeof(COMPLEX));

    for (i = 0; i < length + 12; i++) {
        if (i < 6) {
            in_tmp[i] = ZERO;
        } else if ((i >= 6) && (i < 6 + length)) {
            in_tmp[i] = In[i - 6];
        } else if (i >= 6 + length) {
            in_tmp[i] = ZERO;
        }
    }
    for (i = 0; i < length + 6; i++) {
        In[i] = ZERO;
        for (j = 0; j < 7; j++) {
            In[i].real += filter_coef[6 - j] * in_tmp[i + j].real;
            In[i].imag += filter_coef[6 - j] * in_tmp[i + j].imag;
        }
    }
    ath_hal_free(ah, in_tmp);
}

void
RC_smoothing(struct ath_hal *ah, COMPLEX (*Ka)[Tone_40M], int8_t BW)
// apply channel filtering to radio coefficients
{
    int8_t NTone;
    int i, j;
    COMPLEX *RC_tmp, *RC_tmp1;

    if (BW == BW_40M) { //40M
        NTone = Tone_40M;
    } else {
        NTone = Tone_20M;
    }

    RC_tmp = ath_hal_malloc(ah->ah_osdev, NTone * sizeof(COMPLEX));
    RC_tmp1 = ath_hal_malloc(ah->ah_osdev, (NTone + 9 + 6) * sizeof(COMPLEX));

    for (i = 1; i < 3; i++) {
        for (j = 0; j < NTone; j++) {
            RC_tmp[(j + NTone / 2) % NTone] = Ka[i][j];
        }
        if (BW == BW_40M) {      // 40M
            /*
             * RC_tmp = [
             *     RC_tmp([4 3 2])
             *     RC_tmp(1:57)
             *     RC_tmp(57)
             *     RC_tmp(57)
             *     RC_tmp(57)
             *     RC_tmp(58:114)
             *     RC_tmp([113 112 111])];
             */
            for (j = 0; j < NTone + 9; j++) {
                if (j < 3) {
                    RC_tmp1[j] = RC_tmp[3 - j];
                } else if ((j >= 3) && (j < 60)) {
                    RC_tmp1[j] = RC_tmp[j - 3];
                } else if ((j >= 60) && (j < 63)) {
                    RC_tmp1[j] = RC_tmp[56];
                } else if ((j >= 63) && (j < 120)) {
                    RC_tmp1[j] = RC_tmp[j - 6];
                } else if ((j >= 120) && (j < 123)) {
                    RC_tmp1[j] = RC_tmp[232 - j];
                }
            }
            conv_fir(ah, RC_tmp1, 123);
            for (j = 0; j < NTone; j++) {
                if (j <= 56) {
                    RC_tmp[j] = RC_tmp1[j + 6];
                } else {
                    RC_tmp[j] = RC_tmp1[j + 9];
                }
            }
        } else {  //20M
            /*
             * [RC_tmp([4 3 2])
             *  RC_tmp(1:28)
             *  RC_tmp(28)
             *  RC_tmp(29:56)
             *  RC_tmp([55 54 53])];
             */
            for (j = 0; j < NTone + 7; j++) {
                if (j < 3) {
                    RC_tmp1[j] = RC_tmp[3 - j];
                } else if ((j >= 3) && (j < 31)) {
                    RC_tmp1[j] = RC_tmp[j - 3];
                } else if (j == 31) {
                    RC_tmp1[j] = RC_tmp[27];
                } else if ((j > 31) && (j < 60)) {
                    RC_tmp1[j] = RC_tmp[j - 4];
                } else if (j >= 60) {
                    RC_tmp1[j] = RC_tmp[114 - j];
                }
            }
            conv_fir(ah, RC_tmp1, 63);
            for (j = 0; j < NTone; j++) {
                if (j < 28) {
                    RC_tmp[j] = RC_tmp1[j + 6];
                } else {
                    RC_tmp[j] = RC_tmp1[j + 7];
                }
            }
        }
        for (j = 0; j < NTone; j++) {
            Ka[i][j] = Complex_saturate(
                Complex_Div_int_Floor(RC_tmp[(j + NTone / 2) % NTone], 128),
                ((1 << 7) - 1),
                -(1 << 7));
        }
    }
    ath_hal_free(ah, RC_tmp1);
    ath_hal_free(ah, RC_tmp);
}

/*  A: BFer
*  B: BFee
*  ______________            ______________
* | (A)          |          |          (B) |
* | Ka --> Atx --|->  Hab --|-> Brx        |
* |              |          |              |
* |        Arx <-|--  Hba <-|-- Btx <-- Kb |
* |______________|          |______________|
*/
HAL_BOOL
Ka_Caculation(
    struct ath_hal *ah,
    int8_t Ntx_A,
    int8_t Nrx_A,
    int8_t Ntx_B,
    int8_t Nrx_B,
    COMPLEX (*Hba_eff)[3][Tone_40M],
    COMPLEX (*H_eff_quan)[3][Tone_40M],
    u_int8_t *M_H,
    COMPLEX (*Ka)[Tone_40M],
    u_int8_t NESSA,
    u_int8_t NESSB,
    int8_t BW)
{
    int8_t        i, j, k, NrIdx, NcIdx, minB, NTone;
    COMPLEX       Htmp[3][3], CSDtmp[3][3];
    int8_t        USED_BINS[Tone_40M];
    COMPLEX       (*Hab_eff)[3][Tone_40M];
    COMPLEX       a[6];
    HAL_BOOL      a_valid[6];
    u_int8_t      ka1_valid, ka2_valid;
    COMPLEX       Ka_1, Ka_2;
    int           mag;
    COMPLEX const (*W3A)[3], (*W3B)[3];
    COMPLEX const (*W2A)[2], (*W2B)[2];
    HAL_BOOL      status = AH_TRUE;

    Hab_eff = ath_hal_malloc(ah->ah_osdev, 3 * 3 * Tone_40M * sizeof(COMPLEX));
    if (BW == BW_40M) {//40M
        j = -58;
        NTone = Tone_40M;
        for (i = 0; i < NTone; i++) {
            //use_bin=mod(j,128)+1;
            if (j < 0) {
                USED_BINS[i] = j + 128;
            } else {
                USED_BINS[i] = j;
            }
            j++;
            if (j == -1) {
                j = 2;
            }
        }
    } else {  // 20M
        j = -28;
        NTone = Tone_20M;
        for (i = 0; i < NTone; i++) {
            //use_bin=mod(j,64)+1;
            if (j < 0) {
                USED_BINS[i] = j + 64;
            } else {
                USED_BINS[i] = j;
            }
            j++;
            if (j == 0) {
                j = 1;
            }
        }
    }
    OS_MEMZERO(Hab_eff, 3 * 3 * Tone_40M * sizeof(COMPLEX));

    if (Ntx_B > Nrx_B) {
        minB = Nrx_B;
    } else {
        minB = Ntx_B;
    }

    if (NESSA == 0) {
        W3A = W3_None;    // no walsh
        W2A = W2_None;
    } else {
        W2A = W2_Normal;
        W3A = W3_Normal;  // should use corrected when osprey 1.0 and NESS=1
    }

    if (NESSB == 0) {
        W3B = W3_None;    // no walsh
        W2B = W2_None;
    } else {
        W2B = W2_Normal;
        W3B = W3_Normal;  // should use corrected when osprey 1.0 and NESS=1
    }

#if debugKA
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "==>%s:M_H:", __func__);

    for (k = 0; k < NTone; k++) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, " %d,", M_H[k]);
    }
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
#endif

    for (k = 0; k < NTone; k++) {
        // CSI2 to H
        for (j = 0; j < NUM_ITER; j++) {
            for (NrIdx = 0; NrIdx < Nrx_B; NrIdx++) {
                for (NcIdx = 0; NcIdx < Ntx_A; NcIdx++) {
                    Hab_eff[NrIdx][NcIdx][k].real +=
                        pn_value[M_H[k]][j] *
                        (H_eff_quan[NrIdx][NcIdx][k].real >> (j + 1));
                    Hab_eff[NrIdx][NcIdx][k].imag +=
                        pn_value[M_H[k]][j] *
                        (H_eff_quan[NrIdx][NcIdx][k].imag >> (j + 1));
                }
            }
        }

#if debugKa
        if (k == 0) {
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "==>%s: original Hab_eff for tone 0:", __func__);
            for (NrIdx = 0; NrIdx < Nrx_B; NrIdx++) {
                for (NcIdx = 0; NcIdx < Ntx_A; NcIdx++) {
                    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                        " %d, %d;",
                        Hab_eff[NrIdx][NcIdx][k].real,
                        Hab_eff[NrIdx][NcIdx][k].imag);
                }
            }
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
        }
#endif

        //remove walsh_A of Hab_eff
        if (Ntx_A == 3) {
            Remove_3Walsh(Nrx_B, Ntx_A,k, Hab_eff,W3A);
        } else if (Ntx_A == 2) {
            Remove_2Walsh(Nrx_B, Ntx_A,k, Hab_eff,W2A);
        }
#if debugKa
        if (k == 0) {
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "==>%s:Hab_eff for tone 0 after remove Walsh:", __func__);
            for (NrIdx = 0; NrIdx < Nrx_B; NrIdx++) {
                for (NcIdx = 0; NcIdx < Ntx_A; NcIdx++) {
                    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                        " %d, %d;",
                        Hab_eff[NrIdx][NcIdx][k].real,
                        Hab_eff[NrIdx][NcIdx][k].imag);
                }
            }
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
        }
#endif

        //remove csd of Hab_eff
        OS_MEMZERO(Htmp, sizeof(Htmp));
        OS_MEMZERO(CSDtmp, sizeof(CSDtmp));//diag(csd2(:,USED_BINS(k)));
        if (NESSA != 0) { // not regular sounding
            if (BW == BW_40M) { //40M
                if (Ntx_A == 3) {
                    for (i = 0; i < Ntx_A; i++) {
                        CSDtmp[i][i].real = CSD3Tx40M[i][USED_BINS[k]].real;
                        CSDtmp[i][i].imag = CSD3Tx40M[i][USED_BINS[k]].imag;
                    }
                } else if (Ntx_A == 2) {
                    for (i = 0; i < Ntx_A; i++) {
                        CSDtmp[i][i].real = CSD2Tx40M[i][USED_BINS[k]].real;
                        CSDtmp[i][i].imag = CSD2Tx40M[i][USED_BINS[k]].imag;
                    }
                }
            } else {   // 20M
                if (Ntx_A == 3) {
                    for (i = 0; i < Ntx_A; i++) {
                        CSDtmp[i][i].real = CSD3Tx20M[i][USED_BINS[k]].real;
                        CSDtmp[i][i].imag = CSD3Tx20M[i][USED_BINS[k]].imag;
                    }
                } else if (Ntx_A == 2) {
                    for (i = 0; i < Ntx_A; i++) {
                        CSDtmp[i][i].real = CSD2Tx20M[i][USED_BINS[k]].real;
                        CSDtmp[i][i].imag = CSD2Tx20M[i][USED_BINS[k]].imag;
                    }
                }
            }
        } else {
            for (i = 0; i < Ntx_A; i++) {
                CSDtmp[i][i].real = 128;
            }
        }

        for (NrIdx = 0; NrIdx < Nrx_B; NrIdx++) {
            for (NcIdx = 0; NcIdx < Ntx_A; NcIdx++) {
                for (i = 0; i < Ntx_A; i++) {
                    Htmp[NrIdx][NcIdx] =
                        Complex_Add(Htmp[NrIdx][NcIdx],
                            Complex_Multiply(
                                Hab_eff[NrIdx][i][k], CSDtmp[i][NcIdx]));
                }
                Hab_eff[NrIdx][NcIdx][k] =
                    Complex_Div_int_Floor(Htmp[NrIdx][NcIdx], (1 << 13));
            }
        }
#if debugKa
        if (k == 0) {
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "==>%s:Hab_eff for tone 0 after remove CSD:",__func__);
            for (NrIdx = 0; NrIdx < Nrx_B; NrIdx++) {
                for (NcIdx = 0; NcIdx < Ntx_A; NcIdx++) {
                    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                        " %d, %d;",
                        Hab_eff[NrIdx][NcIdx][k].real,
                        Hab_eff[NrIdx][NcIdx][k].imag);
                    }
            }
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
        }

        if (k == 0) {
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "==>%s:original Hba_eff for tone 0:", __func__);
            for (NrIdx = 0; NrIdx < Nrx_B; NrIdx++) {
                for (NcIdx = 0; NcIdx < Ntx_A; NcIdx++) {
                    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                        " %d, %d;",
                        Hba_eff[NrIdx][NcIdx][k].real,
                        Hba_eff[NrIdx][NcIdx][k].imag);
                    }
            }
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
        }
#endif

        if (minB == 3) {
            Remove_3Walsh(Nrx_A, Ntx_B, k, Hba_eff, W3B);
        } else if (minB == 2) {
            Remove_2Walsh(Nrx_A, Ntx_B, k, Hba_eff, W2B);
        }

        // adjust format
        if (minB != 1) {
            for (NrIdx = 0; NrIdx < Nrx_A; NrIdx++) {
                for (NcIdx = 0; NcIdx < Ntx_B; NcIdx++) {
                    Hba_eff[NrIdx][NcIdx][k] = Complex_Div_int_Floor(
                        Hba_eff[NrIdx][NcIdx][k], (1 << 6));
                }
            }
        }

#if debugKa
        if (k == 0) {
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "==>%s:Hba_eff for tone 0 after remove Walsh:", __func__);
            for (NrIdx = 0; NrIdx < Nrx_B; NrIdx++) {
                for (NcIdx = 0; NcIdx < Ntx_A; NcIdx++) {
                    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                        " %d, %d;",
                        Hba_eff[NrIdx][NcIdx][k].real,
                        Hba_eff[NrIdx][NcIdx][k].imag);
                }
            }
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
        }
#endif

        OS_MEMZERO(&Ka_1, sizeof(COMPLEX));
        OS_MEMZERO(&Ka_2, sizeof(COMPLEX));
        ka1_valid = 0;
        ka2_valid = 0;
        for (i = 0; i < minB; i++) {  // 3 set of antenna
            a[2 * i] = Complex_Div_Fixpt(
                Complex_Multiply(Hab_eff[i][1][k], Hba_eff[0][i][k]),
                Complex_Multiply(Hab_eff[i][0][k], Hba_eff[1][i][k]));
            mag = Find_mag_approx(a[2 * i]);
            a_valid[2 * i] = (mag > (rc_min)) & (mag < (rc_max));
            ka1_valid += a_valid[2 * i];

            if (Ntx_A == 3) {     //calculate Ka2
                a[2 * i + 1] = Complex_Div_Fixpt(
                    Complex_Multiply(Hab_eff[i][2][k], Hba_eff[0][i][k]),
                    Complex_Multiply(Hab_eff[i][0][k], Hba_eff[2][i][k]));
                mag = Find_mag_approx(a[2 * i + 1]);
                a_valid[2 * i + 1] = (mag > (rc_min)) & (mag < (rc_max));
                ka2_valid += a_valid[2 * i + 1];
            }
        }

#if debugKa
        if (k == 0) {
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "==>%s: original ka and valid for tone 0 :", __func__); //clhwu
            for (i = 0; i < 6; i++) {
                HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                    "ka %d,%d,valid %d;", a[i].real, a[i].imag, a_valid[i]);
            }
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
        }
#endif

        // average valid Ka
        for (i = 0; i < minB; i++) {
            if (a_valid[2 * i]) {
                Ka_1 = Complex_Add(Ka_1, a[2 * i]);
            }
            if (Ntx_A == 3) {
                if (a_valid[2 * i + 1]) {
                    Ka_2 = Complex_Add(Ka_2, a[2 * i + 1]);
                }
            }
        }
        if (ka1_valid == 0) {
            Ka_1 = ZERO;
            status = AH_FALSE;
        } else {
            Ka_1 = Complex_Div_int_Floor(Ka_1, ka1_valid);
        }
        if (Ntx_A == 3) {
            if (ka2_valid == 0) {
                Ka_2 = ZERO;
                status = AH_FALSE;
            } else {
                Ka_2 = Complex_Div_int_Floor(Ka_2, ka2_valid);
            }
        }

        Ka[0][k].real = 1024;
        Ka[1][k] = Ka_1;
        Ka[2][k] = Ka_2;

#if debugKa
        if (k == 0) {
            HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                "==>%s:Ka_1 %d %d;Ka_2 %d %d\n",
                __func__, Ka_1.real, Ka_1.imag, Ka_2.real, Ka_2.imag);
        }
#endif
        if (CAL_GAIN == 0) {
            COMPLEX cal_tmp;

            cal_tmp.real = 1 << 7;
            cal_tmp.imag = 0;

            for (i = 0; i < 3; i++) {
                mag = Find_mag_approx(Ka[i][k]);

                if (mag == 0) {
                    Ka[i][k] = ZERO;
                } else {
                    Ka[i][k] = Complex_Div_int_Floor(
                        Complex_Multiply(Ka[i][k], cal_tmp), mag);
                }
            }
        } else {
            Ka[0][k].real /= 16;
            Ka[1][k] = Complex_Div_int_Floor(Ka_1, 16);
            if (Ntx_A == 3) {
                Ka[2][k] = Complex_Div_int_Floor(Ka_2, 16);
            }
        }
        for (i = 0; i < minB; i++) { // saturation check ; check for ka_1 ka_2
            Ka[i][k] = Complex_saturate(Ka[i][k], ((1 << 7) - 1), -(1 << 7));
        }
    }

#if debugKa
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "%s==> Ka of tone 0 after calculation:", __func__);
    for (i = 0; i < 3; i++) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "%d,%d;", Ka[i][0].real, Ka[i][0].imag);
    }
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
#endif

    ath_hal_free(ah, Hab_eff);
    if (status == AH_FALSE) {
        status = RC_interp(Ka, NTone);
    }

#if debugKa
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "%s==>Ka of tone 0 after interp:", __func__);
    for (i = 0; i < 3; i++) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "%d,%d;", Ka[i][0].real, Ka[i][0].imag);
    }
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
#endif

    if (status == AH_FALSE) {
        return status;
    }

    if (Smooth) {
        RC_smoothing(ah, Ka, BW);
    }

#if debugKa
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "%s==>Ka of tone 0 after smoothing:", __func__);
    for (i = 0; i < 3; i++) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "%d,%d;", Ka[i][0].real, Ka[i][0].imag);
    }
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
#endif

    for (k = 0; k < NTone; k++) {
        for (i = 0; i < minB; i++) {      // saturation check again
            Ka[i][k] = Complex_saturate(Ka[i][k], ((1 << 7) - 1), -(1 << 7));
        }
    }

#if debugKa
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "%s==>final Ka of tone 0:", __func__);
    for (i = 0; i < 3; i++) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "%d,%d;", Ka[i][0].real, Ka[i][0].imag);
    }
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "\n");
#endif

    return  AH_TRUE;
}

void
cordic_rot(int X, int Y, int *X_out, int *phase, int8_t *ph_idx, int8_t *signx)
{
    int8_t sign;
    int phase_acc;
    u_int8_t i;
    int xtemp, ytemp;

    if (X < 0) {
        *signx = -1;
    } else {
        *signx = 1;
    }

    X = (*signx) * X;    //abs
    phase_acc = 0;
    *ph_idx = 0;
    for (i = 0; i < NUM_ITER_V; i++) {
        if (Y < 0) {
            sign = 1;
        } else {
            sign = -1; //sign of next iter.
        }
        xtemp = X;
        ytemp = Y;
        X = xtemp - sign * Int_Div_int_Floor(ytemp, (1 << i));
        Y = ytemp + sign * Int_Div_int_Floor(xtemp, (1 << i));
        phase_acc -= phase_acc-sign * PH_LUT[i];
        X = Int_saturate(X, (1 << Nb_coridc) - 1, -(1 << Nb_coridc));
        Y = Int_saturate(Y, (1 << Nb_coridc) - 1, -(1 << Nb_coridc));
        if (sign == 1) {
            *ph_idx += (1 << i);
        }
    }
    *X_out =
        Int_Div_int_Floor(X ,2) +
        Int_Div_int_Floor(X, 8) -
        Int_Div_int_Floor(X, 64);
    X = X;
    if (*signx == -1) {
        *phase = -phase_acc + (1 << (Nb_ph + 1));
    } else if (phase_acc >= 0) {
        *phase = phase_acc;
    } else {
        *phase = phase_acc + (1 << (Nb_ph + 2));
    }
}

void
compress_v(
    COMPLEX (*V)[3],
    u_int8_t Nr,
    u_int8_t Nc,
    u_int8_t *phi,
    u_int8_t *psi)
{
    int Nridx, Ncidx;
    int X, Y;
    int X_out, phase;
    int8_t ph_idx,signx;
    int phi_x, phi_y;
    COMPLEX rot;
    int i, j;
    int phi_idx = 0, psi_idx = 0;
    COMPLEX Vtemp[3][3];
    COMPLEX temp, temp1, temp2;

    for (i = 0; i < 3; i++) {
        phi[i] = 0;
        psi[i] = 0;
    }
    //rot least row to x axis
    for (Ncidx = 0; Ncidx < Nc; Ncidx++) {
        X = V[Nr-1][Ncidx].real;
        Y = V[Nr-1][Ncidx].imag;
        cordic_rot(X, Y, &X_out, &phase, &ph_idx, &signx);
        V[Nr-1][Ncidx].real = X_out;
        V[Nr-1][Ncidx].imag = 0;
        phi_x = signx*cos_table[ph_idx];
        phi_y = sin_table[ph_idx];
        phi_x = Int_saturate(phi_x, (1 << Nb_sin) - 1,-(1 << Nb_sin));
        rot.real = phi_x;
        rot.imag = -phi_y;

        //floor(V(1:end-1,col_idx)*rot*2^-Nb_sin+0.5+0.5i);
        for (i = 0; i < Nr - 1; i++) {
            V[i][Ncidx] = Complex_Multiply(V[i][Ncidx], rot);
            V[i][Ncidx].real += (1 << Nb_sin) / 2;
            V[i][Ncidx].imag += (1 << Nb_sin) / 2;
            V[i][Ncidx] = Complex_Div_int_Floor(V[i][Ncidx], 1 << Nb_sin);
            V[i][Ncidx] = Complex_saturate(
                V[i][Ncidx], (1 << (Nb_phin - 2)) - 1,-(1 << (Nb_phin - 2)));
        }
    }

    for (Ncidx = 0; Ncidx < Nc; Ncidx++) {
        // find phi
        for (Nridx = Ncidx; Nridx < Nr - 1; Nridx++) {
            X = V[Nridx][Ncidx].real;
            Y = V[Nridx][Ncidx].imag;
            cordic_rot(X, Y, &X_out, &phase, &ph_idx, &signx);
            V[Nridx][Ncidx].real = X_out;
            V[Nridx][Ncidx].imag = 0;
            V[i][Ncidx] = Complex_saturate(
                V[i][Ncidx], (1 << Nb_phin) - 1, 1 << Nb_phin);
            phi_x = signx * cos_table[ph_idx];
            phi_y = sin_table[ph_idx];
            phi_x = Int_saturate(phi_x, (1 << Nb_sin) - 1,-(1 << Nb_sin));
            rot.real = phi_x;
            rot.imag = -phi_y;

            /*
            V(row_idx,col_idx+1:end) = floor(
                V(row_idx,col_idx+1:end)*rot*2^-Nb_sin+0.5+0.5i);
             */
            for (i = Ncidx + 1; i < Nc; i++) {
                V[Nridx][i] = Complex_Multiply(V[Nridx][i], rot);
                V[Nridx][i].real += (1 << Nb_sin) / 2;
                V[Nridx][i].imag += (1 << Nb_sin) / 2;
                V[Nridx][i] = Complex_Div_int_Floor(V[Nridx][i], 1 << Nb_sin);
                V[Nridx][i] = Complex_saturate(
                    V[Nridx][i],
                    (1 << (Nb_phin - 2)) - 1,
                    -(1 << (Nb_phin - 2)));
            }
            phi[phi_idx] = phase;
            phi_idx++;
        }

        // find psi
        for (i = 0; i < Nc; i++) {
            for (j = 0; j < Nr; j++) {
                Vtemp[j][i].real = V[j][i].real;
                Vtemp[j][i].imag = V[j][i].imag;
            }
        }

        for (Nridx = Ncidx + 1; Nridx < Nr; Nridx++) {
            X = V[Ncidx][Ncidx].real;
            Y = V[Nridx][Ncidx].real;
            cordic_rot(X, Y, &X_out, &phase, &ph_idx, &signx);
            Vtemp[Ncidx][Ncidx].real = X_out;
            Vtemp[Ncidx][Ncidx].imag = 0;
            Vtemp[Nridx][Ncidx].real = 0;
            Vtemp[Nridx][Ncidx].imag = 0;
            /*
            Vtemp(col_idx,col_idx+1:end) =
                floor((V_tilde(col_idx,col_idx+1:end)
                       * signx*cos_table(ph_idx)
                       + V_tilde(row_idx,col_idx+1:end)
                       * sin_table(ph_idx))
                      * 2 ^ -Nb_sin + 0.5 + 0.5i);
             */
            temp1.real = signx * cos_table[ph_idx];
            temp1.imag = 0;
            temp2.real = sin_table[ph_idx];
            temp2.imag = 0;
            for (i = Ncidx + 1; i < Nc; i++) {
                temp = Complex_Add(
                    Complex_Multiply(V[Ncidx][i], temp1),
                    Complex_Multiply(V[Nridx][i], temp2));
                temp.real += (1 << Nb_sin) / 2;
                temp.imag += (1 << Nb_sin) / 2;
                Vtemp[Ncidx][i] = Complex_Div_int_Floor(temp, 1 << Nb_sin);
            }
            /*
            Vtemp(row_idx,col_idx+1:end) =
                floor((V_tilde(col_idx,col_idx+1:end)
                       * -sin_table(ph_idx)
                       + V_tilde(row_idx,col_idx+1:end)
                       * signx*cos_table(ph_idx))
                      * 2 ^ -Nb_sin + 0.5 + 0.5i);
             */
            temp1.real = -sin_table[ph_idx];
            temp1.imag = 0;
            temp2.real = signx * cos_table[ph_idx];
            temp2.imag = 0;
            for (i = Ncidx + 1; i < Nc; i++) {
                temp = Complex_Add(
                    Complex_Multiply(V[Ncidx][i], temp1),
                    Complex_Multiply(V[Nridx][i], temp2));
                temp.real += (1 << Nb_sin) / 2;
                temp.imag += (1 << Nb_sin) / 2;
                Vtemp[Nridx][i] = Complex_Div_int_Floor(temp, 1 << Nb_sin);
            }
            /*
            V_tmp(col_idx,col_idx+1:end) =
                Int_saturate(V_tmp(col_idx,col_idx+1:end),
                    2^(Nb_phin-2)-1,-2^(Nb_phin-2));
            V_tmp(row_idx,col_idx+1:end) =
                Int_saturate(V_tmp(row_idx,col_idx+1:end),
                    2^(Nb_phin)-1,-2^(Nb_phin));
             */
            for (i = Ncidx + 1; i < Nc; i++) {
                Vtemp[Ncidx][i] = Complex_saturate(
                    Vtemp[Ncidx][i],
                    (1 << (Nb_phin - 2)) - 1,
                    -(1 << (Nb_phin - 2)));
                Vtemp[Nridx][i] = Complex_saturate(
                    Vtemp[Nridx][i],
                    (1 << Nb_phin) - 1,
                    -(1 << Nb_phin));
            }

            for (i = 0; i < Nc; i++) {
                for (j = 0; j < Nr; j++) {
                    V[j][i].real = Vtemp[j][i].real;
                    V[j][i].imag = Vtemp[j][i].imag;
                }
            }

            if (phase > (1 << (Nb_ph + 1))) {
                phase = 0;
            } else if (phase >= (1 << Nb_ph)) {
                phase = 1 << Nb_ph;
            }
            psi[psi_idx] = phase;
            psi_idx++;
        }
    }
    if ((Nb_psi - Nb_ph) < 0) {
        for (i = 0; i < 3; i++) {
            phi[i] = Int_Div_int_Floor(phi[i], 1 << (Nb_ph - Nb_psi));
            psi[i] = Int_Div_int_Floor(psi[i], 1 << (Nb_ph - Nb_psi));
        }
    }
}

void
H_to_CSI(
    struct ath_hal *ah,
    u_int8_t Ntx,
    u_int8_t Nrx,
    COMPLEX (*H)[3][Tone_40M],
    u_int8_t *M_H,
    u_int8_t Ntone)
{
    u_int8_t i, j, k;
    u_int8_t const r_inv_quan[8] = {127, 114, 102, 91, 81, 72, 64, 57};
    int q_M_H[8];
    int max, *max_h, tmp, *m_h_linear;
    COMPLEX temp;

    max_h = ath_hal_malloc(ah->ah_osdev, Ntone * sizeof(int));
    m_h_linear = ath_hal_malloc(ah->ah_osdev, Ntone * sizeof(int));
    max = 0;
    for (k = 0; k < Ntone; k++) {
        max_h[k] = 0;
        for (j = 0; j < Ntx; j++) {
            for (i = 0; i < Nrx; i++) {      //search the max point of each tone
                tmp = H[i][j][k].real;
                if (H[i][j][k].real < 0) {
                    tmp = -H[i][j][k].real;
                }
                if (tmp>max_h[k]) {
                    max_h[k] = tmp;
                }
                tmp = H[i][j][k].imag;
                if (H[i][j][k].imag < 0) {
                    tmp = -H[i][j][k].imag;
                }
                if (tmp > max_h[k]) {
                    max_h[k] = tmp;
                }
            }
        }
        if (max_h[k] > max) {
            max = max_h[k];
        }
    }

    for (i = 0; i < 8; i++) {
        q_M_H[i] = max * r_inv_quan[i] + (1 << 6);
        q_M_H[i] = Int_Div_int_Floor(q_M_H[i], 1 << 7);
        q_M_H[i] = Int_saturate(q_M_H[i], ((1 << 9) - 1), -(1 << 9));
    }
    for (k = 0; k < Ntone; k++) {
        for (i = 0; i < 8; i++) {
            if (q_M_H[7 - i] >= max_h[k]) {
                break;
            }
        }
        if (i == 8) {
            M_H[k] = 0;
        } else {
            M_H[k] = 7 - i;
        }
        m_h_linear[k] = q_M_H[M_H[k]];
        // inverse the m_h_linear
        tmp = m_h_linear[k] / 2 + (1 << Nb_MHINV);
        m_h_linear[k] = Int_Div_int_Floor(tmp, m_h_linear[k]);
        m_h_linear[k] = Int_saturate(
            m_h_linear[k], ((1 << (Nb_MHINV - 6)) - 1),-(1 << (Nb_MHINV - 6)));
        /*
        H_quantized_FB(:,:,k) = floor(
            H_per_tone * M_H_linear_inv(k) *
            2 ^ (Nb - 1 - Nb_MHINV) + 0.5 + 0.5 * sqrt(-1))
         */
        temp.real = 32;    //2^(Nb-1-Nb_MHINV) *0.5
        temp.imag = 32;
        for (j = 0; j < Ntx; j++) {
            for (i = 0; i < Nrx; i++) {  //search the max point of each tone
                H[i][j][k].real *= m_h_linear[k];
                H[i][j][k].imag *= m_h_linear[k];
                H[i][j][k] = Complex_Add(H[i][j][k], temp);
                H[i][j][k] = Complex_Div_int_Floor(H[i][j][k], 64);
                H[i][j][k] =
                    Complex_saturate(H[i][j][k], ((1 << 7) - 1), -(1 << 7));
             }
        }
    }
    ath_hal_free(ah, max_h);
    ath_hal_free(ah, m_h_linear);
}
#endif

#endif /* ATH_SUPPORT_TxBF*/
#endif /* AH_SUPPORT_AR9300 */
