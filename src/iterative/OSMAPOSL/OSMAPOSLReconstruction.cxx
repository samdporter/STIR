//
//
/*
    Copyright (C) 2000 PARAPET partners
    Copyright (C) 2000 - 2011-12-31, Hammersmith Imanet Ltd
    Copyright (C) 2012-06-05 - 2012, Kris Thielemans
    Copyright (C) 2018 Commonwealth Scientific and Industrial Research Organisation
    Copyright (C) 2019 - 2020 University College London
    This file is part of STIR.

    SPDX-License-Identifier: Apache-2.0 AND License-ref-PARAPET-license

    See STIR/LICENSE.txt for details
*/

/*!
  \file
  \ingroup OSMAPOSL
  \ingroup reconstructors
  \brief  implementation of the stir::OSMAPOSLReconstruction class

  \author Matthew Jacobson
  \author Sanida Mustafovic
  \author Kris Thielemans
  \author Ashley Gillman
  \author Daniel Deidda
  \author PARAPET project
*/

#include "stir/OSMAPOSL/OSMAPOSLReconstruction.h"
#include "stir/recon_buildblock/PoissonLogLikelihoodWithLinearModelForMean.h"
#include "stir/DiscretisedDensity.h"
//#include "stir/LogLikBased/common.h"
#include "stir/ThresholdMinToSmallPositiveValueDataProcessor.h"
#include "stir/ChainedDataProcessor.h"
#include "stir/Succeeded.h"
#include "stir/numerics/divide.h"
#include "stir/thresholding.h"
#include "stir/is_null_ptr.h"
#include "stir/NumericInfo.h"
#include "stir/utilities.h"
// for get_symmetries_ptr()
#include "stir/DataSymmetriesForViewSegmentNumbers.h"
#include "stir/ViewSegmentNumbers.h"
#include "stir/info.h"
#include "stir/error.h"
#include "stir/format.h"

#include "stir/modelling/ParametricDiscretisedDensity.h"
#include "stir/modelling/KineticParameters.h"

#include <memory>
#include <iostream>
#include <sstream>

#include "stir/unique_ptr.h"
#include <algorithm>
using std::min;
using std::max;
using std::cerr;
using std::endl;

START_NAMESPACE_STIR

template <typename TargetT>
const char* const OSMAPOSLReconstruction<TargetT>::registered_name = "OSMAPOSL";

template <typename TargetT>
PoissonLogLikelihoodWithLinearModelForMean<TargetT>&
OSMAPOSLReconstruction<TargetT>::objective_function()
{
  return static_cast<PoissonLogLikelihoodWithLinearModelForMean<TargetT>&>(*this->objective_function_sptr);
}

template <typename TargetT>
PoissonLogLikelihoodWithLinearModelForMean<TargetT> const&
OSMAPOSLReconstruction<TargetT>::objective_function() const
{
  return static_cast<PoissonLogLikelihoodWithLinearModelForMean<TargetT>&>(*this->objective_function_sptr);
}

template <typename TargetT>
PoissonLogLikelihoodWithLinearModelForMean<TargetT> const&
OSMAPOSLReconstruction<TargetT>::get_objective_function() const
{
  // just use the above (private) function
  return this->objective_function();
}

//*********** parameters ***********

template <typename TargetT>
void
OSMAPOSLReconstruction<TargetT>::set_defaults()
{
  base_type::set_defaults();
  enforce_initial_positivity = true;
  maximum_relative_change = NumericInfo<float>().max_value();
  minimum_relative_change = 0;
  write_update_image = 0;
  inter_update_filter_interval = 0;
  inter_update_filter_ptr.reset();
  MAP_model = "additive";
}

