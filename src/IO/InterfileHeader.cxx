/*
    Copyright (C) 2000 PARAPET partners
    Copyright (C) 2000 - 2009-04-30, Hammersmith Imanet Ltd
    Copyright (C) 2011-07-01 - 2012, Kris Thielemans
    Copyright (C) 2013, 2016, 2018, 2020, 2023, 2024 University College London
    Copyright 2017 ETH Zurich, Institute of Particle Physics and Astrophysics
    This file is part of STIR.

    SPDX-License-Identifier: Apache-2.0 AND License-ref-PARAPET-license

    See STIR/LICENSE.txt for details
*/
/*!
  \file
  \ingroup InterfileIO
  \brief implementations for the stir::InterfileHeader class

  \author Kris Thielemans
  \author PARAPET project
  \author Richard Brown
  \author Parisa Khateri
*/

#include "stir/IO/InterfileHeader.h"
#include "stir/ExamInfo.h"
#include "stir/TimeFrameDefinitions.h"
#include "stir/PatientPosition.h"
#include "stir/ImagingModality.h"
#include "stir/ProjDataInfoCylindricalArcCorr.h"
#include "stir/ProjDataInfoCylindricalNoArcCorr.h"
#include "stir/RadionuclideDB.h"
#include "stir/info.h"
#include "stir/warning.h"
#include "stir/error.h"
#include "stir/format.h"
#include <numeric>
#include <functional>
#include "stir/ProjDataInfoBlocksOnCylindricalNoArcCorr.h"
#include "stir/ProjDataInfoGenericNoArcCorr.h"

using std::pair;
using std::sort;
using std::cerr;
using std::endl;
using std::string;
using std::vector;

START_NAMESPACE_STIR
const double MinimalInterfileHeader::double_value_not_set = -12345.60789;

shared_ptr<const ExamInfo>
MinimalInterfileHeader::get_exam_info_sptr() const
{
  return exam_info_sptr;
}

const ExamInfo&
MinimalInterfileHeader::get_exam_info() const
{
  return *exam_info_sptr;
}

MinimalInterfileHeader::MinimalInterfileHeader()
    : KeyParser()
{
  exam_info_sptr.reset(new ExamInfo);
  // need to default to PET for backwards compatibility
  // this->exam_info_sptr->imaging_modality = ImagingModality::PT;

  add_start_key("INTERFILE");
  add_key("imaging modality",
          KeyArgument::ASCII,
          (KeywordProcessor)&MinimalInterfileHeader::set_imaging_modality,
          &imaging_modality_as_string);

  add_key("version of keys",
          KeyArgument::ASCII,
          (KeywordProcessor)&MinimalInterfileHeader::set_version_specific_keys,
          &version_of_keys);

  // support for siemens interfile
  add_key("%sms-mi version number", &siemens_mi_version);
  add_stop_key("END OF INTERFILE");
}

void
MinimalInterfileHeader::set_imaging_modality()
{
  set_variable();
  this->exam_info_sptr->imaging_modality = ImagingModality(imaging_modality_as_string);
}

void
MinimalInterfileHeader::set_version_specific_keys()
{
  set_variable();
}

InterfileHeader::InterfileHeader()
    : MinimalInterfileHeader()
{
  number_format_values.push_back("bit");
  number_format_values.push_back("ascii");
  number_format_values.push_back("signed integer");
  number_format_values.push_back("unsigned integer");
  number_format_values.push_back("float");

  byte_order_values.push_back("LITTLEENDIAN");
  byte_order_values.push_back("BIGENDIAN");

  PET_data_type_values.push_back("Emission");
  PET_data_type_values.push_back("Transmission");
  PET_data_type_values.push_back("Blank");
  PET_data_type_values.push_back("AttenuationCorrection");
  PET_data_type_values.push_back("Normalisation");
  PET_data_type_values.push_back("Image");

  type_of_data_values.push_back("Static");
  type_of_data_values.push_back("Dynamic");
  type_of_data_values.push_back("Tomographic");
  type_of_data_values.push_back("Curve");
  type_of_data_values.push_back("ROI");
  type_of_data_values.push_back("PET");
  type_of_data_values.push_back("Other");

  patient_orientation_values.push_back("head_in");
  patient_orientation_values.push_back("feet_in");
  patient_orientation_values.push_back("other");
  patient_orientation_values.push_back("unknown"); // default

  patient_rotation_values.push_back("supine");
  patient_rotation_values.push_back("prone");
  patient_rotation_values.push_back("right");
  patient_rotation_values.push_back("left");
  patient_rotation_values.push_back("other");
  patient_rotation_values.push_back("unknown"); // default

  // default values
  // KT 07/10/2002 added 2 new ones
  number_format_index = 3; // unsigned integer
  bytes_per_pixel = -1;    // standard does not provide a default
  // KT 02/11/98 set default for correct variable
  byte_order_index = 1; //  file_byte_order = ByteOrder::big_endian;

  type_of_data_index = 6;        // PET
  PET_data_type_index = 5;       // Image
  patient_orientation_index = 3; // unknown
  patient_rotation_index = 5;    // unknown
  num_dimensions = 2;            // set to 2 to be compatible with Interfile version 3.3 (which doesn't have this keyword)
  matrix_labels.resize(num_dimensions);
  matrix_size.resize(num_dimensions);
  pixel_sizes.resize(num_dimensions, 1.);
  num_energy_windows = 1;
  lower_en_window_thresholds.resize(num_energy_windows);
  upper_en_window_thresholds.resize(num_energy_windows);
  lower_en_window_thresholds[0] = -1.F;
  upper_en_window_thresholds[0] = -1.F;
  num_time_frames = 1;
  image_scaling_factors.resize(num_time_frames);
  for (int i = 0; i < num_time_frames; i++)
    image_scaling_factors[i].resize(1, 1.);
  lln_quantification_units = 1.;

  data_offset_each_dataset.resize(num_time_frames, 0UL);

  data_offset = 0UL;
  calibration_factor = -1;

  radionuclide_name.resize(1);
  radionuclide_half_life.resize(1);
  radionuclide_half_life[0] = -1.F;
  radionuclide_branching_ratio.resize(1);
  radionuclide_branching_ratio[0] = -1.F;

  add_key("name of data file", &data_file_name);
  add_key("originating system", &exam_info_sptr->originating_system);
  ignore_key("GENERAL DATA");
  ignore_key("GENERAL IMAGE DATA");

  add_key("calibration factor", &calibration_factor);
  // deprecated, but used by Siemens
  add_key("isotope name", &isotope_name);
  ignore_key("number of radionuclides"); // just always use 1. TODO should check really
  add_vectorised_key("radionuclide name", &radionuclide_name);
  add_vectorised_key("radionuclide halflife (sec)", &radionuclide_half_life);
  add_vectorised_key("radionuclide branching factor", &radionuclide_branching_ratio);
  add_key("study date", &study_date_time.date);
  add_key("study_time", &study_date_time.time);
  add_key("type of data",
          KeyArgument::ASCIIlist,
          (KeywordProcessor)&InterfileHeader::set_type_of_data,
          &type_of_data_index,
          &type_of_data_values);

  add_key("patient orientation", &patient_orientation_index, &patient_orientation_values);
  add_key("patient rotation", &patient_rotation_index, &patient_rotation_values);

  add_key("imagedata byte order", &byte_order_index, &byte_order_values);

  ignore_key("data format");
  add_key("number format", &number_format_index, &number_format_values);
  add_key("number of bytes per pixel", &bytes_per_pixel);
  add_key("number of dimensions", KeyArgument::INT, (KeywordProcessor)&InterfileHeader::read_matrix_info, &num_dimensions);
  add_vectorised_key("matrix size", &matrix_size);
  add_vectorised_key("matrix axis label", &matrix_labels);
  add_vectorised_key("scaling factor (mm/pixel)", &pixel_sizes);
  add_key("number of time frames", KeyArgument::INT, (KeywordProcessor)&InterfileHeader::read_frames_info, &num_time_frames);
  add_vectorised_key("image relative start time (sec)", &image_relative_start_times);
  add_vectorised_key("image duration (sec)", &image_durations);
  // image start time[<f>] := <TimeFormat>

  // ignore these as we'll never use them
  ignore_key("maximum pixel count");
  ignore_key("minimum pixel count");

  // TODO move to PET?
  add_vectorised_key("image scaling factor", &image_scaling_factors);

  // support for Louvain la Neuve's extension of 3.3
  add_key("quantification units", &lln_quantification_units);

  add_key("number of energy windows",
          KeyArgument::INT,
          (KeywordProcessor)&InterfileHeader::read_num_energy_windows,
          &num_energy_windows);
  add_vectorised_key("energy window lower level", &lower_en_window_thresholds);
  add_vectorised_key("energy window upper level", &upper_en_window_thresholds);

  bed_position_horizontal = 0.F;
  add_key("start horizontal bed position (mm)", &bed_position_horizontal);
  bed_position_vertical = 0.F;
  add_key("start vertical bed position (mm)", &bed_position_vertical);
}

