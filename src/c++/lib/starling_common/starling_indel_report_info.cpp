// -*- mode: c++; indent-tabs-mode: nil; -*-
//
// Strelka - Small Variant Caller
// Copyright (c) 2009-2016 Illumina, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//

///
/// \author Chris Saunders
///


#include "blt_common/ref_context.hh"
#include "blt_util/seq_util.hh"
#include "starling_common/pos_basecall_buffer.hh"
#include "starling_common/starling_indel_report_info.hh"

#include "boost/lexical_cast.hpp"

#include <cassert>
#include <cmath>


#ifdef DEBUG_REPORT
#include "blt_util/log.hh"
#endif



static
void
copy_ref_subseq(const reference_contig_segment& ref,
                const pos_t start_pos,
                const pos_t end_pos,
                std::string& out_seq)
{
    out_seq.clear();
    for (pos_t p(start_pos); p<end_pos; ++p)
    {
        out_seq += ref.get_base(p);
    }
}



/// get the indel cigar and ref and indel strings used in the indel
/// summary line output
///
static
void
get_vcf_summary_strings(
    const IndelKey& indelKey,
    const IndelData& indelData,
    const reference_contig_segment& ref,
    std::string& vcf_indel_seq,
    std::string& vcf_ref_seq)
{
    if       (indelKey.is_breakpoint())
    {
        if       (indelKey.type == INDEL::BP_LEFT)
        {
            copy_ref_subseq(ref,indelKey.pos-1,indelKey.pos,vcf_ref_seq);
            vcf_indel_seq = vcf_ref_seq + indelData.getInsertSeq() + '.';
        }
        else if (indelKey.type == INDEL::BP_RIGHT)
        {
            copy_ref_subseq(ref,indelKey.pos,indelKey.pos+1,vcf_ref_seq);
            vcf_indel_seq = '.' + indelData.getInsertSeq() + vcf_ref_seq;
        }
        else
        {
            assert(0);
        }
    }
    else
    {
        copy_ref_subseq(ref,indelKey.pos-1,indelKey.pos+indelKey.delete_length(),vcf_ref_seq);
        copy_ref_subseq(ref,indelKey.pos-1,indelKey.pos,vcf_indel_seq);
        vcf_indel_seq += indelData.getInsertSeq();
    }
}



static
void
set_repeat_info(
    const IndelKey& indelKey,
    const reference_contig_segment& ref,
    starling_indel_report_info& indelReportInfo)
{
    if (! ((indelReportInfo.it == INDEL::INSERT) ||
           (indelReportInfo.it == INDEL::DELETE) ||
           (indelReportInfo.it == INDEL::SWAP))) return;

    unsigned insert_repeat_count(0);
    unsigned delete_repeat_count(0);

    if       (indelReportInfo.it == INDEL::INSERT)
    {
        get_vcf_seq_repeat_unit(indelReportInfo.vcf_indel_seq,indelReportInfo.repeat_unit,insert_repeat_count);
    }
    else if (indelReportInfo.it == INDEL::DELETE)
    {
        get_vcf_seq_repeat_unit(indelReportInfo.vcf_ref_seq,indelReportInfo.repeat_unit,delete_repeat_count);
    }
    else if (indelReportInfo.it == INDEL::SWAP)
    {
        std::string insert_ru;
        std::string delete_ru;
        get_vcf_seq_repeat_unit(indelReportInfo.vcf_indel_seq,insert_ru,insert_repeat_count);
        get_vcf_seq_repeat_unit(indelReportInfo.vcf_ref_seq,delete_ru,delete_repeat_count);
        if ((insert_ru != delete_ru) || insert_ru.empty()) return;

        indelReportInfo.repeat_unit=insert_ru;
    }
    else
    {
        assert(false && "Unexpected indel type");
    }
    indelReportInfo.repeat_unit_length=indelReportInfo.repeat_unit.size();

    // count repeats in contextual sequence:
    unsigned indel_context_repeat_count(0);
    {
        const pos_t indel_begin_pos(indelKey.pos);
        const pos_t indel_end_pos(indelKey.right_pos());
        const int repeat_unit_size(static_cast<int>(indelReportInfo.repeat_unit.size()));

        // count upstream repeats:
        for (pos_t i(indel_begin_pos-repeat_unit_size); i>=0; i-=repeat_unit_size)
        {
            bool is_repeat(true);
            for (int j(0); j<repeat_unit_size; ++j)
            {
                if (ref.get_base(i+j) != indelReportInfo.repeat_unit[j])
                {
                    is_repeat = false;
                    break;
                }
            }
            if (! is_repeat) break;
            indel_context_repeat_count += 1;
        }

        // count downstream repeats:
        const pos_t rs(ref.end());
        for (pos_t i(indel_end_pos); (i+static_cast<pos_t>(repeat_unit_size)-1)<rs; i+=repeat_unit_size)
        {
            bool is_repeat(true);
            for (int j(0); j<repeat_unit_size; ++j)
            {
                if (ref.get_base(i+j) != indelReportInfo.repeat_unit[j])
                {
                    is_repeat = false;
                    break;
                }
            }
            if (! is_repeat) break;
            indel_context_repeat_count += 1;
        }
    }

    indelReportInfo.ref_repeat_count = indel_context_repeat_count+delete_repeat_count;
    indelReportInfo.indel_repeat_count = indel_context_repeat_count+insert_repeat_count;
}



