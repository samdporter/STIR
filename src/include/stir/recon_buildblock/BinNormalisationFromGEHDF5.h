/*
  Copyright (C) 2000-2007, Hammersmith Imanet Ltd
  Copyright (C) 2013-2014, 2020 University College London
  Copyright (C) 2017-2019 University of Leeds

  Largely a copy of the ECAT7 version.

  SPDX-License-Identifier: Apache-2.0

  See STIR/LICENSE.txt for details
*/
/*!
  \file
  \ingroup recon_buildblock
  \ingroup GE
  \brief Declaration of class stir::GE_RDF_HDF5::BinNormalisationFromGEHDF5

  \author Kris Thielemans
  \author Palak Wadhwa
*/

#ifndef __stir_recon_buildblock_BinNormalisationFromGEHDF5_H__
#define __stir_recon_buildblock_BinNormalisationFromGEHDF5_H__

#include "stir/recon_buildblock/BinNormalisation.h"
#include "stir/recon_buildblock/BinNormalisationWithCalibration.h"
#include "stir/RegisteredParsingObject.h"
#include "stir/ProjData.h"
#include "stir/shared_ptr.h"
#include "stir/ProjDataInfoCylindricalNoArcCorr.h"
#include "stir/data/SinglesRates.h"
#include "stir/Scanner.h"
#include "stir/Array.h"
#include <string>

using std::string;

START_NAMESPACE_STIR

class ProjDataInMemory;

namespace GE
{
namespace RDF_HDF5
{

class GEHDF5Wrapper;

/*!
  \ingroup recon_buildblock
  \ingroup GE
  \brief A BinNormalisation class that gets the normalisation factors from
  an GEHDF5 3D normalisation file.

  \par Parsing example
  \verbatim
  Bin Normalisation type := from GE HDF5
  Bin Normalisation From GEHDF5:=
  normalisation filename:= myfile.hn

  ; next keywords can be used to switch off some of the normalisation components
  ; do not use unless you know why
  ; use_detector_efficiencies:=1
  ; use_dead_time:=1
  ; use_geometric_factors:=1
  ; use_crystal_interference_factors:=1
  End Bin Normalisation From GEHDF5:=
  \endverbatim

  \todo dead-time is not yet implemented


*/
class BinNormalisationFromGEHDF5
    : public RegisteredParsingObject<BinNormalisationFromGEHDF5, BinNormalisation, BinNormalisationWithCalibration>
{
private:
  using base_type = BinNormalisationWithCalibration;

public:
  //! Name which will be used when parsing a BinNormalisation object
  static const char* const registered_name;

  //! Default constructor
  /*!
    \warning You should not call any member functions for any object just
    constructed with this constructor. Initialise the object properly first
    by parsing.
  */
  BinNormalisationFromGEHDF5();

  //! Constructor that reads the projdata from a file
  BinNormalisationFromGEHDF5(const string& filename);

  Succeeded set_up(const shared_ptr<const ExamInfo>& exam_info_sptr, const shared_ptr<const ProjDataInfo>&) override;
  float get_uncalibrated_bin_efficiency(const Bin& bin) const override;

  bool use_detector_efficiencies() const;
  bool use_dead_time() const;
  bool use_geometric_factors() const;
  bool use_crystal_interference_factors() const;

private:
  Array<1, float> axial_t1_array;
  Array<1, float> axial_t2_array;
  Array<1, float> trans_t1_array;
  shared_ptr<SinglesRates> singles_rates_ptr;
  Array<2, float> efficiency_factors;
  shared_ptr<ProjDataInMemory> geo_eff_factors_sptr;
  shared_ptr<Scanner> scanner_ptr;
  int num_transaxial_crystals_per_block;
  // TODO move to Scanner
  int num_axial_blocks_per_singles_unit;
  shared_ptr<const ProjDataInfo> proj_data_info_ptr;
  ProjDataInfoCylindricalNoArcCorr const* proj_data_info_cyl_ptr;
  shared_ptr<const ProjDataInfoCylindricalNoArcCorr> proj_data_info_cyl_uncompressed_ptr;
  int span;
  int mash;
  int num_blocks_per_singles_unit;

  bool _use_detector_efficiencies;
  bool _use_dead_time;
  bool _use_geometric_factors;

  void read_norm_data(const string& filename);
  float get_dead_time_efficiency(const DetectionPositionPair<>& detection_position_pair,
                                 const double start_time,
                                 const double end_time) const;

  float get_geometric_efficiency_factors(const DetectionPositionPair<>& detection_position_pair) const;
  float get_efficiency_factors(const DetectionPositionPair<>& detection_position_pair) const;
  // parsing stuff
  void set_defaults() override;
  void initialise_keymap() override;
  bool post_processing() override;

  string normalisation_GEHDF5_filename;
  shared_ptr<GEHDF5Wrapper> m_input_hdf5_sptr;
};

} // namespace RDF_HDF5
} // namespace GE
END_NAMESPACE_STIR

#endif