void
InterfileHeader::set_version_specific_keys()
{
  MinimalInterfileHeader::set_version_specific_keys();
  if (this->version_of_keys == "STIR3.0")
    {
      info("Setting energy window keys as in STIR3.0");
      // only a single energy window, and non-vectorised
      remove_key("energy window lower level");
      remove_key("energy window upper level");
      add_key("energy window lower level", &lower_en_window_thresholds[0]);
      add_key("energy window upper level", &upper_en_window_thresholds[0]);
    }
}

// MJ 17/05/2000 made bool
bool
InterfileHeader::post_processing()
{
  if (type_of_data_index < 0)
    {
      warning("Interfile Warning: 'type_of_data' keyword required");
      return true;
    }

  if (!study_date_time.date.empty() && !study_date_time.time.empty())
    {
      try
        {
          exam_info_sptr->start_time_in_secs_since_1970 = Interfile_datetime_to_secs_since_Unix_epoch(study_date_time);
        }
      catch (...)
        {}
    }

  this->exam_info_sptr->set_calibration_factor(calibration_factor);

  const bool is_spect = this->exam_info_sptr->imaging_modality.get_modality() == ImagingModality::NM;

  // radionuclide
  {
    RadionuclideDB radionuclide_db;
    const std::string rn_name = !this->radionuclide_name[0].empty() ? this->radionuclide_name[0] : this->isotope_name;
    auto radionuclide = radionuclide_db.get_radionuclide(exam_info_sptr->imaging_modality, rn_name);
    if (radionuclide.get_half_life(false) < 0)
      radionuclide = Radionuclide(rn_name.empty() ? "Unknown" : rn_name,
                                  is_spect ? -1.F : 511.F, // TODO handle energy for SPECT
                                  radionuclide_branching_ratio[0],
                                  radionuclide_half_life[0],
                                  this->exam_info_sptr->imaging_modality);
    this->exam_info_sptr->set_radionuclide(radionuclide);
  }

  if (patient_orientation_index < 0 || patient_rotation_index < 0)
    return true;
  // warning: relies on index taking same values as enums in PatientPosition
  exam_info_sptr->patient_position.set_rotation(static_cast<PatientPosition::RotationValue>(patient_rotation_index));
  exam_info_sptr->patient_position.set_orientation(static_cast<PatientPosition::OrientationValue>(patient_orientation_index));

  if (number_format_index < 0 || static_cast<ASCIIlist_type::size_type>(number_format_index) >= number_format_values.size())
    {
      warning("Interfile internal error: 'number_format_index' out of range\n");
      return true;
    }
  // KT 07/10/2002 new
  // check if bytes_per_pixel is set if the data type is not 'bit'
  if (number_format_index != 0 && bytes_per_pixel <= 0)
    {
      warning("Interfile error: 'number of bytes per pixel' keyword should be set\n to a number > 0");
      return true;
    }

  type_of_numbers = NumericType(number_format_values[number_format_index], bytes_per_pixel);

  file_byte_order = byte_order_index == 0 ? ByteOrder::little_endian : ByteOrder::big_endian;

  // KT 07/10/2002 more extensive error checking for matrix_size keyword
  if (matrix_size.size() == 0)
    {
      warning("Interfile error: no matrix size keywords present\n");
      return true;
    }
  for (unsigned int dim = 0; dim != matrix_size.size(); ++dim)
    {
      if (matrix_size[dim].size() == 0)
        {
          warning("Interfile error: dimension (%d) of 'matrix size' not present\n", dim);
          return true;
        }
      for (unsigned int i = 0; i != matrix_size[dim].size(); ++i)
        {
          if (matrix_size[dim][i] <= 0)
            {
              warning("Interfile error: dimension (%d) of 'matrix size' has a number <= 0 at position\n", dim, i);
              return true;
            }
        }
    }

  for (int frame = 0; frame < this->get_num_datasets(); frame++)
    {
      if (image_scaling_factors[frame].size() == 1)
        {
          // use the only value for every scaling factor
          image_scaling_factors[frame].resize(matrix_size[matrix_size.size() - 1][0]);
          for (unsigned int i = 1; i < image_scaling_factors[frame].size(); i++)
            image_scaling_factors[frame][i] = image_scaling_factors[frame][0];
        }
      else if (static_cast<int>(image_scaling_factors[frame].size()) != matrix_size[matrix_size.size() - 1][0])
        {
          warning("Interfile error: wrong number of image scaling factors\n");
          return true;
        }
    }

  // KT 07/10/2002 new
  // support for non-standard key
  // TODO as there's currently no way to find out if a key was used in the header, we just rely on the
  // fact that the default didn't change. This isn't good enough, but it has to do for now.
  if (lln_quantification_units != 1.)
    {
      const bool all_one = image_scaling_factors[0][0] == 1.;
      for (int frame = 0; frame < this->get_num_datasets(); frame++)
        for (unsigned int i = 0; i < image_scaling_factors[frame].size(); i++)
          {
            // check if all image_scaling_factors are equal to 1 (i.e. the image_scaling_factors keyword
            // probably never occured) or lln_quantification_units
            if ((all_one && image_scaling_factors[frame][i] != 1.)
                || (!all_one && image_scaling_factors[frame][i] != lln_quantification_units))
              {
                warning("Interfile error: key 'quantification units' can only be used when either "
                        "image_scaling_factors[] keywords are not present, or have identical values.\n");
                return true;
              }
            // if they're all 1, we set the value to lln_quantification_units
            if (all_one)
              image_scaling_factors[frame][i] = lln_quantification_units;
          }
      if (all_one)
        {
          warning("Interfile warning: non-standard key 'quantification_units' used to set 'image_scaling_factors' to %g\n",
                  lln_quantification_units);
        }
    } // lln_quantification_units
  if (num_energy_windows > 0)
    {
      if (num_energy_windows > 1)
        warning("Currently only reading the first energy window.");
      if (upper_en_window_thresholds[0] > 0 && lower_en_window_thresholds[0] > 0)
        {
          exam_info_sptr->set_high_energy_thres(upper_en_window_thresholds[0]);
          exam_info_sptr->set_low_energy_thres(lower_en_window_thresholds[0]);
        }
    }

  exam_info_sptr->time_frame_definitions = TimeFrameDefinitions(image_relative_start_times, image_durations);

  return false;
}

void
InterfileHeader::read_matrix_info()
{
  set_variable();

  matrix_labels.resize(num_dimensions);
  matrix_size.resize(num_dimensions);
  pixel_sizes.resize(num_dimensions, 1.);
}