template <typename TargetT>
void
OSMAPOSLReconstruction<TargetT>::initialise_keymap()
{
  base_type::initialise_keymap();
  this->parser.add_start_key("OSMAPOSLParameters");
  this->parser.add_stop_key("End");
  this->parser.add_stop_key("End OSMAPOSLParameters");

  this->parser.add_key("enforce initial positivity condition", &this->enforce_initial_positivity);
  this->parser.add_key("inter-update filter subiteration interval", &this->inter_update_filter_interval);
  this->parser.add_parsing_key("inter-update filter type", &this->inter_update_filter_ptr);
  this->parser.add_key("MAP_model", &this->MAP_model);
  this->parser.add_key("maximum relative change", &this->maximum_relative_change);
  this->parser.add_key("minimum relative change", &this->minimum_relative_change);
  this->parser.add_key("write update image", &this->write_update_image);
}

template <typename TargetT>
void
OSMAPOSLReconstruction<TargetT>::ask_parameters()
{

  base_type::ask_parameters();

  enforce_initial_positivity = ask("Enforce initial positivity condition?", true);

  inter_update_filter_interval
      = ask_num("Do inter-update filtering at sub-iteration intervals of: ", 0, this->num_subiterations, 0);

  if (inter_update_filter_interval > 0)
    {

      cerr << endl << "Supply inter-update filter type:\nPossible values:\n";
      DataProcessor<TargetT>::list_registered_names(cerr);

      const std::string inter_update_filter_type = ask_string("");

      inter_update_filter_ptr.reset(DataProcessor<TargetT>::read_registered_object(0, inter_update_filter_type));
    }

  if (!this->objective_function_sptr->prior_is_zero())
    MAP_model = ask_string("Use additive or multiplicative form of MAP-OSL ('additive' or 'multiplicative')", "additive");

  // KT 17/08/2000 3 new parameters
  const double max_in_double = static_cast<double>(NumericInfo<float>().max_value());
  maximum_relative_change = ask_num("maximum relative change", 1., max_in_double, max_in_double);
  minimum_relative_change = ask_num("minimum relative change", 0., 1., 0.);

  write_update_image = ask_num("write update image", 0, 1, 0);
}

template <typename TargetT>
bool
OSMAPOSLReconstruction<TargetT>::post_processing()
{
  if (base_type::post_processing())
    return true;

  if (!this->objective_function_sptr->prior_is_zero())
    {
      // TODO MAP_model really should be an ASCIIlist, without automatic checking on values
      // let set_MAP_model do the checking
      this->set_MAP_model(this->MAP_model);
    }
  return false;
}

//*********** set_ functions ***********
template <typename TargetT>
void
OSMAPOSLReconstruction<TargetT>::set_inter_update_filter_interval(const int arg)
{
  this->inter_update_filter_interval = arg;
}

template <typename TargetT>
void
OSMAPOSLReconstruction<TargetT>::set_inter_update_filter_ptr(const shared_ptr<DataProcessor<TargetT>>& arg)
{
  this->inter_update_filter_ptr = arg;
}

template <typename TargetT>
void
OSMAPOSLReconstruction<TargetT>::set_maximum_relative_change(const double arg)
{
  this->maximum_relative_change = arg;
}

template <typename TargetT>
void
OSMAPOSLReconstruction<TargetT>::set_minimum_relative_change(const double arg)
{
  this->minimum_relative_change = arg;
}

template <typename TargetT>
void
OSMAPOSLReconstruction<TargetT>::set_enforce_initial_positivity(const bool arg)
{
  this->enforce_initial_positivity = arg;
}

template <typename TargetT>
void
OSMAPOSLReconstruction<TargetT>::set_write_update_image(const int arg)
{
  this->write_update_image = arg;
}

template <typename TargetT>
void
OSMAPOSLReconstruction<TargetT>::set_MAP_model(const std::string& arg)
{
  this->MAP_model = arg;
  if (MAP_model != "additive" && MAP_model != "multiplicative")
    error(format("MAP model should have as value 'additive' or 'multiplicative', while it is '{}'", MAP_model));
}

