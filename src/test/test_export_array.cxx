/*
    Copyright (C) 2016, 2020 University College London

    This file is part of STIR.

    SPDX-License-Identifier: Apache-2.0

    See STIR/LICENSE.txt for details
*/

/*!
  \brief Test class for exporting and importing ProjData
  as arrays.
  \ingroup test
  \author Nikos Efthimiou
  \author Kris Thielemans
*/

#include "stir/utilities.h"
#include "stir/RunTests.h"
#include "stir/ProjDataInMemory.h"
#include "stir/ProjDataInterfile.h"
#include "stir/Scanner.h"
#include "stir/ExamInfo.h"
#include "stir/SegmentByView.h"
#include "stir/Array.h"
#include "stir/IndexRange2D.h"
#include "stir/IndexRange3D.h"
#include "stir/info.h"
#include "stir/warning.h"
#include "stir/error.h"
#include "stir/format.h"
#include "stir/copy_fill.h"

#include "stir/ProjData.h"
#include "stir/DynamicProjData.h"
#include <iostream>

START_NAMESPACE_STIR

class ExportArrayTests : public RunTests
{
public:
  void run_tests() override;

protected:
  void test_static_data();
  void test_dynamic_data();
  void run_static_test(ProjData& test_proj_data, ProjData& check_proj_data, const std::string& test_name);
  void check_if_equal_projdata(const ProjData& test_proj_data, const ProjData& check_proj_data, const std::string& test_name);
};

void
ExportArrayTests ::run_tests()
{
  test_static_data();
  test_dynamic_data();
}

void
ExportArrayTests ::check_if_equal_projdata(const ProjData& test_proj_data,
                                           const ProjData& check_proj_data,
                                           const std::string& test_name)
{
  for (int segment_num = test_proj_data.get_min_segment_num(); segment_num <= test_proj_data.get_max_segment_num(); ++segment_num)
    {
      const SegmentByView<float> test_segment_by_view_data = test_proj_data.get_segment_by_view(segment_num);

      const SegmentByView<float> check_segment_by_view_data = check_proj_data.get_segment_by_view(segment_num);

      for (int view_num = test_segment_by_view_data.get_min_view_num(); view_num <= test_segment_by_view_data.get_max_view_num();
           ++view_num)
        {
          Viewgram<float> test_view = test_segment_by_view_data.get_viewgram(view_num);
          Viewgram<float> check_view = check_segment_by_view_data.get_viewgram(view_num);

          for (int axial = test_view.get_min_axial_pos_num(); axial <= test_view.get_max_axial_pos_num(); ++axial)
            {
              for (int s = test_view.get_min_tangential_pos_num(); s <= test_view.get_max_tangential_pos_num(); ++s)
                {
                  check_if_equal(
                      test_view[axial][s], check_view[axial][s], test_name + ": test ProjData different from check ProjData.");
                  check_if_equal(
                      check_view[axial][s], (float)segment_num, test_name + ": check ProjData different from segment number.");
                }
            }
        }
    }
}