void
InterfileHeader::read_num_energy_windows()
{
  set_variable();

  upper_en_window_thresholds.resize(num_energy_windows, -1.);
  lower_en_window_thresholds.resize(num_energy_windows, -1.);
}

void
InterfileHeader::set_type_of_data()
{
  set_variable();

  if (this->type_of_data_index == -1)
    error("Interfile parsing: type_of_data needs to be set to supported value");

  const string type_of_data = this->type_of_data_values[this->type_of_data_index];

  if (type_of_data == "PET")
    {
      ignore_key("PET STUDY (Emission data)");
      ignore_key("PET STUDY (Image data)");
      ignore_key("PET STUDY (General)");
      add_key("PET data type", &PET_data_type_index, &PET_data_type_values);
      ignore_key("process status");
      ignore_key("IMAGE DATA DESCRIPTION");
      // TODO rename keyword
      add_vectorised_key("data offset in bytes", &data_offset_each_dataset);
    }
  else if (type_of_data == "Tomographic")
    {
      ignore_key("SPECT STUDY (General)");
      ignore_key("SPECT STUDY (acquired data)");

      process_status_values.push_back("Reconstructed");
      process_status_values.push_back("Acquired");
      add_key("process status", &process_status_index, &process_status_values);

#if 0
      // overwrite vectored-value, as v3.3 had a scalar
      add_key("data offset in bytes", &data_offset);
#endif
    }
}

void
InterfileHeader::read_frames_info()
{
  set_variable();
  const int num_datasets = this->get_num_datasets();
  image_scaling_factors.resize(num_datasets);
  for (int i = 0; i < num_datasets; i++)
    image_scaling_factors[i].resize(1, 1.);
  data_offset_each_dataset.resize(num_datasets, 0UL);
  image_relative_start_times.resize(num_time_frames, 0.);
  image_durations.resize(num_time_frames, 0.);
}

/***********************************************************************/
InterfileImageHeader::InterfileImageHeader()
    : InterfileHeader()
{
  num_image_data_types = 1;
  index_nesting_level.resize(1, "");
  image_data_type_description.resize(num_image_data_types, "");

  add_vectorised_key("first pixel offset (mm)", &first_pixel_offsets);
  add_key("number of image data types",
          KeyArgument::INT,
          (KeywordProcessor)&InterfileImageHeader::read_image_data_types,
          &num_image_data_types);
  add_key("index nesting level", &index_nesting_level);
  add_vectorised_key("image data type description", &image_data_type_description);
}

void
InterfileImageHeader::read_image_data_types()
{
  set_variable();
  const int num_datasets = this->get_num_datasets();
  image_scaling_factors.resize(num_datasets);
  for (int i = 0; i < num_datasets; i++)
    image_scaling_factors[i].resize(1, 1.);
  data_offset_each_dataset.resize(num_datasets, 0UL);
  // should do this if ever we support multiple indices (TODO)
  // index_nesting_level.resize(2,"");
  image_data_type_description.resize(num_image_data_types, "");
}

void
InterfileImageHeader::read_matrix_info()
{
  base_type::read_matrix_info();
  this->first_pixel_offsets.resize(num_dimensions);
  std::fill(this->first_pixel_offsets.begin(), this->first_pixel_offsets.end(), base_type::double_value_not_set);
}

// MJ 17/05/2000 made bool
bool
InterfileImageHeader::post_processing()
{

  if (InterfileHeader::post_processing() == true)
    return true;

  if (PET_data_type_values[PET_data_type_index] != "Image")
    {
      warning("Interfile error: expecting an image\n");
      return true;
    }

  if (num_dimensions != 3)
    {
      warning("Interfile error: expecting 3D image\n");
      return true;
    }

  if ((matrix_size[0].size() != 1) || (matrix_size[1].size() != 1) || (matrix_size[2].size() != 1))
    {
      warning("Interfile error: only handling image with homogeneous dimensions\n");
      return true;
    }

  // KT 09/10/98 changed order z,y,x->x,y,z
  // KT 09/10/98 allow no labels at all
  if (matrix_labels[0].length() > 0 && (matrix_labels[0] != "x" || matrix_labels[1] != "y" || matrix_labels[2] != "z"))
    {
      warning("Interfile: only supporting x,y,z order of coordinates now.\n");
      return true;
    }
  std::vector<double> first_pixel_offsets;

  return false;
}
/**********************************************************************/

// KT 26/10/98
//  KT 13/11/98 moved stream arg from constructor to parse()
InterfilePDFSHeader::InterfilePDFSHeader()
    : InterfileHeader()
{
  num_segments = -1;

  add_key("minimum ring difference per segment",
          KeyArgument::LIST_OF_INTS,
          (KeywordProcessor)&InterfilePDFSHeader::resize_segments_and_set,
          &min_ring_difference);
  add_key("maximum ring difference per segment",
          KeyArgument::LIST_OF_INTS,
          (KeywordProcessor)&InterfilePDFSHeader::resize_segments_and_set,
          &max_ring_difference);

  tof_mash_factor = 1;
  add_key("TOF mashing factor", &tof_mash_factor);
#if STIR_VERSION < 070000
  add_alias_key("TOF mashing factor", "%TOF mashing factor");
#endif

  // Scanner keys
  // warning these keys should match what is in Scanner::parameter_info()
  // TODO get Scanner to parse these
  ignore_key("Scanner parameters");
  // this is currently ignored (use "originating system" instead)
  ignore_key("Scanner type");

  // first set to some crazy values
  num_rings = -1;
  add_key("number of rings", &num_rings);
  num_detectors_per_ring = -1;
  add_key("number of detectors per ring", &num_detectors_per_ring);
  transaxial_FOV_diameter_in_cm = -1;
  add_key("transaxial FOV diameter (cm)", &transaxial_FOV_diameter_in_cm);
  inner_ring_diameter_in_cm = -1;
  add_key("inner ring diameter (cm)", &inner_ring_diameter_in_cm);
  average_depth_of_interaction_in_cm = -1;
  add_key("average depth of interaction (cm)", &average_depth_of_interaction_in_cm);
  distance_between_rings_in_cm = -1;
  add_key("distance between rings (cm)", &distance_between_rings_in_cm);
  default_bin_size_in_cm = -1;
  add_key("default bin size (cm)", &default_bin_size_in_cm);
  // this is a good default value
  view_offset_in_degrees = 0;
  add_key("view offset (degrees)", &view_offset_in_degrees);
  max_num_non_arccorrected_bins = 0;
  default_num_arccorrected_bins = 0;
  add_key("Maximum number of non-arc-corrected bins", &max_num_non_arccorrected_bins);
  add_key("Default number of arc-corrected bins", &default_num_arccorrected_bins);

  num_axial_blocks_per_bucket = 0;
  add_key("number of blocks_per_bucket in axial direction", &num_axial_blocks_per_bucket);
  num_transaxial_blocks_per_bucket = 0;
  add_key("number of blocks_per_bucket in transaxial direction", &num_transaxial_blocks_per_bucket);
  num_axial_crystals_per_block = 0;
  add_key("number of crystals_per_block in axial direction", &num_axial_crystals_per_block);
  num_transaxial_crystals_per_block = 0;
  add_key("number of crystals_per_block in transaxial direction", &num_transaxial_crystals_per_block);
  num_axial_crystals_per_singles_unit = -1;
  add_key("number of crystals_per_singles_unit in axial direction", &num_axial_crystals_per_singles_unit);
  num_transaxial_crystals_per_singles_unit = -1;
  add_key("number of crystals_per_singles_unit in transaxial direction", &num_transaxial_crystals_per_singles_unit);
  // sensible default
  num_detector_layers = 1;
  add_key("number of detector layers", &num_detector_layers);
  energy_resolution = -1.f;
  add_key("Energy resolution", &energy_resolution);
  reference_energy = -1.f;
  add_key("Reference energy (in keV)", &reference_energy);

  max_num_timing_poss = -1;
  add_key("Maximum number of (unmashed) TOF time bins", &max_num_timing_poss);
#if STIR_VERSION < 070000
  add_alias_key("Maximum number of (unmashed) TOF time bins", "Number of TOF time bins");
#endif
  timing_poss_sequence.clear();
  add_key("TOF bin order", &timing_poss_sequence);
  size_of_timing_pos = -1.f;
  add_key("Size of unmashed TOF time bins (ps)", &size_of_timing_pos);
#if STIR_VERSION < 070000
  add_alias_key("Size of unmashed TOF time bins (ps)", "Size of timing bin (ps)");
#endif
  timing_resolution = -1.f;
  add_key("TOF timing resolution (ps)", &timing_resolution);
#if STIR_VERSION < 070000
  add_alias_key("TOF timing resolution (ps)", "timing resolution (ps)");
#endif

  // new keys for block geometry
  scanner_geometry = "Cylindrical";
  add_key("Scanner geometry (BlocksOnCylindrical/Cylindrical/Generic)", KeyArgument::ASCII, &scanner_geometry);

  axial_distance_between_crystals_in_cm = -0.1;
  add_key("distance between crystals in axial direction (cm)", &axial_distance_between_crystals_in_cm);

  transaxial_distance_between_crystals_in_cm = -0.1;
  add_key("distance between crystals in transaxial direction (cm)", &transaxial_distance_between_crystals_in_cm);

  axial_distance_between_blocks_in_cm = -0.1;
  add_key("distance between blocks in axial direction (cm)", &axial_distance_between_blocks_in_cm);

  transaxial_distance_between_blocks_in_cm = -0.1;
  add_key("distance between blocks in transaxial direction (cm)", &transaxial_distance_between_blocks_in_cm);
  // end of new keys for block geometry
  // new keys for generic geometry
  crystal_map = "";
  add_key("Name of crystal map", &crystal_map);
  // end of new keys for generic geometry

  ignore_key("end scanner parameters");

  effective_central_bin_size_in_cm = -1;
  add_key("effective central bin size (cm)", &effective_central_bin_size_in_cm);
  add_key("applied corrections", &applied_corrections);
}