//*********** other functions ***********

template <typename TargetT>
OSMAPOSLReconstruction<TargetT>::OSMAPOSLReconstruction()
{
  set_defaults();
}

template <typename TargetT>
OSMAPOSLReconstruction<TargetT>::OSMAPOSLReconstruction(const std::string& parameter_filename)
{
  this->initialise(parameter_filename);
  info(this->parameter_info());
}

template <typename TargetT>
std::string
OSMAPOSLReconstruction<TargetT>::method_info() const
{

  // TODO add prior name?

  std::ostringstream s;

  if (this->inter_update_filter_interval > 0)
    s << "IUF-";
  if (this->num_subsets > 1)
    s << "OS";
  if (this->objective_function_sptr->prior_is_zero())
    s << "EM";
  else
    s << "MAPOSL";
  if (this->inter_iteration_filter_interval > 0)
    s << "S";

  return s.str();
}

template <typename TargetT>
Succeeded
OSMAPOSLReconstruction<TargetT>::set_up(shared_ptr<TargetT> const& target_image_ptr)
{
  // TODO should use something like iterator_traits to figure out the
  // type instead of hard-wiring float
  static const float small_num = 0.000001F;

  if (base_type::set_up(target_image_ptr) == Succeeded::no)
    return Succeeded::no;

  if (is_null_ptr(dynamic_cast<PoissonLogLikelihoodWithLinearModelForMean<TargetT> const*>(this->objective_function_sptr.get())))
    {
      error("OSMAPOSL can only work with an objective function of type PoissonLogLikelihoodWithLinearModelForMean");
      return Succeeded::no;
    }

  // check subset balancing
  {
    std::string warning_message = "OSMAPOSL\n";
    if (!this->objective_function().subsets_are_approximately_balanced(warning_message))
      {
        error("%s\nOSMAPOSL cannot handle this.", warning_message.c_str());
        return Succeeded::no;
      }
  } // end check balancing

  if (this->enforce_initial_positivity)
    threshold_min_to_small_positive_value(target_image_ptr->begin_all(), target_image_ptr->end_all(), small_num);

  if (this->inter_update_filter_interval < 0)
    {
      error("Range error in inter-update filter interval");
      return Succeeded::no;
    }

  if (this->inter_update_filter_interval > 0 && !is_null_ptr(this->inter_update_filter_ptr))
    {
      // ensure that the result image of the filter is positive
      shared_ptr<ThresholdMinToSmallPositiveValueDataProcessor<TargetT>> thresholding_sptr(
          new ThresholdMinToSmallPositiveValueDataProcessor<TargetT>);
      this->inter_update_filter_ptr.reset(new ChainedDataProcessor<TargetT>(this->inter_update_filter_ptr, thresholding_sptr));
      // KT 04/06/2003 moved set_up after chaining the filter. Otherwise it would be
      // called again later on anyway.
      // Note however that at present,
      info("Building inter-update filter kernel");
      if (this->inter_update_filter_ptr->set_up(*target_image_ptr) == Succeeded::no)
        {
          error("Error building inter-update filter");
          return Succeeded::no;
        }
    }
  if (this->inter_iteration_filter_interval > 0 && !is_null_ptr(this->inter_iteration_filter_ptr))
    {
      // ensure that the result image of the filter is positive
      shared_ptr<ThresholdMinToSmallPositiveValueDataProcessor<TargetT>> thresholding_sptr(
          new ThresholdMinToSmallPositiveValueDataProcessor<TargetT>);
      this->inter_iteration_filter_ptr.reset(
          new ChainedDataProcessor<TargetT>(this->inter_iteration_filter_ptr, thresholding_sptr));
      // KT 04/06/2003 moved set_up after chaining the filter (and removed it from IterativeReconstruction)
      info("Building inter-iteration filter kernel");
      if (this->inter_iteration_filter_ptr->set_up(*target_image_ptr) == Succeeded::no)
        {
          error("Error building inter iteration filter");
          return Succeeded::no;
        }
    }

  // initialise mutliplicative update to zeros
  multiplicative_update_image_ptr = unique_ptr<TargetT>(target_image_ptr->get_empty_copy());

  return Succeeded::yes;
}

