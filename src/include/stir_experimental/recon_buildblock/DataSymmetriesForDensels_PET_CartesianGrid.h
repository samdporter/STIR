//
//
/*
    Copyright (C) 2001- 2011,  Hammersmith Imanet Ltd
    This file is part of STIR.

    SPDX-License-Identifier: Apache-2.0

    See STIR/LICENSE.txt for details
*/
/*!

  \file
  \ingroup recon_buildblock

  \brief Declaration of class stir::DataSymmetriesForDensels_PET_CartesianGrid

  \author Kris Thielemans

*/
#ifndef __stir_recon_buildblock_DataSymmetriesForDensels_PET_CartesianGrid_H__
#define __stir_recon_buildblock_DataSymmetriesForDensels_PET_CartesianGrid_H__

#include "stir/recon_buildblock/DataSymmetriesForDensels.h"
#include "stir/ProjDataInfo.h"
//#include "stir/SymmetryOperations_PET_CartesianGrid.h"
//#include "stir/ViewSegmentNumbers.h"
//#include "stir/VoxelsOnCartesianGrid.h"
#include "stir/Densel.h"
#include "stir/shared_ptr.h"
#include <vector>

START_NAMESPACE_STIR

template <int num_dimensions, typename elemT>
class DiscretisedDensity;
template <int num_dimensions, typename elemT>
class DiscretisedDensityOnCartesianGrid;

/*!
  \ingroup recon_buildblock
  \brief Symmetries appropriate for a (cylindrical) PET scanner, and
  a discretised density on a Cartesian grid.

  All operations (except the constructor) are inline as timing of
  the methods of this class is critical.
*/
class DataSymmetriesForDensels_PET_CartesianGrid : public DataSymmetriesForDensels
{
private:
  typedef DataSymmetriesForDensels base_type;
  typedef DataSymmetriesForDensels_PET_CartesianGrid self_type;

public:
  DataSymmetriesForDensels_PET_CartesianGrid(const shared_ptr<const ProjDataInfo>& proj_data_info_ptr,
                                             const shared_ptr<const DiscretisedDensity<3, float>>& image_info_ptr);

#ifndef STIR_NO_COVARIANT_RETURN_TYPES
  DataSymmetriesForDensels_PET_CartesianGrid*
#else
  DataSymmetriesForDensels*
#endif
  clone() const override;

  bool operator==(const DataSymmetriesForDensels_PET_CartesianGrid&) const;

#if 0
  TODO!
  //! returns the range of the indices for basic Densels
  virtual DenselIndexRange
    get_basic_densel_index_range() const;
#endif

  inline void get_related_densels(std::vector<Densel>&, const Densel& b) const override;

  inline int num_related_densels(const Densel& b) const override;

  inline unique_ptr<SymmetryOperation> find_symmetry_operation_from_basic_densel(Densel&) const override;

  inline bool find_basic_densel(Densel& b) const override;

  //! find out how many image planes there are for every scanner ring
  inline float get_num_planes_per_scanner_ring() const;

  //! find correspondence between axial_pos_num and image coordinates
  /*! z = num_planes_per_axial_pos * axial_pos_num + axial_pos_to_z_offset

      compute the offset by matching up the centre of the scanner
      in the 2 coordinate systems
      */
  inline float get_num_planes_per_axial_pos(const int segment_num) const;
  inline float get_axial_pos_to_z_offset(const int segment_num) const;

private:
  const shared_ptr<const ProjDataInfo>& proj_data_info_ptr;
  int num_planes;
  int num_independent_planes;
  int num_views;
  int num_planes_per_scanner_ring;
  //! a list of values for every segment_num
  VectorWithOffset<int> num_planes_per_axial_pos;
  //! a list of values for every segment_num
  VectorWithOffset<float> axial_pos_to_z_offset;

#if 0
  // at the moment, we don't need the following 2 members

  // TODO somehow store only the info
  shared_ptr<const DiscretisedDensity<3,float> > image_info_ptr;

  // a convenience function that does the dynamic_cast from the above
  inline const DiscretisedDensityOnCartesianGrid<3,float> *
    cartesian_grid_info_ptr() const;
#endif

  bool blindly_equals(const root_type* const) const override;

  inline SymmetryOperation* find_sym_op_general_densel(const int z, const int y, const int x) const;
};

END_NAMESPACE_STIR

#include "stir_experimental/recon_buildblock/DataSymmetriesForDensels_PET_CartesianGrid.inl"

#endif