void
InterfilePDFSHeader::resize_segments_and_set()
{
  // find_storage_order returns true if already found (or error)
  if (num_segments < 0 && !find_storage_order())
    {
      min_ring_difference.resize(num_segments);
      max_ring_difference.resize(num_segments);
    }

  if (num_segments >= 0)
    set_variable();
}

int
InterfilePDFSHeader::find_storage_order()
{

  /*	if(type_of_data_values[type_of_data_index] != "PET")
        {

        warning("Interfile error: expecting PET study ");
        stop_parsing();
        return true;

        }
*/
  if (num_dimensions != 4 && num_dimensions != 5)
    {
      warning("Interfile error: expecting 4D structure or 5D in case of TOF information ");
      stop_parsing();
      return true;
    }

  if (num_dimensions == 4)
    {
      // non-TOF
      num_timing_poss = 1;
      tof_mash_factor = 0;
    }
  else
    {
      // TOF
      if (matrix_labels[4] == "timing positions")
        {
          this->num_timing_poss = matrix_size[4][0];
        }
      else
        {
          warning("Interfile header parsing: currently need 'matrix axis label [5] := timing positions' for TOF data");
          stop_parsing();
          return true;
        }
    }

  if (matrix_labels[0] != "tangential coordinate")
    {
      // use error message with index [1] as that is what the user sees.
      warning("Interfile error: expecting 'matrix axis label[1] := tangential coordinate'\n");
      stop_parsing();
      return true;
    }
  num_bins = matrix_size[0][0];

  if (matrix_labels[3] == "segment")
    {
      num_segments = matrix_size[3][0];

      if (matrix_labels[1] == "axial coordinate" && matrix_labels[2] == "view")
        {
          // If TOF information is in there
          if (num_dimensions > 4)
            storage_order = ProjDataFromStream::Timing_Segment_View_AxialPos_TangPos;
          else
            storage_order = ProjDataFromStream::Segment_View_AxialPos_TangPos;

          num_views = matrix_size[2][0];
#ifdef _MSC_VER
          num_rings_per_segment.assign(matrix_size[1].begin(), matrix_size[1].end());
#else
          num_rings_per_segment = matrix_size[1];
#endif
          return false;
        }
      else if (matrix_labels[1] == "view" && matrix_labels[2] == "axial coordinate")
        {
          // If TOF information is in there
          if (num_dimensions > 4)
            storage_order = ProjDataFromStream::Timing_Segment_AxialPos_View_TangPos;
          else
            storage_order = ProjDataFromStream::Segment_AxialPos_View_TangPos;
          num_views = matrix_size[1][0];
#ifdef _MSC_VER
          num_rings_per_segment.assign(matrix_size[2].begin(), matrix_size[2].end());
#else
          num_rings_per_segment = matrix_size[2];
#endif
          return false;
        }
    }
  /*
  else if (matrix_labels[3] == "view" &&
  matrix_labels[2] == "segment" && matrix_labels[1] == "axial coordinate")
  {
  storage_order = ProjDataFromStream::View_Segment_AxialPos_TangPos;
  num_segments = matrix_size[2][0];
  num_views = matrix_size[3][0];
  #ifdef _MSC_VER
  num_rings_per_segment.assign(matrix_size[1].begin(), matrix_size[1].end());
  #else
  num_rings_per_segment = matrix_size[1];
  #endif
  return false;
   }
  */
  // shouldn 't get here
  warning("Interfile error: matrix labels not in expected (or supported) format\n");
  stop_parsing();
  return true;
}

// definition for using sort() below.
// This is a function object that allows comparing the first elements of 2
// pairs.
template <class T1, class T2>
class compare_first
{
public:
  bool operator()(const pair<T1, T2>& p1, const pair<T1, T2>& p2) const { return p1.first < p2.first; }
};