template <typename TargetT>
void
OSMAPOSLReconstruction<TargetT>::compute_sub_gradient_without_penalty_plus_sensitivity(TargetT& gradient,
                                                                                       const TargetT& current_estimate,
                                                                                       const int subset_num)
{
  this->objective_function().compute_sub_gradient_without_penalty_plus_sensitivity(gradient, current_estimate, subset_num);
};

template <typename TargetT>
const TargetT&
OSMAPOSLReconstruction<TargetT>::get_subset_sensitivity(const int subset_num)
{
  return this->objective_function().get_subset_sensitivity(subset_num);
};

template <typename TargetT>
void
OSMAPOSLReconstruction<TargetT>::apply_multiplicative_update(TargetT& current_image_estimate,
                                                             const TargetT& multiplicative_update_image)
{
  this->check(current_image_estimate);
  typename TargetT::const_full_iterator multiplicative_update_image_iter = multiplicative_update_image.begin_all_const();
  const typename TargetT::const_full_iterator end_multiplicative_update_image_iter = multiplicative_update_image.end_all_const();
  typename TargetT::full_iterator current_image_estimate_iter = current_image_estimate.begin_all();
  while (multiplicative_update_image_iter != end_multiplicative_update_image_iter)
    {
      *current_image_estimate_iter *= (*multiplicative_update_image_iter);
      ++current_image_estimate_iter;
      ++multiplicative_update_image_iter;
    }
}

