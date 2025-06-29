//
//
/*!
  \file
  \ingroup densitydata
  \brief Implementation of class stir::DynamicDiscretisedDensity
  \author Kris Thielemans
  \author Charalampos Tsoumpas
  \author Richard Brown

*/
/*
    Copyright (C) 2005- 2011, Hammersmith Imanet Ltd
    Copyright (C) 2018, University College London
    This file is part of STIR.

    SPDX-License-Identifier: Apache-2.0

    See STIR/LICENSE.txt for details
*/

#include "stir/DynamicDiscretisedDensity.h"
#include "stir/VoxelsOnCartesianGrid.h"
#include "stir/IO/stir_ecat7.h"
#include "stir/IO/read_from_file.h"
#include "stir/decay_correction_factor.h"
#include <iostream>
#include "stir/Succeeded.h"
#include "stir/is_null_ptr.h"
#include "stir/round.h"
#include "stir/format.h"
#include "stir/warning.h"
#include "stir/error.h"

using std::string;

START_NAMESPACE_STIR

DynamicDiscretisedDensity::DynamicDiscretisedDensity(const DynamicDiscretisedDensity& argument)
{
  (*this) = argument;
}

DynamicDiscretisedDensity&
DynamicDiscretisedDensity::operator=(const DynamicDiscretisedDensity& argument)
{
  this->set_exam_info(argument.get_exam_info());
  this->_densities.resize(argument._densities.size());
  for (unsigned int i = 0; i < argument._densities.size(); ++i)
    this->_densities[i].reset(argument._densities[i]->clone());

  this->_scanner_sptr = argument._scanner_sptr;
  this->_is_decay_corrected = argument._is_decay_corrected;
  return *this;
}

void
DynamicDiscretisedDensity::set_density(const DiscretisedDensity<3, float>& density, const unsigned int frame_num)
{
  // scan start should be the same
  if (fabs(this->get_exam_info().start_time_in_secs_since_1970 - density.get_exam_info().start_time_in_secs_since_1970) > .5)
    error("DynamicDiscretisedDensity::set_density: Density should have same start_time_secs");
  // The added density should only contain 1 time frame
  if (density.get_exam_info().time_frame_definitions.get_num_time_frames() != 1)
    error("DynamicDiscretisedDensity::set_density: Density should contain 1 time frame");
  if (this->get_exam_info_sptr()->time_frame_definitions.get_num_time_frames() < frame_num)
    error("DynamicDiscretisedDensity::set_density: Set DynamicDiscretisedDensity time frame definition before using set_density");

  // Check the starts and ends match
  double dyn_start = this->exam_info_sptr->time_frame_definitions.get_start_time(frame_num);
  double dis_start = density.get_exam_info().time_frame_definitions.get_start_time(1);
  double dyn_end = this->exam_info_sptr->time_frame_definitions.get_end_time(frame_num);
  double dis_end = density.get_exam_info().time_frame_definitions.get_end_time(1);

  if (fabs(dyn_start - dis_start) > 1e-10)
    error(format(
        "DynamicDiscretisedDensity::set_density: Time frame start should match (is {} but expected {})", dis_start, dyn_start));

  if (fabs(dyn_end - dis_end) > 1e-10)
    error(
        format("DynamicDiscretisedDensity::set_density: Time frame end should match (is {} but expected {})", dis_end, dyn_end));

  this->_densities.at(frame_num - 1).reset(density.clone());
}

const std::vector<shared_ptr<DiscretisedDensity<3, float>>>&
DynamicDiscretisedDensity::get_densities() const
{
  return this->_densities;
}

const DiscretisedDensity<3, float>&
DynamicDiscretisedDensity::get_density(const unsigned int frame_num) const
{
  return *this->_densities.at(frame_num - 1);
}

DiscretisedDensity<3, float>&
DynamicDiscretisedDensity::get_density(const unsigned int frame_num)
{
  return *this->_densities.at(frame_num - 1);
}

const float
DynamicDiscretisedDensity::get_isotope_halflife() const
{
  return this->exam_info_sptr->get_radionuclide().get_half_life();
}