// This function assigns segment numbers by sorting the average
// ring differences. It returns a list of the segment numbers
// in the same order as the min/max_ring_difference vectors
void
find_segment_sequence(vector<int>& segment_sequence,
                      VectorWithOffset<int>& sorted_num_rings_per_segment,
                      VectorWithOffset<int>& sorted_min_ring_diff,
                      VectorWithOffset<int>& sorted_max_ring_diff,
                      vector<int>& num_rings_per_segment,
                      const vector<int>& min_ring_difference,
                      const vector<int>& max_ring_difference)
{
  const int num_segments = static_cast<int>(min_ring_difference.size());
  assert(num_segments % 2 == 1);

  vector<pair<float, int>> sum_and_location(num_segments);
  for (int i = 0; i < num_segments; i++)
    {
      sum_and_location[i].first = static_cast<float>(min_ring_difference[i] + max_ring_difference[i]);
      sum_and_location[i].second = i;
    }
#if 0
  cerr<< "DISPLAY SUM and LOCATION\n"<<endl;
  
  cerr<<"SUM\n"<<endl;
  for(unsigned int i = 0;i<sum_and_location.size();i++)
  {
    cerr<< sum_and_location[i].first<<" ";
  }
  cerr<<endl;
  
  cerr<<"Location\n"<<endl;
  for(unsigned int i = 0;i<sum_and_location.size();i++)
  {
    cerr<< sum_and_location[i].second<<" ";
  }
  cerr<<endl;
#endif

  // sort with respect to 'sum'
  std::sort(sum_and_location.begin(), sum_and_location.end(), compare_first<float, int>());
#if 0  
  cerr<<"display  sum_sorted"<<endl;
  for(unsigned int i = 0;i<sum_and_location.size();i++)
  {
    cerr<< sum_and_location[i].first<<" ";
  }
  cerr<<endl;
#endif

  // find number of segment 0
  int segment_zero_num = 0;
  while (segment_zero_num < num_segments && sum_and_location[segment_zero_num].first < -1E-3)
    segment_zero_num++;

  if (segment_zero_num == num_segments || sum_and_location[segment_zero_num].first > 1E-3)
    {
      error("This data does not seem to contain segment 0. \n"
            "We can't handle this at the moment. Sorry.");
    }

  vector<pair<int, int>> location_and_segment_num(num_segments);
  for (int i = 0; i < num_segments; i++)
    {
      location_and_segment_num[i].first = sum_and_location[i].second;
      location_and_segment_num[i].second = i - segment_zero_num;
    }

#if 0
  cerr<< "display location segment\n"<<endl;
  for(unsigned int i = 0;i<location_and_segment_num.size();i++)
  {
    cerr<< location_and_segment_num[i].first<<" ";
  }
  cerr<<endl;
  
  cerr<< "display segment\n"<<endl;
  for(unsigned int i = 0;i<location_and_segment_num.size();i++)
  {
    cerr<< location_and_segment_num[i].second<<" ";
  }
  cerr<<endl;
#endif

  const int min_segment_num = location_and_segment_num[0].second;
  const int max_segment_num = location_and_segment_num[num_segments - 1].second;

  // KT 19/05/2000 replaced limit with min/max_segment_num
  // int limit = static_cast<int>(ceil(num_segments/2 ));

  sorted_min_ring_diff = VectorWithOffset<int>(min_segment_num, max_segment_num);
  sorted_max_ring_diff = VectorWithOffset<int>(min_segment_num, max_segment_num);
  sorted_num_rings_per_segment = VectorWithOffset<int>(min_segment_num, max_segment_num);

  for (int i = 0; i < num_segments; i++)
    {
      sorted_min_ring_diff[(location_and_segment_num[i].second)] = min_ring_difference[(location_and_segment_num[i].first)];

      sorted_max_ring_diff[(location_and_segment_num[i].second)] = max_ring_difference[(location_and_segment_num[i].first)];

      sorted_num_rings_per_segment[(location_and_segment_num[i].second)]
          = num_rings_per_segment[(location_and_segment_num[i].first)];
    }

#if 0
  cerr<< "sorted_min_ring_diff\n"<<endl;
  for( int i =min_segment_num;i<max_segment_num;i++)
  {
    cerr<< sorted_min_ring_diff[i]<<" ";
  }

  cerr<<endl;

  cerr<< "sorted_max_ring_diff\n"<<endl;
  for( int i =min_segment_num;i<max_segment_num;i++)
  {
    cerr<< sorted_max_ring_diff[i]<<" ";
  }
  cerr<<endl;

  
  cerr<< "sorted_num_rings_per_segment\n"<<endl;
  for( int i =min_segment_num;i<max_segment_num;i++)
  {
    cerr<< sorted_num_rings_per_segment[i]<<" ";
  }
  cerr<<endl;
#endif

  // sort back to original location
  sort(location_and_segment_num.begin(), location_and_segment_num.end(), compare_first<int, int>());

  segment_sequence.resize(num_segments);
  for (int i = 0; i < num_segments; i++)
    segment_sequence[i] = location_and_segment_num[i].second;

#if 0    
  cerr<< "segment sequence\n"<<endl;
  for(unsigned int i =0;i<segment_sequence.size();i++)
  {
    cerr<< segment_sequence[i]<<" ";
  }
  cerr<<endl;
#endif

  //}
}