//!
//! \brief ExportArrayTests::test_dynamic_data
//! \details This test will try to write DynamicProjData in a 2D array.
//! Where, each 1D row will correspond to one frame. Then the data will be
//! written and retrieved from the disk, resorted in DynamicProjData and
//! compared with the original. The test will fails if the array retrieved
//! from the disk is of different size from the original.
void
ExportArrayTests::test_dynamic_data()
{
  info("Initialising...");
  //. Create ProjData

  //- ProjDataInfo
  //-- Scanner
  shared_ptr<Scanner> test_scanner_sptr(new Scanner(Scanner::Siemens_mMR));

  //-- ExamInfo
  shared_ptr<ExamInfo> test_exam_info_sptr(new ExamInfo());
  // TODO, Currently all stir::Scanner types are PET.
  test_exam_info_sptr->imaging_modality = ImagingModality::PT;

  info("Creating test DynamicProjData...");
  shared_ptr<DynamicProjData> test_dynamic_projData_sptr(new DynamicProjData(test_exam_info_sptr));

  shared_ptr<ProjDataInfo> tmp_proj_data_info_sptr(
      ProjDataInfo::ProjDataInfoCTI(test_scanner_sptr,
                                    1,
                                    1, /* Reduce the number of segments */
                                    test_scanner_sptr->get_max_num_views(),
                                    test_scanner_sptr->get_max_num_non_arccorrected_bins(),
                                    false));

  const int num_of_gates = 3;
  info(format("Resizing the DynamicProjData for {} gates... ", num_of_gates));
  test_dynamic_projData_sptr->resize(num_of_gates);

  for (int i_gate = 1; i_gate <= num_of_gates; i_gate++)
    {
      info(format("Allocating and filling the {} gate... ", i_gate));

      shared_ptr<ProjData> test_proj_data_gate_ptr(new ProjDataInMemory(test_exam_info_sptr, tmp_proj_data_info_sptr));

      for (int segment_num = test_proj_data_gate_ptr->get_min_segment_num();
           segment_num <= test_proj_data_gate_ptr->get_max_segment_num();
           ++segment_num)
        {
          SegmentByView<float> segment_by_view_data = test_proj_data_gate_ptr->get_segment_by_view(segment_num);

          // 1000 is an arbitary number to distiguish data in different gates.
          segment_by_view_data.fill(static_cast<float>(segment_num + (i_gate * 1000)));

          if (!(test_proj_data_gate_ptr->set_segment(segment_by_view_data) == Succeeded::yes))
            warning("Error set_segment %d\n", segment_num);
        }

      info("Populating the Dynamic ProjData... ");
      test_dynamic_projData_sptr->set_proj_data_sptr(test_proj_data_gate_ptr, i_gate);
    }

  const std::size_t total_size = test_dynamic_projData_sptr->size_all();
  const int total_gates = static_cast<int>(test_dynamic_projData_sptr->get_num_proj_data());
  const int projdata_size = static_cast<int>(test_dynamic_projData_sptr->get_proj_data_size());

  info(format("Total size: {}, number of gates: {}, size of projdata {}", total_size, total_gates, projdata_size));
  // Allocate 2D array to store the data.
  info("Allocating test array...");
  Array<2, float> test_array(IndexRange2D(0, total_gates, 0, projdata_size));
  test_array.fill(-1);
  Array<2, float>::full_iterator test_array_iter = test_array.begin_all();

  // Copy data to array.
  info("Copying test dynamic projdata to array ...");
  copy_to(*test_dynamic_projData_sptr, test_array_iter);

  // Convert it to ProjData
  info("Copying data from array to check dynamic projdata ...");

  shared_ptr<DynamicProjData> check_dynamic_projData_sptr(new DynamicProjData(test_exam_info_sptr, num_of_gates));

  for (int i_gate = 0; i_gate < num_of_gates; i_gate++)
    {
      info(format("Allocating and filling the {} gate... ", (i_gate + 1)));

      shared_ptr<ProjData> test_proj_data_gate_ptr(new ProjDataInMemory(test_exam_info_sptr, tmp_proj_data_info_sptr));

      info("Populating the Dynamic ProjData... ");
      check_dynamic_projData_sptr->set_proj_data_sptr(test_proj_data_gate_ptr, (i_gate + 1));
    }

  fill_from(*check_dynamic_projData_sptr, test_array.begin_all_const(), test_array.end_all_const());

  info("Checking if data are the same...");
  for (int i_gate = 1; i_gate <= num_of_gates; i_gate++)
    {
      shared_ptr<ProjData> _test_projdata_sptr(test_dynamic_projData_sptr->get_proj_data_sptr(i_gate));
      shared_ptr<ProjData> _check_projdata_sptr(check_dynamic_projData_sptr->get_proj_data_sptr(i_gate));

      for (int segment_num = _test_projdata_sptr->get_min_segment_num();
           segment_num <= _test_projdata_sptr->get_max_segment_num();
           ++segment_num)
        {
          SegmentByView<float> _test_segment_by_view_data = _test_projdata_sptr->get_segment_by_view(segment_num);

          SegmentByView<float> _check_segment_by_view_data = _check_projdata_sptr->get_segment_by_view(segment_num);

          for (int view_num = _test_segment_by_view_data.get_min_view_num();
               view_num <= _test_segment_by_view_data.get_max_view_num();
               ++view_num)
            {
              Viewgram<float> _test_view = _test_segment_by_view_data.get_viewgram(view_num);
              Viewgram<float> _check_view = _check_segment_by_view_data.get_viewgram(view_num);

              for (int axial = _test_view.get_min_axial_pos_num(); axial <= _test_view.get_max_axial_pos_num(); ++axial)
                {
                  for (int s = _test_view.get_min_tangential_pos_num(); s <= _test_view.get_max_tangential_pos_num(); ++s)
                    {
                      check_if_equal(_test_view[axial][s], _check_view[axial][s], "Test ProjData different from check ProjData.");
                      check_if_equal(_check_view[axial][s],
                                     (float)(segment_num + (i_gate * 1000)),
                                     "Check ProjData different from segment number.");
                    }
                }
            }
        }
    }
}