const float
DynamicDiscretisedDensity::get_scanner_default_bin_size() const
{
  if (!this->_scanner_sptr)
    error("DynamicDiscretisedDensity::get_scanner_default_bin_size(): scanner not set");
  return this->_scanner_sptr->get_default_bin_size();
}

float
DynamicDiscretisedDensity::get_calibration_factor() const
{
  return this->exam_info_sptr->get_calibration_factor();
}

const TimeFrameDefinitions&
DynamicDiscretisedDensity::get_time_frame_definitions() const
{
  return this->get_exam_info().get_time_frame_definitions();
}

const double
DynamicDiscretisedDensity::get_start_time_in_secs_since_1970() const
{
  return this->get_exam_info().start_time_in_secs_since_1970;
}

DynamicDiscretisedDensity*
DynamicDiscretisedDensity::read_from_file(
    const string& filename) // The written image is read in respect to its center as origin!!!
{
  unique_ptr<DynamicDiscretisedDensity> dyn_sptr(stir::read_from_file<DynamicDiscretisedDensity>(filename));
  return dyn_sptr.release();
}

Succeeded
DynamicDiscretisedDensity::write_to_ecat7(const string& filename) const
{
#ifndef HAVE_LLN_MATRIX
  return Succeeded::no;
#else

  Main_header mhead;
  ecat::ecat7::make_ECAT7_main_header(mhead, *_scanner_sptr, filename, get_density(1));
  mhead.num_frames = get_time_frame_definitions().get_num_frames();
  mhead.acquisition_type = mhead.num_frames > 1 ? DynamicEmission : StaticEmission;
  mhead.calibration_factor = _calibration_factor;
  mhead.isotope_halflife = _isotope_halflife;
  round_to(mhead.scan_start_time, floor(this->get_start_time_in_secs_since_1970()));
  MatrixFile* mptr = matrix_create(filename.c_str(), MAT_CREATE, &mhead);
  if (mptr == 0)
    {
      warning("DynamicDiscretisedDensity::write_to_ecat7 cannot write output file %s\n", filename.c_str());
      return Succeeded::no;
    }
  for (unsigned int frame_num = 1; frame_num <= get_time_frame_definitions().get_num_frames(); ++frame_num)
    {
      if (ecat::ecat7::DiscretisedDensity_to_ECAT7(mptr, get_density(frame_num), frame_num) == Succeeded::no)
        {
          matrix_close(mptr);
          return Succeeded::no;
        }
    }
  matrix_close(mptr);
  return Succeeded::yes;
#endif // end of HAVE_LLN_MATRIX
}

void
DynamicDiscretisedDensity::calibrate_frames() const
{
  for (unsigned int frame_num = 1; frame_num <= get_time_frame_definitions().get_num_frames(); ++frame_num)
    {
      *(_densities[frame_num - 1]) *= exam_info_sptr->get_calibration_factor();
    }
}

void
DynamicDiscretisedDensity::set_calibration_factor(const float calibration_factor)
{
  auto new_exam_info_sptr = std::make_shared<ExamInfo>(this->get_exam_info());
  new_exam_info_sptr->set_calibration_factor(calibration_factor);
  this->set_exam_info(*new_exam_info_sptr);
}

void
DynamicDiscretisedDensity::set_if_decay_corrected(const bool is_decay_corrected)
{
  this->_is_decay_corrected = is_decay_corrected;
}

void
DynamicDiscretisedDensity::decay_correct_frames()
{
  if (_is_decay_corrected == true)
    warning("DynamicDiscretisedDensity is already decay corrected");
  else
    {
      for (unsigned int frame_num = 1; frame_num <= get_time_frame_definitions().get_num_frames(); ++frame_num)
        {
          *(_densities[frame_num - 1])
              *= static_cast<float>(decay_correction_factor(get_isotope_halflife(),
                                                            get_time_frame_definitions().get_start_time(frame_num),
                                                            get_time_frame_definitions().get_end_time(frame_num)));
        }
      _is_decay_corrected = true;
    }
}

END_NAMESPACE_STIR
