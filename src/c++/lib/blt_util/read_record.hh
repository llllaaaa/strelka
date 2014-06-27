// -*- mode: c++; indent-tabs-mode: nil; -*-
//
// Starka
// Copyright (c) 2009-2014 Illumina, Inc.
//
// This software is provided under the terms and conditions of the
// Illumina Open Source Software License 1.
//
// You should have received a copy of the Illumina Open Source
// Software License 1 along with this program. If not, see
// <https://github.com/sequencing/licenses/>
//

/// \file

/// \author Chris Saunders
///

#pragma once


#include "boost/utility.hpp"


struct read_record : private boost::noncopyable
{

    virtual ~read_record() {}

    virtual const char* qname() const = 0;

    virtual bool is_unmapped() const = 0;
    virtual bool is_dup() const = 0;
    virtual bool is_filter() const = 0;

    virtual int pos() const = 0;
};