void
get_starling_indel_report_info(
    const IndelKey& indelKey,
    const IndelData& indelData,
    const reference_contig_segment& ref,
    starling_indel_report_info& indelReportInfo)
{
    // indel summary info
    get_vcf_summary_strings(indelKey,indelData,ref,indelReportInfo.vcf_indel_seq,indelReportInfo.vcf_ref_seq);

    indelReportInfo.it=indelKey.type;

    const pos_t indel_begin_pos(indelKey.pos);
    const pos_t indel_end_pos(indelKey.right_pos());

    // repeat analysis:
    set_repeat_info(indelKey,ref,indelReportInfo);

    // interrupted hpol compuation:
    indelReportInfo.ihpol=get_interrupted_hpol_size(indel_begin_pos-1,ref);
    indelReportInfo.ihpol=std::max(indelReportInfo.ihpol,get_interrupted_hpol_size(indel_begin_pos,ref));
    if (indel_begin_pos != indel_end_pos)
    {
        indelReportInfo.ihpol=std::max(indelReportInfo.ihpol,get_interrupted_hpol_size(indel_end_pos-1,ref));
        indelReportInfo.ihpol=std::max(indelReportInfo.ihpol,get_interrupted_hpol_size(indel_end_pos,ref));
    }
}



ReadPathScores
indel_lnp_to_pprob(const starling_base_deriv_options& dopt,
                   const ReadPathScores& path_lnp,
                   const bool is_tier2_pass,
                   const bool is_use_alt_indel)
{
    unsigned n_alleles(2);
    if (is_use_alt_indel)
    {
        n_alleles += path_lnp.alt_indel.size();
    }

    static const double allele_prior(1./static_cast<double>(n_alleles));
    static const double allele_lnprior(std::log(allele_prior));

    ReadPathScores pprob;
    ReadPathScores::score_t pprob_nonsite = dopt.get_nonsite_path_lnp(is_tier2_pass,path_lnp.nsite) + dopt.nonsite_lnprior;
    pprob.ref     = path_lnp.ref     + dopt.site_lnprior + allele_lnprior;
    pprob.indel   = path_lnp.indel   + dopt.site_lnprior + allele_lnprior;

    if (is_use_alt_indel)
    {
        for (const auto& val : path_lnp.alt_indel)
        {
            pprob.alt_indel.push_back(std::make_pair(val.first,(val.second + dopt.site_lnprior + allele_lnprior)));
        }
    }

    double scale(std::max(pprob_nonsite,std::max(pprob.ref,pprob.indel)));

    if (is_use_alt_indel)
    {
        for (const auto& val : pprob.alt_indel)
        {
            if (scale < val.second) scale = val.second;
        }
    }

    pprob_nonsite = std::exp(pprob_nonsite-scale);
    pprob.ref = std::exp(pprob.ref-scale);
    pprob.indel = std::exp(pprob.indel-scale);
    if (is_use_alt_indel)
    {
        for (auto& val : pprob.alt_indel)
        {
            val.second = std::exp((val.second)-scale);
        }
    }


    double sum(pprob_nonsite+pprob.ref+pprob.indel);
    if (is_use_alt_indel)
    {
        for (const auto& val : pprob.alt_indel)
        {
            sum += val.second;
        }
    }

    // pprob_nonsite /= sum; /// no point in normalizing this if we aren't adding it back into pprob
    pprob.ref /= sum;
    pprob.indel /= sum;
    if (is_use_alt_indel)
    {
        for (auto& val : pprob.alt_indel)
        {
            val.second /= sum;
        }
    }

    return pprob;
}