void
ExportArrayTests ::run_static_test(ProjData& test_proj_data, ProjData& check_proj_data, const std::string& test_name)
{
  std::cerr << "Running " << test_name << '\n';

  //. Fill ProjData with the number of the segment
  info("Filling test ProjData with the segment number ... ");

  for (int segment_num = test_proj_data.get_min_segment_num(); segment_num <= test_proj_data.get_max_segment_num(); ++segment_num)
    {
      info(format("Segment: {} ", segment_num));
      SegmentByView<float> segment_by_view_data = test_proj_data.get_empty_segment_by_view(segment_num);

      segment_by_view_data.fill(static_cast<float>(segment_num));

      if (!(test_proj_data.set_segment(segment_by_view_data) == Succeeded::yes))
        error("Error set_segment %d\n", segment_num);
    }

  //- Get the total size of the ProjData

  const unsigned int total_size = test_proj_data.size_all();

  //- Allocate 1D array and get iterator

  info("Allocating array ...");
  Array<3, float> test_array(
      IndexRange3D(test_proj_data.get_num_sinograms(), test_proj_data.get_num_views(), test_proj_data.get_num_tangential_poss()));
  check(test_array.size_all() == total_size, "check on size of array");
  Array<3, float>::full_iterator test_array_iter = test_array.begin_all();

  //-
  info("Copying from ProjData to array ...");
  copy_to(test_proj_data, test_array_iter);

  //- Check if segment order is as expected
  {
    const std::vector<int> segment_sequence = ProjData::standard_segment_sequence(*test_proj_data.get_proj_data_info_sptr());
    test_array_iter = test_array.begin_all();
    for (std::vector<int>::const_iterator iter = segment_sequence.begin(); iter != segment_sequence.end(); ++iter)
      {
        const int seg = *iter;
        const SegmentBySinogram<float> segment = test_proj_data.get_segment_by_sinogram(seg);
        for (SegmentBySinogram<float>::const_full_iterator seg_iter = segment.begin_all(); seg_iter != segment.end_all();
             ++seg_iter, ++test_array_iter)
          {
            if (!check_if_equal(*seg_iter, *test_array_iter, "check if array in correct order"))
              {
                // one failed, so many others will as well. we just stop checking.
                // make sure we get out of the outer loop as well
                iter = segment_sequence.end() - 1;
                break;
              }
          }
      }
  }

  // Convert it back to ProjData
  info("Copying from array to a new ProjData ...");
  fill_from(check_proj_data, test_array.begin_all_const(), test_array.end_all_const());
  this->check_if_equal_projdata(test_proj_data, check_proj_data, test_name);
}

//!
//! \brief ExportArrayTests::test_static_data
//! \details This test will chech if projection data copied to arrays are the
//! same when copied back to projdata.
//!
void
ExportArrayTests ::test_static_data()
{
  info("Initialising...");
  //. Create ProjData

  //- ProjDataInfo
  //-- Scanner
  shared_ptr<Scanner> test_scanner_sptr(new Scanner(Scanner::Siemens_mMR));

  //-- ExamInfo
  shared_ptr<ExamInfo> test_exam_info_sptr(new ExamInfo());
  // TODO, Currently all stir::Scanner types are PET.
  test_exam_info_sptr->imaging_modality = ImagingModality::PT;

  //-
  shared_ptr<ProjDataInfo> tmp_proj_data_info_sptr(
      ProjDataInfo::ProjDataInfoCTI(test_scanner_sptr,
                                    1,
                                    1,
                                    test_scanner_sptr->get_max_num_views(),
                                    test_scanner_sptr->get_max_num_non_arccorrected_bins(),
                                    false));

  {
    ProjDataInMemory test_proj_data(test_exam_info_sptr, tmp_proj_data_info_sptr);
    {
      ProjDataInMemory check_proj_data(test_exam_info_sptr, tmp_proj_data_info_sptr);
      run_static_test(test_proj_data, check_proj_data, "static test in-memory");
    }
    {
      ProjDataInterfile check_proj_data(test_exam_info_sptr,
                                        tmp_proj_data_info_sptr,
                                        "test_proj_data_export_check.hs",
                                        std::ios::out | std::ios::trunc | std::ios::in);
      run_static_test(test_proj_data, check_proj_data, "static test in-memory/interfile");
    }
  }
  {
    ProjDataInterfile test_proj_data(
        test_exam_info_sptr, tmp_proj_data_info_sptr, "test_proj_data_export.hs", std::ios::out | std::ios::trunc | std::ios::in);
    {
      ProjDataInMemory check_proj_data(test_exam_info_sptr, tmp_proj_data_info_sptr);
      run_static_test(test_proj_data, check_proj_data, "static test in-memory");
    }
    {
      ProjDataInterfile check_proj_data(test_exam_info_sptr,
                                        tmp_proj_data_info_sptr,
                                        "test_proj_data_export_check.hs",
                                        std::ios::out | std::ios::trunc | std::ios::in);
      run_static_test(test_proj_data, check_proj_data, "static test in-memory/interfile");
    }
  }
}

END_NAMESPACE_STIR

USING_NAMESPACE_STIR
int
main(int argc, char** argv)
{
  ExportArrayTests tests;
  tests.run_tests();
  return tests.main_return_value();
}
