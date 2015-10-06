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
/*
 *  Created on: Jun 4, 2015
 *      Author: jduddy
 */

#include "variant_prefilter_stage.hh"
#include "calibration_models.hh"

variant_prefilter_stage::variant_prefilter_stage(const calibration_models& model, std::shared_ptr<variant_pipe_stage_base> destination)
: variant_pipe_stage_base(destination)
, _model(model)
{
}

static
void
set_site_gt(const diploid_genotype::result_set& rs,
            digt_call_info& smod)
{
    smod.max_gt=rs.max_gt;
    smod.gqx=rs.max_gt_qphred;
    smod.gq  = 2;
}


void variant_prefilter_stage::add_site_modifiers(
    const digt_site_info& si,
    digt_call_info& smod,
    const calibration_models& model)
{
    smod.clear();
    smod.is_unknown=(si.ref=='N');
    smod.is_used_covered=(si.n_used_calls!=0);
    smod.is_covered=(si.smod.is_used_covered || si.n_unused_calls!=0);

    if     (smod.is_unknown)
    {
        smod.gqx=0;
        smod.gq=0;
        smod.max_gt=0;
    }
    else if (si.dgt.genome.max_gt != si.dgt.poly.max_gt)
    {
        smod.gqx=0;
        smod.gq=si.dgt.poly.max_gt_qphred;
        smod.max_gt=si.dgt.poly.max_gt;
    }
    else
    {
        if (si.dgt.genome.max_gt_qphred<si.dgt.poly.max_gt_qphred)
        {
            set_site_gt(si.dgt.genome,smod);
        }
        else
        {
            set_site_gt(si.dgt.poly,smod);
        }
        smod.gq=si.dgt.poly.max_gt_qphred;
    }

    model.clasify_site(si, smod);
}




void variant_prefilter_stage::process(std::unique_ptr<site_info> info)
{
    if (typeid(*info) == typeid(digt_site_info))
    {
        auto si(downcast<digt_site_info>(std::move(info)));

        add_site_modifiers(*si, si->smod, _model);
        if (si->dgt.is_haploid())
        {
            if (si->smod.max_gt == si->dgt.ref_gt)
            {
                si->smod.modified_gt=MODIFIED_SITE_GT::ZERO;
            }
            else
            {
                si->smod.modified_gt=MODIFIED_SITE_GT::ONE;
            }
        }
        else if (si->dgt.is_noploid())
        {
            if (! si->is_print_unknowngt())
            {
                si->smod.set_filter(VCF_FILTERS::PloidyConflict);
            }
        }

        _sink->process(std::move(si));
    }
    else
    {
        auto si(downcast<continuous_site_info>(std::move(info)));
        for (auto& call : si->calls)
        {
            _model.default_clasify_site(*si, call);
        }

        _sink->process(std::move(si));
    }
}

void variant_prefilter_stage::process(std::unique_ptr<indel_info> info)
{
    if (typeid(*info) == typeid(digt_indel_info))
    {
        auto ii(downcast<digt_indel_info>(std::move(info)));
        // add filter for all indels in no-ploid regions:
        if (ii->first()._dindel.is_noploid())
        {
            ii->set_filter(VCF_FILTERS::PloidyConflict);
        }

        _sink->process(std::move(ii));
    }
    else
    {
        auto ii(downcast<continuous_indel_info>(std::move(info)));
        for (auto& call : ii->calls)
        {
            _model.default_clasify_indel(call);
        }
        _sink->process(std::move(ii));
    }
}



