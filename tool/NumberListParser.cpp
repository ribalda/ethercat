/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2009  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 ****************************************************************************/

#include <cstring>
#include <sstream>
#include <stdexcept>
using namespace std;

#include "NumberListParser.h"

/*****************************************************************************/

NumberListParser::NumberListParser():
    max(0U),
    hasMax(false)
{
}

/*****************************************************************************/

NumberListParser::~NumberListParser()
{
}

/*****************************************************************************/

NumberListParser::List NumberListParser::parse(const char *data)
{
    List ret;
    unsigned int i = 0, size = strlen(data), firstNum = 0U, secondNum = 0U;
    typedef enum {
        SectionStart,
        FirstNumber,
        Range,
        SecondNumber,
        Finished
    } State;
    State state = SectionStart;

    while (state != Finished) {
        switch (state) {
            case SectionStart:
                if (i >= size) {
                    state = Finished;
                } else if (isNumeric(data[i])) {
                    firstNum = parseNumber(data, &i, size);
                    state = FirstNumber;
                } else if (data[i] == '-') {
                    firstNum = 0U;
                    i++;
                    state = Range;
                } else if (data[i] == ',') {
                    i++;
                } else {
                    stringstream err;
                    err << "Invalid character " << data[i]
                        << " at position " << i << "in state "
                        << state << "." << endl;
                    throw runtime_error(err.str());
                }
                break;

            case FirstNumber:
                if (i >= size) {
                    ret.push_back(firstNum);
                    state = Finished;
                } else if (data[i] == '-') {
                    i++;
                    state = Range;
                } else if (data[i] == ',') {
                    i++;
                    ret.push_back(firstNum);
                    state = SectionStart;
                } else {
                    stringstream err;
                    err << "Invalid character " << data[i]
                        << " at position " << i << "in state "
                        << state << "." << endl;
                    throw runtime_error(err.str());
                }
                break;

            case Range:
                if (i >= size) {
                    int max = maximum();
                    // only increasing ranges if second number omitted
                    if (max >= 0 && firstNum <= (unsigned int) max) {
                        List r = range(firstNum, max);
                        ret.splice(ret.end(), r);
                    }
                    state = Finished;
                } else if (isNumeric(data[i])) {
                    secondNum = parseNumber(data, &i, size);
                    state = SecondNumber;
                } else if (data[i] == ',') {
                    int max = maximum();
                    i++;
                    if (max >= 0) {
                        List r = range(firstNum, max);
                        ret.splice(ret.end(), r);
                    }
                    state = SectionStart;
                } else {
                    stringstream err;
                    err << "Invalid character " << data[i]
                        << " at position " << i << "in state "
                        << state << "." << endl;
                    throw runtime_error(err.str());
                }
                break;

            case SecondNumber:
                if (i >= size) {
                    List r = range(firstNum, secondNum);
                    ret.splice(ret.end(), r);
                    state = Finished;
                } else if (data[i] == ',') {
                    i++;
                    List r = range(firstNum, secondNum);
                    ret.splice(ret.end(), r);
                    state = SectionStart;
                } else {
                    stringstream err;
                    err << "Invalid character " << data[i]
                        << " at position " << i << "in state "
                        << state << "." << endl;
                    throw runtime_error(err.str());
                }
                break;

            default:
                {
                    stringstream err;
                    err << "Invalid state " << state << ".";
                    throw runtime_error(err.str());
                }
        }
    }

    return ret;
}

/*****************************************************************************/

int NumberListParser::maximum()
{
    if (!hasMax) {
        max = getMax();
    }

    return max;
}

/*****************************************************************************/

bool NumberListParser::isNumeric(char c)
{
    return c >= '0' && c <= '9';
}

/*****************************************************************************/

unsigned int NumberListParser::parseNumber(
        const char *data,
        unsigned int *i,
        unsigned int size
        )
{
    unsigned int numSize = 0U, ret;

    while (*i + numSize < size && isNumeric(data[*i + numSize])) {
        numSize++;
    }

    if (numSize) {
        stringstream str;
        str << string(data + *i, numSize);
        str >> ret;
    } else {
        throw runtime_error("EOF");
    }

    *i = *i + numSize;
    return ret;
}

/****************************************************************************/

NumberListParser::List NumberListParser::range(
        unsigned int i,
        unsigned int j
        )
{
    List ret;

    if (i <= j) {
        for (; i <= j; i++) {
            ret.push_back(i);
        }
    } else {
        for (; j <= i; j++) {
            ret.push_front(j);
        }
    }

    return ret;
}

/****************************************************************************/