void
get_starling_indel_sample_report_info(
    const starling_base_deriv_options& dopt,
    const IndelKey& indelKey,
    const IndelSampleData& indelSampleData,
    const pos_basecall_buffer& bc_buff,
    const bool is_tier2_pass,
    const bool is_use_alt_indel,
    starling_indel_sample_report_info& isri)
{
    // get read info:
    {
        static const double path_pprob_thresh(0.999);

        unsigned n_subscore_reads(0);

        for (const auto& val : indelSampleData.read_path_lnp)
        {
            const ReadPathScores& path_lnp(val.second);

            // optionally skip tier2 data:
            if ((! is_tier2_pass) && (! path_lnp.is_tier1_read)) continue;

            const ReadPathScores pprob(indel_lnp_to_pprob(dopt,path_lnp,is_tier2_pass,is_use_alt_indel));
            if       (pprob.ref >= path_pprob_thresh)
            {
                isri.n_q30_ref_reads++;
                if (path_lnp.is_fwd_strand)
                {
                    ++isri.n_q30_ref_reads_fwd;
                }
                else
                {
                    ++isri.n_q30_ref_reads_rev;
                }

                isri.readpos_ranksum.add_observation(true, path_lnp.read_pos);
            }
            else if (pprob.indel >= path_pprob_thresh)
            {
                isri.n_q30_indel_reads++;
                if (path_lnp.is_fwd_strand)
                {
                    ++isri.n_q30_indel_reads_fwd;
                }
                else
                {
                    ++isri.n_q30_indel_reads_rev;
                }

                isri.readpos_ranksum.add_observation(false, path_lnp.read_pos);
            }
            else
            {
                bool is_alt_found(false);
#if 0
                if (pprob.is_alt && (pprob.alt >= path_pprob_thresh))
                {
                    isri.n_q30_alt_reads++;
                    is_alt_found=true;
                }
#else
                for (const auto& palt : pprob.alt_indel)
                {
                    if (palt.second >= path_pprob_thresh)
                    {
                        isri.n_q30_alt_reads++;
                        if (path_lnp.is_fwd_strand)
                        {
                            ++isri.n_q30_alt_reads_fwd;
                        }
                        else
                        {
                            ++isri.n_q30_alt_reads_rev;
                        }
                        is_alt_found=true;
                        break;
                    }
                }
#endif
                if (! is_alt_found)
                {
                    n_subscore_reads++;
                    if (path_lnp.is_fwd_strand)
                    {
                        ++isri.n_other_reads_fwd;
                    }
                    else
                    {
                        ++isri.n_other_reads_rev;
                    }
                }
            }
        }

        // total number of reads with non-zero, yet insufficient indel
        // breakpoint overlap
        const unsigned n_suboverlap_tier1_reads(indelSampleData.suboverlap_tier1_read_ids.size());
        isri.n_other_reads = (n_subscore_reads+n_suboverlap_tier1_reads);

        if (is_tier2_pass)
        {
            const unsigned n_suboverlap_tier2_reads(indelSampleData.suboverlap_tier2_read_ids.size());
            isri.n_other_reads += n_suboverlap_tier2_reads;
        }
    }

    {
        // get various indel stats from the pileup:
        pos_t depth_pos(indelKey.pos-1);
        if (indelKey.type==INDEL::BP_RIGHT) depth_pos=indelKey.pos;
        const snp_pos_info& spi(bc_buff.get_pos(depth_pos));
        isri.tier1Depth=spi.calls.size();
        isri.mapqTracker=spi.mapqTracker;
    }
}


// debug printers

void starling_indel_sample_report_info::dump(std::ostream& os) const
{
    os << "n_q30_ref_reads=" << n_q30_ref_reads
       << ",n_q30_indel_reads=" << n_q30_indel_reads
       << ",n_q30_alt_reads=" << n_q30_alt_reads
       << ",n_other_reads=" << n_other_reads
       << ",tier1Depth=" << tier1Depth;
}

std::ostream& operator<<(std::ostream& os, const starling_indel_sample_report_info& obj)
{
    obj.dump(os);
    return os;
}

void starling_indel_report_info::dump(std::ostream& os) const
{
    os << "vcf_ref_seq=" << vcf_ref_seq
       << ",vcf_indel_seq=" << vcf_indel_seq
       << ",repeat_unit=" << repeat_unit
       << ",ref_repeat_count=" << ref_repeat_count
       << ",indel_repeat_count=" << indel_repeat_count
       << ",ihpol=" << ihpol
       << ",it=" << it;
}

std::ostream& operator<<(std::ostream& os, const starling_indel_report_info& obj)
{
    obj.dump(os);
    return os;
}