// MJ 17/05/2000 made bool
// NE 28/12/2016 Accounts for TOF stuff.
bool
InterfilePDFSHeader::post_processing()
{

  if (InterfileHeader::post_processing() == true)
    return true;

  if (PET_data_type_values[PET_data_type_index] != "Emission")
    {
      warning("Interfile error: expecting emission data\n");
      return true;
    }

  if (min_ring_difference.size() != static_cast<unsigned int>(num_segments))
    {
      warning("Interfile error: per-segment information is inconsistent: min_ring_difference\n");
      return true;
    }
  if (max_ring_difference.size() != static_cast<unsigned int>(num_segments))
    {
      warning("Interfile error: per-segment information is inconsistent: max_ring_difference\n");
      return true;
    }
  if (num_rings_per_segment.size() != static_cast<unsigned int>(num_segments))
    {
      warning("Interfile error: per-segment information is inconsistent: num_rings_per_segment\n");
      return true;
    }

  // check for arc-correction
  if (applied_corrections.size() == 0)
    {
      warning("\nParsing Interfile header for projection data: \n"
              "\t'applied corrections' keyword not found. Assuming arc-corrected data\n");
      is_arccorrected = true;
    }
  else
    {
      is_arccorrected = false;
      for (auto iter = applied_corrections.begin(); iter != applied_corrections.end(); ++iter)
        {
          const string correction = standardise_keyword(*iter);
          if (correction == "arc correction" || correction == "arc corrected")
            {
              is_arccorrected = true;
              break;
            }
          else if (correction != "none")
            warning("\nParsing Interfile header for projection data: \n"
                    "\t value '%s' for keyword 'applied corrections' ignored\n",
                    correction.c_str());
        }
    }

  VectorWithOffset<int> sorted_min_ring_diff;
  VectorWithOffset<int> sorted_max_ring_diff;
  VectorWithOffset<int> sorted_num_rings_per_segment;

  find_segment_sequence(segment_sequence,
                        sorted_num_rings_per_segment,
                        sorted_min_ring_diff,
                        sorted_max_ring_diff,
                        num_rings_per_segment,
                        min_ring_difference,
                        max_ring_difference);
#if 0  
  cerr << "PDFS data read inferred header :\n";
  cerr << "Segment sequence :";
  for (unsigned int i=0; i<segment_sequence.size(); i++)
    cerr << segment_sequence[i] << "  ";
  cerr << endl;
  cerr << "RingDiff minimum :";
  for (int i=sorted_min_ring_diff.get_min_index(); i<=sorted_min_ring_diff.get_max_index(); i++)
    cerr <<sorted_min_ring_diff[i] << "  ";  cerr << endl;
  cerr << "RingDiff maximum :";
  for (int i=sorted_max_ring_diff.get_min_index(); i<=sorted_max_ring_diff.get_max_index(); i++)
    cerr << sorted_max_ring_diff[i] << "  ";  cerr << endl;
  cerr << "Nbplanes/segment :";
  for (int i=sorted_num_rings_per_segment.get_min_index(); i<=sorted_num_rings_per_segment.get_max_index(); i++)
    cerr << sorted_num_rings_per_segment[i] << "  ";  cerr << endl;

  cerr << "Total number of planes :" 
    << std::accumulate(num_rings_per_segment.begin(), num_rings_per_segment.end(), 0)
    << endl;
#endif

  // TOF order
  if (this->timing_poss_sequence.size())
    {
      if (this->timing_poss_sequence.size() != static_cast<std::vector<int>::size_type>(this->num_timing_poss))
        {
          warning("Inconsistent number of TOF bins (" + std::to_string(this->num_timing_poss)
                  + ") and size of the 'TOF bin order' list (" + std::to_string(this->timing_poss_sequence.size()) + ").");
          return true;
        }
    }

  // handle scanner

  shared_ptr<Scanner> guessed_scanner_ptr(Scanner::get_scanner_from_name(get_exam_info().originating_system));
  bool originating_system_was_recognised = guessed_scanner_ptr->get_type() != Scanner::Unknown_scanner;
  if (!originating_system_was_recognised)
    {
      info("Interfile warning: I did not recognise the scanner from 'originating_system' (" + get_exam_info().originating_system
           + "). Hopefully there is enough information present. I will check this now.");
    }

  bool mismatch_between_header_and_guess = false;

  // check if STIR info matches the one in the header, and fill in missing details

  if (guessed_scanner_ptr->get_type() != Scanner::Unknown_scanner
      && guessed_scanner_ptr->get_type() != Scanner::User_defined_scanner)
    {
      // fill in values which are not in the Interfile header

      if (num_rings < 1)
        num_rings = guessed_scanner_ptr->get_num_rings();
      if (num_detectors_per_ring < 1)
        num_detectors_per_ring = guessed_scanner_ptr->get_max_num_views() * 2;
#if 0
    if (transaxial_FOV_diameter_in_cm < 0)
      transaxial_FOV_diameter_in_cm = guessed_scanner_ptr->FOV_radius*2/10.;
#endif
      if (inner_ring_diameter_in_cm < 0)
        inner_ring_diameter_in_cm = guessed_scanner_ptr->get_inner_ring_radius() * 2 / 10.;
      if (average_depth_of_interaction_in_cm < 0)
        average_depth_of_interaction_in_cm = guessed_scanner_ptr->get_average_depth_of_interaction() / 10;
      if (distance_between_rings_in_cm < 0)
        distance_between_rings_in_cm = guessed_scanner_ptr->get_ring_spacing() / 10;
      if (default_bin_size_in_cm < 0)
        default_bin_size_in_cm = guessed_scanner_ptr->get_default_bin_size() / 10;
      if (max_num_non_arccorrected_bins <= 0)
        max_num_non_arccorrected_bins = guessed_scanner_ptr->get_max_num_non_arccorrected_bins();
      if (default_num_arccorrected_bins <= 0)
        default_num_arccorrected_bins = guessed_scanner_ptr->get_default_num_arccorrected_bins();

      if (num_axial_blocks_per_bucket <= 0)
        num_axial_blocks_per_bucket = guessed_scanner_ptr->get_num_axial_blocks_per_bucket();
      if (num_transaxial_blocks_per_bucket <= 0)
        num_transaxial_blocks_per_bucket = guessed_scanner_ptr->get_num_transaxial_blocks_per_bucket();
      if (num_axial_crystals_per_block <= 0)
        num_axial_crystals_per_block = guessed_scanner_ptr->get_num_axial_crystals_per_block();
      if (num_transaxial_crystals_per_block <= 0)
        num_transaxial_crystals_per_block = guessed_scanner_ptr->get_num_transaxial_crystals_per_block();
      if (num_axial_crystals_per_singles_unit < 0)
        num_axial_crystals_per_singles_unit = guessed_scanner_ptr->get_num_axial_crystals_per_singles_unit();
      if (num_transaxial_crystals_per_singles_unit < 0)
        num_transaxial_crystals_per_singles_unit = guessed_scanner_ptr->get_num_transaxial_crystals_per_singles_unit();
      if (num_detector_layers <= 0)
        num_detector_layers = guessed_scanner_ptr->get_num_detector_layers();
      if (energy_resolution < 0)
        energy_resolution = guessed_scanner_ptr->get_energy_resolution();
      if (reference_energy < 0)
        reference_energy = guessed_scanner_ptr->get_reference_energy();

      // new variables for block geometry
      if (axial_distance_between_crystals_in_cm < 0)
        axial_distance_between_crystals_in_cm = guessed_scanner_ptr->get_transaxial_crystal_spacing() / 10;
      if (transaxial_distance_between_crystals_in_cm < 0)
        transaxial_distance_between_crystals_in_cm = guessed_scanner_ptr->get_transaxial_crystal_spacing() / 10;
      if (axial_distance_between_blocks_in_cm < 0)
        axial_distance_between_blocks_in_cm = guessed_scanner_ptr->get_axial_block_spacing() / 10;
      if (transaxial_distance_between_blocks_in_cm < 0)
        transaxial_distance_between_blocks_in_cm = guessed_scanner_ptr->get_transaxial_block_spacing() / 10;
      // end of new variables for block geometry

      if (guessed_scanner_ptr->is_tof_ready())
        {
          if (max_num_timing_poss < 0)
            max_num_timing_poss = guessed_scanner_ptr->get_max_num_timing_poss();
          if (size_of_timing_pos < 0)
            size_of_timing_pos = guessed_scanner_ptr->get_size_of_timing_pos();
          if (timing_resolution < 0)
            timing_resolution = guessed_scanner_ptr->get_timing_resolution();
        }

      // consistency check with values of the guessed_scanner_ptr we guessed above

      if (num_rings != guessed_scanner_ptr->get_num_rings())
        {
          warning(format("Interfile warning: 'number of rings' ({}) is expected to be {}.\n",
                         num_rings,
                         guessed_scanner_ptr->get_num_rings()));
          mismatch_between_header_and_guess = true;
        }
      if (num_detectors_per_ring != guessed_scanner_ptr->get_num_detectors_per_ring())
        {
          warning(format("Interfile warning: 'number of detectors per ring' ({}) is expected to be {}.\n",
                         num_detectors_per_ring,
                         guessed_scanner_ptr->get_num_detectors_per_ring()));
          mismatch_between_header_and_guess = true;
        }
      if (fabs(inner_ring_diameter_in_cm - guessed_scanner_ptr->get_inner_ring_radius() * 2 / 10.) > .001)
        {
          warning(format("Interfile warning: 'inner ring diameter (cm)' ({}) is expected to be {}.\n",
                         inner_ring_diameter_in_cm,
                         (guessed_scanner_ptr->get_inner_ring_radius() * 2 / 10.)));
          mismatch_between_header_and_guess = true;
        }
      if (fabs(average_depth_of_interaction_in_cm - guessed_scanner_ptr->get_average_depth_of_interaction() / 10) > .001)
        {
          warning(format("Interfile warning: 'average depth of interaction (cm)' ({}) is expected to be {}.\n",
                         average_depth_of_interaction_in_cm,
                         (guessed_scanner_ptr->get_average_depth_of_interaction() / 10)));
          mismatch_between_header_and_guess = true;
        }
      if (fabs(distance_between_rings_in_cm - guessed_scanner_ptr->get_ring_spacing() / 10) > .001)
        {
          warning(format("Interfile warning: 'distance between rings (cm)' ({}) is expected to be {}.\n",
                         distance_between_rings_in_cm,
                         (guessed_scanner_ptr->get_ring_spacing() / 10)));
          mismatch_between_header_and_guess = true;
        }
      if (fabs(default_bin_size_in_cm - guessed_scanner_ptr->get_default_bin_size() / 10) > .001)
        {
          warning(format("Interfile warning: 'default bin size (cm)' ({}) is expected to be {}.\n",
                         default_bin_size_in_cm,
                         (guessed_scanner_ptr->get_default_bin_size() / 10)));
          mismatch_between_header_and_guess = true;
        }
      if (max_num_non_arccorrected_bins - guessed_scanner_ptr->get_max_num_non_arccorrected_bins())
        {
          warning(format("Interfile warning: 'max_num_non_arccorrected_bins' ({}) is expected to be {}",
                         max_num_non_arccorrected_bins,
                         guessed_scanner_ptr->get_max_num_non_arccorrected_bins()));
          mismatch_between_header_and_guess = true;
        }
      if (default_num_arccorrected_bins - guessed_scanner_ptr->get_default_num_arccorrected_bins())
        {
          warning(format("Interfile warning: 'default_num_arccorrected_bins' ({}) is expected to be {}",
                         default_num_arccorrected_bins,
                         guessed_scanner_ptr->get_default_num_arccorrected_bins()));
          mismatch_between_header_and_guess = true;
        }
      if (guessed_scanner_ptr->get_num_transaxial_blocks_per_bucket() > 0
          && num_transaxial_blocks_per_bucket != guessed_scanner_ptr->get_num_transaxial_blocks_per_bucket())
        {
          warning(format("Interfile warning: num_transaxial_blocks_per_bucket ({}) is expected to be {}.\n",
                         num_transaxial_blocks_per_bucket,
                         guessed_scanner_ptr->get_num_transaxial_blocks_per_bucket()));
          mismatch_between_header_and_guess = true;
        }
      if (guessed_scanner_ptr->get_num_axial_blocks_per_bucket() > 0
          && num_axial_blocks_per_bucket != guessed_scanner_ptr->get_num_axial_blocks_per_bucket())
        {
          warning(format("Interfile warning: num_axial_blocks_per_bucket ({}) is expected to be {}.\n",
                         num_axial_blocks_per_bucket,
                         guessed_scanner_ptr->get_num_axial_blocks_per_bucket()));
          mismatch_between_header_and_guess = true;
        }
      if (guessed_scanner_ptr->get_num_axial_crystals_per_block() > 0
          && num_axial_crystals_per_block != guessed_scanner_ptr->get_num_axial_crystals_per_block())
        {
          warning(format("Interfile warning: num_axial_crystals_per_block ({}) is expected to be {}.\n",
                         num_axial_crystals_per_block,
                         guessed_scanner_ptr->get_num_axial_crystals_per_block()));
          mismatch_between_header_and_guess = true;
        }
      if (guessed_scanner_ptr->get_num_transaxial_crystals_per_block() > 0
          && num_transaxial_crystals_per_block != guessed_scanner_ptr->get_num_transaxial_crystals_per_block())
        {
          warning(format("Interfile warning: num_transaxial_crystals_per_block ({}) is expected to be {}.\n",
                         num_transaxial_crystals_per_block,
                         guessed_scanner_ptr->get_num_transaxial_crystals_per_block()));
          mismatch_between_header_and_guess = true;
        }
      if (guessed_scanner_ptr->get_num_axial_crystals_per_singles_unit() > 0
          && num_axial_crystals_per_singles_unit != guessed_scanner_ptr->get_num_axial_crystals_per_singles_unit())
        {
          warning(format("Interfile warning: axial crystals per singles unit ({}) is expected to be {}.\n",
                         num_axial_crystals_per_singles_unit,
                         guessed_scanner_ptr->get_num_axial_crystals_per_singles_unit()));
          mismatch_between_header_and_guess = true;
        }
      if (guessed_scanner_ptr->get_num_transaxial_crystals_per_singles_unit() > 0
          && num_transaxial_crystals_per_singles_unit != guessed_scanner_ptr->get_num_transaxial_crystals_per_singles_unit())
        {
          warning(format("Interfile warning: transaxial crystals per singles unit ({}) is expected to be {}.\n",
                         num_transaxial_crystals_per_singles_unit,
                         guessed_scanner_ptr->get_num_transaxial_crystals_per_singles_unit()));
          mismatch_between_header_and_guess = true;
        }
      if (guessed_scanner_ptr->get_num_detector_layers() > 0
          && num_detector_layers != guessed_scanner_ptr->get_num_detector_layers())
        {
          warning(format("Interfile warning: num_detector_layers ({}) is expected to be {}.\n",
                         num_detector_layers,
                         guessed_scanner_ptr->get_num_detector_layers()));
          mismatch_between_header_and_guess = true;
        }
      //
      // 06/16: N.E: Currently, the energy resolution and the reference energy, are used only in the
      // scatter correction. Therefore a waring is displayed but they don't trigger
      // a mismatch. I assume that the user will handle this. This is in accordance with the
      // scanner '==' operator, which displays a warning message for these two parameters
      // but continues as usual.
      if (energy_resolution > 0)
        {
          if (energy_resolution != guessed_scanner_ptr->get_energy_resolution())
            {
              warning(format("Interfile warning: 'energy resolution' ({:4.3f}) is expected to be {:4.3f}. "
                             "Currently, the energy resolution and the reference energy, are used only in"
                             " scatter correction.",
                             energy_resolution,
                             guessed_scanner_ptr->get_energy_resolution()));
              //    mismatch_between_header_and_guess = true;
            }
          if (reference_energy != guessed_scanner_ptr->get_reference_energy())
            {
              warning(format("Interfile warning: 'reference energy' ({:4.3f}) is expected to be {:4.3f}."
                             "Currently, the energy resolution and the reference energy, are used only in"
                             " scatter correction.",
                             reference_energy,
                             guessed_scanner_ptr->get_reference_energy()));
              //    mismatch_between_header_and_guess = true;
            }
        }

      // new variables for block geometry
      if (fabs(axial_distance_between_crystals_in_cm - guessed_scanner_ptr->get_axial_crystal_spacing() / 10) > .001)
        {
          warning(format("Interfile warning: 'distance between crystals in axial direction (cm)' ({}) is expected to be {}.\n",
                         axial_distance_between_crystals_in_cm,
                         (guessed_scanner_ptr->get_axial_crystal_spacing() / 10)));
          mismatch_between_header_and_guess = true;
        }
      if (fabs(transaxial_distance_between_crystals_in_cm - guessed_scanner_ptr->get_transaxial_crystal_spacing() / 10) > .001)
        {
          warning(
              format("Interfile warning: 'distance between crystals in transaxial direction (cm)' ({}) is expected to be {}.\n",
                     transaxial_distance_between_crystals_in_cm,
                     (guessed_scanner_ptr->get_transaxial_crystal_spacing() / 10)));
          mismatch_between_header_and_guess = true;
        }
      if (fabs(axial_distance_between_blocks_in_cm - guessed_scanner_ptr->get_axial_block_spacing() / 10) > .001)
        {
          warning(format("Interfile warning: 'distance between crystals in axial direction (cm)' ({}) is expected to be {}.\n",
                         axial_distance_between_blocks_in_cm,
                         (guessed_scanner_ptr->get_axial_block_spacing() / 10)));
          mismatch_between_header_and_guess = true;
        }
      if (fabs(transaxial_distance_between_blocks_in_cm - guessed_scanner_ptr->get_transaxial_block_spacing() / 10) > .001)
        {
          warning(format("Interfile warning: 'distance between crystals in axial direction (cm)' ({}) is expected to be {}.\n",
                         transaxial_distance_between_blocks_in_cm,
                         (guessed_scanner_ptr->get_transaxial_block_spacing() / 10)));
          mismatch_between_header_and_guess = true;
        }
      // end of new variables for block geometry

      if (guessed_scanner_ptr->is_tof_ready())
        {
          if (max_num_timing_poss != guessed_scanner_ptr->get_max_num_timing_poss())
            {
              warning(format("Interfile warning: 'Maximum number of (unmashed) TOF time bins' ({}) is expected to be {}.",
                             max_num_timing_poss,
                             guessed_scanner_ptr->get_max_num_timing_poss()));
              mismatch_between_header_and_guess = true;
            }
          if (abs(size_of_timing_pos - guessed_scanner_ptr->get_size_of_timing_pos()) > 0.001F)
            {
              warning(format("Interfile warning: 'Size of unmashed TOF timing bin (ps)' ({}) is expected to be {}.",
                             size_of_timing_pos,
                             guessed_scanner_ptr->get_size_of_timing_pos()));
              mismatch_between_header_and_guess = true;
            }
          if (abs(timing_resolution - guessed_scanner_ptr->get_timing_resolution()) > 0.01F)
            {
              warning(format("Interfile warning: 'TOF timing resolution (ps)' ({}) is expected to be {}.",
                             timing_resolution,
                             guessed_scanner_ptr->get_timing_resolution()));
              mismatch_between_header_and_guess = true;
            }
        }

      // end of checks. If they failed, we ignore the guess
      if (mismatch_between_header_and_guess)
        {
          warning(format("Interfile warning: I have used all explicit settings for the scanner\n"
                         "\tfrom the Interfile header, and remaining fields set from the\n"
                         "\t{} model.\n",
                         guessed_scanner_ptr->get_name().c_str()));
          if (!originating_system_was_recognised)
            guessed_scanner_ptr.reset(new Scanner(Scanner::Unknown_scanner));
        }
    }

  if (guessed_scanner_ptr->get_type() == Scanner::Unknown_scanner
      || guessed_scanner_ptr->get_type() == Scanner::User_defined_scanner)
    {
      // warn if the Interfile header does not provide enough info

      if (num_rings < 1)
        warning("Interfile warning: 'number of rings' invalid.");
      if (num_detectors_per_ring < 1)
        warning("Interfile warning: 'num_detectors_per_ring' invalid.");
#if 0
    if (transaxial_FOV_diameter_in_cm < 0)
      warning("Interfile warning: 'transaxial FOV diameter (cm)' invalid.");
#endif
      if (inner_ring_diameter_in_cm <= 0)
        warning("Interfile warning: 'inner ring diameter (cm)' invalid. This might be disastrous.");
      if (average_depth_of_interaction_in_cm < 0)
        warning("Interfile warning: 'average depth of interaction (cm)' invalid. This might be disastrous.");
      if (distance_between_rings_in_cm <= 0)
        warning("Interfile warning: 'distance between rings (cm)' invalid.");
      if (default_bin_size_in_cm <= 0)
        warning("Interfile warning: 'default_bin size (cm)' invalid. This will likely cause problems in image reconstruction "
                "when setting image sizes via 'zoom' etc.");
      if (num_axial_crystals_per_singles_unit <= 0)
        warning("Interfile warning: 'axial crystals per singles unit' invalid (but currently only used for ECAT dead-time).");
      if (num_transaxial_crystals_per_singles_unit <= 0)
        warning("Interfile warning: 'transaxial crystals per singles unit' invalid (but currently only used for ECAT dead-time)");
      if (scanner_geometry == "BlocksOnCylindrical")
        {
          if (axial_distance_between_crystals_in_cm <= 0)
            warning("Interfile warning: 'distance between crystals in axial direction (cm)' invalid.");
          if (transaxial_distance_between_crystals_in_cm <= 0)
            warning("Interfile warning: 'distance between crystals in transaxial direction (cm)' invalid.");
          if (axial_distance_between_blocks_in_cm <= 0)
            warning("Interfile warning: 'distance between blocks in axial direction (cm)' invalid.");
          if (transaxial_distance_between_blocks_in_cm <= 0)
            warning("Interfile warning: 'distance between blocks in transaxial direction (cm)' invalid.");
        }
    }

  // finally, we construct a new scanner object with
  // data from the Interfile header (or the guessed scanner).

  shared_ptr<Scanner> scanner_sptr_from_file(new Scanner(guessed_scanner_ptr->get_type(),
                                                         get_exam_info_sptr()->originating_system,
                                                         num_detectors_per_ring,
                                                         num_rings,
                                                         max_num_non_arccorrected_bins,
                                                         default_num_arccorrected_bins,
                                                         static_cast<float>(inner_ring_diameter_in_cm * 10. / 2),
                                                         static_cast<float>(average_depth_of_interaction_in_cm * 10),
                                                         static_cast<float>(distance_between_rings_in_cm * 10.),
                                                         static_cast<float>(default_bin_size_in_cm * 10),
                                                         static_cast<float>(view_offset_in_degrees * _PI / 180),
                                                         num_axial_blocks_per_bucket,
                                                         num_transaxial_blocks_per_bucket,
                                                         num_axial_crystals_per_block,
                                                         num_transaxial_crystals_per_block,
                                                         num_axial_crystals_per_singles_unit,
                                                         num_transaxial_crystals_per_singles_unit,
                                                         num_detector_layers,
                                                         energy_resolution,
                                                         reference_energy,
                                                         max_num_timing_poss,
                                                         size_of_timing_pos,
                                                         timing_resolution,
                                                         scanner_geometry,
                                                         static_cast<float>(axial_distance_between_crystals_in_cm * 10.),
                                                         static_cast<float>(transaxial_distance_between_crystals_in_cm * 10.),
                                                         static_cast<float>(axial_distance_between_blocks_in_cm * 10.),
                                                         static_cast<float>(transaxial_distance_between_blocks_in_cm * 10.),
                                                         crystal_map));

  bool is_consistent = scanner_sptr_from_file->check_consistency() == Succeeded::yes;
  if (scanner_sptr_from_file->get_type() == Scanner::Unknown_scanner
      || scanner_sptr_from_file->get_type() == Scanner::User_defined_scanner || mismatch_between_header_and_guess
      || !is_consistent)
    {
      info(format("Interfile parsing ended up with the following scanner:\n{}\n", scanner_sptr_from_file->parameter_info()));
    }

  // float azimuthal_angle_sampling =_PI/num_views;

  if (scanner_geometry == "Cylindrical")
    {
      if (is_arccorrected)
        {
          if (effective_central_bin_size_in_cm <= 0)
            effective_central_bin_size_in_cm = scanner_sptr_from_file->get_default_bin_size() / 10;
          else if (fabs(effective_central_bin_size_in_cm - scanner_sptr_from_file->get_default_bin_size() / 10) > .001)
            warning(format("Interfile warning: unexpected effective_central_bin_size_in_cm\n"
                           "Value in header is {} while the default for the scanner is {}\n"
                           "Using value from header.",
                           effective_central_bin_size_in_cm,
                           (scanner_sptr_from_file->get_default_bin_size() / 10)));

          data_info_sptr.reset(new ProjDataInfoCylindricalArcCorr(scanner_sptr_from_file,
                                                                  float(effective_central_bin_size_in_cm * 10.),
                                                                  sorted_num_rings_per_segment,
                                                                  sorted_min_ring_diff,
                                                                  sorted_max_ring_diff,
                                                                  num_views,
                                                                  num_bins,
                                                                  tof_mash_factor));
        }
      else
        {
          data_info_sptr.reset(new ProjDataInfoCylindricalNoArcCorr(scanner_sptr_from_file,
                                                                    sorted_num_rings_per_segment,
                                                                    sorted_min_ring_diff,
                                                                    sorted_max_ring_diff,
                                                                    num_views,
                                                                    num_bins,
                                                                    tof_mash_factor));
        }
    }
  else if (scanner_geometry == "BlocksOnCylindrical") // if block geometry
    {
      data_info_sptr.reset(new ProjDataInfoBlocksOnCylindricalNoArcCorr(
          scanner_sptr_from_file, sorted_num_rings_per_segment, sorted_min_ring_diff, sorted_max_ring_diff, num_views, num_bins));
    }
  else // if generic geometry
    {
      data_info_sptr.reset(new ProjDataInfoGenericNoArcCorr(
          scanner_sptr_from_file, sorted_num_rings_per_segment, sorted_min_ring_diff, sorted_max_ring_diff, num_views, num_bins));
    }
  if (data_info_sptr->get_num_tof_poss() != num_timing_poss)
    error(format("Interfile header parsing with TOF: inconsistency between number of TOF bins in data ({}), "
                 "TOF mashing factor ({}) and max number of TOF bins in scanner info ({})",
                 num_timing_poss,
                 tof_mash_factor,
                 scanner_sptr_from_file->get_max_num_timing_poss()));

  // cerr << data_info_sptr->parameter_info() << endl;

  // Set the bed position
  data_info_sptr->set_bed_position_horizontal(bed_position_horizontal);
  data_info_sptr->set_bed_position_vertical(bed_position_vertical);

  return false;
}

END_NAMESPACE_STIR
