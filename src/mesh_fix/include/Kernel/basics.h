/****************************************************************************
* TMesh                                                                  *
*                                                                           *
* Consiglio Nazionale delle Ricerche                                        *
* Istituto di Matematica Applicata e Tecnologie Informatiche                *
* Sezione di Genova                                                         *
* IMATI-GE / CNR                                                            *
*                                                                           *
* Authors: Marco Attene                                                     *
* Copyright(C) 2012: IMATI-GE / CNR                                         *
* All rights reserved.                                                      *
*                                                                           *
* This program is dual-licensed as follows:                                 *
*                                                                           *
* (1) You may use TMesh as free software; you can redistribute it and/or *
* modify it under the terms of the GNU General Public License as published  *
* by the Free Software Foundation; either version 3 of the License, or      *
* (at your option) any later version.                                       *
* In this case the program is distributed in the hope that it will be       *
* useful, but WITHOUT ANY WARRANTY; without even the implied warranty of    *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
* GNU General Public License (http://www.gnu.org/licenses/gpl.txt)          *
* for more details.                                                         *
*                                                                           *
* (2) You may use TMesh as part of a commercial software. In this case a *
* proper agreement must be reached with the Authors and with IMATI-GE/CNR   *
* based on a proper licensing contract.                                     *
*                                                                           *
****************************************************************************/

#ifndef _BASICS_H
#define _BASICS_H

#include <functional>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <limits.h>
#include <float.h>
#include "coordinates.h"

namespace T_MESH
{

#ifdef EXTENSIBLE_TMESH
#define TMESH_VIRTUAL virtual
#else
#define TMESH_VIRTUAL
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TMESH_VERSION	"2.0"
#define TMESH_YEAR		2012

class TMesh
{
 public:

 static void (*display_message)(const char *, int);
 static const char *app_name;
 static const char *app_version;
 static const char *app_year;
 static const char *app_authors;
 static const char *app_url;
 static const char *app_maillist;

 static const char *filename; // This might be null. If not, it represents the file we are currently working with.

 static bool quiet;

 static void init(void (*)(const char *, int) = NULL);

 static void info(const char *, ...);
 static void warning(const char *, ...);
 static void error(const char *, ...);
 static void begin_progress();
 static void report_progress(const char *, ...);
 static void end_progress();

 //! When called with a nonzero argument 'ts', launches a chronometer with a timeout of 'ts' seconds.
 //! Later calls without arguments check the chronometer, and if it is over 'ts' the program exits.
 static void exitOnTimeout(clock_t ts = 0);

 //! Appends a line to the file "tmesh.log"
 static void addMessageToLogFile(const char *msg);

 //! Formats a message headed with date/time/filename, appends it to "tmesh.log", and exits with error
 static void logToFileAndExit(const char *msg);

 //! When called without arguments prints the elapsed time from the latest reset.
 static void printElapsedTime(bool reset = false);

 static void useRationals(bool u);
 static bool isUsingRationals();
 static void useFiltering(bool u);
 static bool isUsingFiltering();

 //! Returns the status before the switch
 static bool useRationals() { bool t = isUsingRationals(); useRationals(true); return t; }

 static void setFilename(const char *fname) { filename = fname; }
};

#define DISPMSG_ACTION_SETWIDGET	1
#define DISPMSG_ACTION_PUTNEWLINE	2
#define DISPMSG_ACTION_PUTPROGRESS	3
#define DISPMSG_ACTION_PUTMESSAGE	4
#define DISPMSG_ACTION_ERRORDIALOG	5

#ifndef _INC_WINDOWS
typedef unsigned char	UBYTE;
typedef   signed char	 BYTE;
typedef unsigned short UINT16;
typedef   signed short	INT16;
#endif

#define UBYTE_MAX	255

#ifndef UINT16_MAX
#define UINT16_MAX	65535
#endif

#define FABS(a) (((a)<0)?(-(a)):(a))
#define LOG2(a) (log(a)/log(2))
#define PI2	(M_PI/2.0)
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b)(((a)>(b))?(a):(b))
#endif

//////// Swaps two pointers. ///////////////////////////////

inline void p_swap(void **a, void **b) {void *t = *a; *a = *b; *b = t;}

/////////////////////////////////////////////////////////////////////////////////////////////

typedef int64_t  j_voidint;

// void* replacement
class Data {
    std::function<void(int64_t)> delete_object_func = [](int64_t){};
    int64_t value = 0; // either pointer, or numeric value

public:
    Data() {
        delete_object_func = [](int64_t){};
    }

    Data(void* ptr) = delete;

    template<typename S>
    Data(S* ptr) {
        delete_object_func = [](int64_t p){
            delete reinterpret_cast<S*>(p);
        };
        value = reinterpret_cast<int64_t>(ptr);
    }

    operator void*() = delete;

    template<typename S>
    operator S*() const {
        return reinterpret_cast<S*>(value);
    }

    bool empty() const {
        return value == 0;
    }

    bool notEmpty() const {
           return value != 0;
       }


    bool operator==(const Data &d) const {
        return value == d.value;
    }

    bool operator!=(const Data &d) const {
        return value != d.value;
    }

    template<typename S>
    bool operator!=(const S* ptr) const {
        return value != reinterpret_cast<int64_t>(ptr);
    }

    operator int64_t() const {
        return value;
    }

    Data& operator=(int64_t val) {
        delete_object_func = [](int64_t){};
        value = val;
        return *this;
    }

    void clear() {
        if (value != 0) {
            delete_object_func(value);
        }
       forget();
    }

    void forget() {
        delete_object_func = [](int64_t) {};
        value = 0;
    }
};


} //namespace T_MESH

#endif //_BASICS_H

