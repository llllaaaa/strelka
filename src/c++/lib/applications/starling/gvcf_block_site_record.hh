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

///
/// \author Chris Saunders
///

#pragma once


#include "gvcf_locus_info.hh"
#include "gvcf_options.hh"
#include "blt_util/stream_stat.hh"


/// manages compressed site record blocks output in the gVCF
///
struct gvcf_block_site_record : public shared_call_info
{
    explicit
    gvcf_block_site_record(const gvcf_options& opt)
        : frac_tol(static_cast<double>(opt.block_percent_tol)/100.)
        , abs_tol(opt.block_abs_tol)
    {
        reset();
    }

    void
    reset()
    {
        count=0;
        block_gqx.reset();
        block_dpu.reset();
        block_dpf.reset();
        pos=-1;
        ref=(char)0;
        gt = ".";
        has_call = is_covered = is_used_covered = is_nonref=false;
        ploidy = 0;

        shared_call_info::clear();
    }

    /// determine if the given site could be joined to this block:
    bool test(const digt_site_info& si) const;

    /// add site to the current block
    void join(const digt_site_info& si);

    /// determine if the given site could be joined to this block:
    bool test(const continuous_site_info& si) const;

    /// add site to the current block
    void join(const continuous_site_info& si);

    // for GT in block records, it's either a no-call (.) or homref (0/0)
    const char* get_gt() const
    {
        if (gt == ".")
            return gt.c_str();
        return "0/0";
    }


    const double frac_tol;
    const int abs_tol;
    int count;
    pos_t pos;
    char ref;
    bool is_nonref;
    bool is_covered;
    bool is_used_covered;
    int ploidy;
    std::string gt;
    stream_stat block_gqx;
    stream_stat block_dpu;
    stream_stat block_dpf;

    bool has_call;
    //stream_stat _blockMQ;
};