template <typename TargetT>
void
OSMAPOSLReconstruction<TargetT>::update_estimate(TargetT& current_image_estimate)
{
  this->check(current_image_estimate);
  // TODO should use something like iterator_traits to figure out the
  // type instead of hard-wiring float
  static const float small_num = 0.000001F;

#ifndef PARALLEL
  // CPUTimer subset_timer;
  // subset_timer.start();
#else  // PARALLEL
  PTimer timerSubset;
  timerSubset.Start();
#endif // PARALLEL

  const int subset_num = this->get_subset_num();
  info(format("Now processing subset #: {}", subset_num));

  this->compute_sub_gradient_without_penalty_plus_sensitivity(
      *multiplicative_update_image_ptr, current_image_estimate, subset_num);

  // divide by subset sensitivity
  {
    const TargetT& sensitivity = this->get_subset_sensitivity(subset_num);

    int count = 0;

    // std::cerr <<this->MAP_model << std::endl;

    if (this->objective_function_sptr->prior_is_zero())
      {
        divide(multiplicative_update_image_ptr->begin_all(),
               multiplicative_update_image_ptr->end_all(),
               sensitivity.begin_all(),
               0.F); // no need to find a threshold for division by sensitivity
      }
    else
      {
        unique_ptr<TargetT> denominator_ptr(current_image_estimate.get_empty_copy());

        this->objective_function_sptr->get_prior_ptr()->compute_gradient(*denominator_ptr, current_image_estimate);

        typename TargetT::full_iterator denominator_iter = denominator_ptr->begin_all();
        const typename TargetT::full_iterator denominator_end = denominator_ptr->end_all();
        typename TargetT::const_full_iterator sensitivity_iter = sensitivity.begin_all();

        if (this->MAP_model == "additive")
          {
            // lambda_new = lambda / (p_v + beta*prior_gradient/ num_subsets) *
            //                   sum_subset backproj(measured/forwproj(lambda))
            // with p_v = sum_{b in subset} p_bv
            // actually, we restrict 1 + beta*prior_gradient/num_subsets/p_v between .1 and 10
            while (denominator_iter != denominator_end)
              {
                *denominator_iter = *denominator_iter / this->get_num_subsets() + (*sensitivity_iter);
                // bound denominator between (*sensitivity_iter)/10 and (*sensitivity_iter)*10
                *denominator_iter = std::max(std::min(*denominator_iter, (*sensitivity_iter) * 10), (*sensitivity_iter) / 10);
                ++denominator_iter;
                ++sensitivity_iter;
              }
          }
        else
          {
            if (this->MAP_model == "multiplicative")
              {
                // multiplicative form
                // lambda_new = lambda / (p_v*(1 + beta*prior_gradient)) *
                //                   sum_subset backproj(measured/forwproj(lambda))
                // with p_v = sum_{b in subset} p_bv
                // actually, we restrict 1 + beta*prior_gradient between .1 and 10
                while (denominator_iter != denominator_end)
                  {
                    *denominator_iter += 1;
                    // bound denominator between 1/10 and 1*10
                    // TODO code will fail if *denominator_iter is not a float
                    *denominator_iter = std::max(std::min(*denominator_iter, 10.F), 1 / 10.F);
                    *denominator_iter *= (*sensitivity_iter);
                    ++denominator_iter;
                    ++sensitivity_iter;
                  }
              }
          }

        // do the division
        // TODO: The thresholding implied in "divide" potentially fails with parametric images
        // as the different parametric images can have very different scales.
        // See https://github.com/UCL/STIR/issues/906
        divide(multiplicative_update_image_ptr->begin_all(),
               multiplicative_update_image_ptr->end_all(),
               denominator_ptr->begin_all(),
               small_num);
      }

    info(format("Number of (cancelled) singularities in Sensitivity division: {}", count));
  }

  if (this->inter_update_filter_interval > 0 && !is_null_ptr(this->inter_update_filter_ptr)
      && !(this->subiteration_num % this->inter_update_filter_interval))
    {
      info("Applying inter-update filter");
      this->inter_update_filter_ptr->apply(current_image_estimate);
    }

  // KT 17/08/2000 limit update
  // TODO move below thresholding?
  if (this->write_update_image && !this->_disable_output)
    {
      // allocate space for the filename assuming that
      // we never have more than 10^49 subiterations ...
      char* fname = new char[this->output_filename_prefix.size() + 60];
      sprintf(fname, "%s_update_%d", this->output_filename_prefix.c_str(), this->subiteration_num);

      // Write it to file
      this->output_file_format_ptr->write_to_file(fname, *multiplicative_update_image_ptr);
      delete[] fname;
    }

  if (this->subiteration_num != 1)
    {
      const float current_min
          = *std::min_element(multiplicative_update_image_ptr->begin_all(), multiplicative_update_image_ptr->end_all());
      const float current_max
          = *std::max_element(multiplicative_update_image_ptr->begin_all(), multiplicative_update_image_ptr->end_all());
      const float new_min = static_cast<float>(this->minimum_relative_change);
      const float new_max = static_cast<float>(this->maximum_relative_change);
      info(format("Update image old min,max: {}, {}, new min,max {}, {}",
                  current_min,
                  current_max,
                  (min(current_min, new_min)),
                  (max(current_max, new_max))));

      threshold_upper_lower(
          multiplicative_update_image_ptr->begin_all(), multiplicative_update_image_ptr->end_all(), new_min, new_max);
    }

  // current_image_estimate *= *multiplicative_update_image_ptr;
  apply_multiplicative_update(current_image_estimate, *multiplicative_update_image_ptr);

#ifdef PARALLEL
  timerSubset.Stop();
  info(format("Subset: {}secs", timerSubset.GetTime()));
#endif
}

template class OSMAPOSLReconstruction<DiscretisedDensity<3, float>>;
template class OSMAPOSLReconstruction<ParametricVoxelsOnCartesianGrid>;

END_NAMESPACE_STIR
