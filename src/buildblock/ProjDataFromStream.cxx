/*!
  \file
  \ingroup projdata
  \brief Implementations for non-inline functions of class stir::ProjDataFromStream

  \author Nikos Efthimiou
  \author Sanida Mustafovic
  \author Kris Thielemans
  \author Claire Labbe
  \author PARAPET project
*/
/*
    Copyright (C) 2000 PARAPET partners
    Copyright (C) 2000 - 2011-12-21, Hammersmith Imanet Ltd
    Copyright (C) 2011-2012, Kris Thielemans
    Copyright (C) 2013, 2017, 2022, 2023 University College London
    Copyright (C) 2016, University of Hull

    This file is part of STIR.

    SPDX-License-Identifier: Apache-2.0 AND License-ref-PARAPET-license

    See STIR/LICENSE.txt for details
*/

#include "stir/ProjDataFromStream.h"
#include "stir/ExamInfo.h"
#include "stir/Succeeded.h"
#include "stir/Viewgram.h"
#include "stir/Sinogram.h"
#include "stir/SegmentBySinogram.h"
#include "stir/SegmentByView.h"
#include "stir/IndexRange2D.h"
#include "stir/IndexRange3D.h"
#include "stir/utilities.h"
#include "stir/IO/interfile.h"
#include "stir/IO/write_data.h"
#include "stir/IO/read_data.h"
#include "stir/is_null_ptr.h"
#include <numeric>
#include <iostream>
#include <fstream>
#include "stir/warning.h"
#include "stir/error.h"

using std::ios;
using std::iostream;
using std::streamoff;
using std::fstream;
using std::cout;
using std::cerr;
using std::endl;
using std::vector;

START_NAMESPACE_STIR
//---------------------------------------------------------
// constructors
//---------------------------------------------------------

ProjDataFromStream::ProjDataFromStream(shared_ptr<const ExamInfo> const& exam_info_sptr,
                                       shared_ptr<const ProjDataInfo> const& proj_data_info_sptr,
                                       shared_ptr<iostream> const& s,
                                       const streamoff offs,
                                       const vector<int>& segment_sequence_in_stream_v,
                                       StorageOrder o,
                                       NumericType data_type,
                                       ByteOrder byte_order,
                                       float scale_factor)

    : ProjData(exam_info_sptr, proj_data_info_sptr),
      sino_stream(s),
      offset(offs),
      segment_sequence(segment_sequence_in_stream_v),
      storage_order(o),
      on_disk_data_type(data_type),
      on_disk_byte_order(byte_order),
      scale_factor(scale_factor)
{
  assert(storage_order != Unsupported);
  assert(!(data_type == NumericType::UNKNOWN_TYPE));

  if (proj_data_info_sptr->get_num_tof_poss() > 1)
    activate_TOF();
}

ProjDataFromStream::ProjDataFromStream(shared_ptr<const ExamInfo> const& exam_info_sptr,
                                       shared_ptr<const ProjDataInfo> const& proj_data_info_sptr,
                                       shared_ptr<iostream> const& s,
                                       const streamoff offs,
                                       StorageOrder o,
                                       NumericType data_type,
                                       ByteOrder byte_order,
                                       float scale_factor)
    : ProjData(exam_info_sptr, proj_data_info_sptr),
      sino_stream(s),
      offset(offs),
      storage_order(o),
      on_disk_data_type(data_type),
      on_disk_byte_order(byte_order),
      scale_factor(scale_factor)
{
  assert(storage_order != Unsupported);
  assert(!(data_type == NumericType::UNKNOWN_TYPE));

  segment_sequence.resize(proj_data_info_sptr->get_num_segments());

  // N.E. Take this opportunity to calculate the size of the complete -full- 3D sinogram.
  //  We will need that to skip timing positions
  int segment_num, i;

  for (i = 0, segment_num = proj_data_info_sptr->get_min_segment_num(); segment_num <= proj_data_info_sptr->get_max_segment_num();
       ++i, ++segment_num)
    {
      segment_sequence[i] = segment_num;
    }

  if (proj_data_info_sptr->get_num_tof_poss() > 1)
    activate_TOF();
}

void
ProjDataFromStream::activate_TOF()
{
  int sum = 0;
  for (int segment_num = proj_data_info_sptr->get_min_segment_num(); segment_num <= proj_data_info_sptr->get_max_segment_num();
       ++segment_num)
    {
      sum += get_num_axial_poss(segment_num) * get_num_views() * get_num_tangential_poss();
    }

  offset_3d_data = static_cast<streamoff>(sum * on_disk_data_type.size_in_bytes());

  // Now, lets initialise a TOF stream - Similarly to segments
  if (storage_order == Segment_View_AxialPos_TangPos || storage_order == Timing_Segment_View_AxialPos_TangPos)
    storage_order = Timing_Segment_View_AxialPos_TangPos;
  else if (storage_order == Segment_AxialPos_View_TangPos || storage_order == Timing_Segment_AxialPos_View_TangPos)
    storage_order = Timing_Segment_AxialPos_View_TangPos;
  else
    error("ProjDataFromStream:  unsupported storage_order for TOF data");

  timing_poss_sequence.resize(proj_data_info_sptr->get_num_tof_poss());

  for (int i = 0, timing_pos_num = proj_data_info_sptr->get_min_tof_pos_num();
       timing_pos_num <= proj_data_info_sptr->get_max_tof_pos_num();
       ++i, ++timing_pos_num)
    {
      timing_poss_sequence[i] = timing_pos_num;
    }
}

void
ProjDataFromStream::set_timing_poss_sequence_in_stream(const std::vector<int>& seq)
{
  this->timing_poss_sequence = seq;
}

namespace detail
{
// 2 local functions to avoid cluttering code below

static void
checked_seekg(const std::string& fname, std::istream& s, const std::streamoff offset)
{
  s.seekg(offset, ios::beg);
  if (!s)
    error("ProjDataFromStream::" + fname + " error after seekg to offset " + std::to_string(offset));
}

static void
checked_seekp(const std::string& fname, std::ostream& s, const std::streamoff offset)
{
  s.seekp(offset, ios::beg);
  if (!s)
    error("ProjDataFromStream::" + fname + " error after seekp to offset " + std::to_string(offset));
}
} // namespace detail

Viewgram<float>
ProjDataFromStream::get_viewgram(const int view_num,
                                 const int segment_num,
                                 const bool make_num_tangential_poss_odd,
                                 const int timing_pos) const
{
  if (is_null_ptr(sino_stream))
    {
      error("ProjDataFromStream::get_viewgram: stream ptr is 0\n");
    }
  if (!*sino_stream)
    {
      error("ProjDataFromStream::get_viewgram: error in stream state before reading\n");
    }

  Viewgram<float> viewgram(proj_data_info_sptr, view_num, segment_num, timing_pos);
  float scale = float(1);
  Succeeded succeeded = Succeeded::yes;
  Bin bin(segment_num, view_num, this->get_min_axial_pos_num(segment_num), this->get_min_tangential_pos_num(), timing_pos);

#ifdef STIR_OPENMP
#  pragma omp critical(PROJDATAFROMSTREAMIO)
#endif
  try
    {
      if (get_storage_order() == Segment_AxialPos_View_TangPos || get_storage_order() == Timing_Segment_AxialPos_View_TangPos)
        {
          for (bin.axial_pos_num() = get_min_axial_pos_num(segment_num);
               bin.axial_pos_num() <= get_max_axial_pos_num(segment_num);
               bin.axial_pos_num()++)
            {
              detail::checked_seekg("get_viewgram", *sino_stream, get_offset(bin));
              if ((succeeded
                   = read_data(*sino_stream, viewgram[bin.axial_pos_num()], on_disk_data_type, scale, on_disk_byte_order))
                  == Succeeded::no)
                break;
              if (scale != 1)
                break;
            }
        }
      else if (get_storage_order() == Segment_View_AxialPos_TangPos
               || get_storage_order() == Timing_Segment_View_AxialPos_TangPos)
        {
          // read in one go (skipping the extra seek)
          detail::checked_seekg("get_viewgram", *sino_stream, get_offset(bin));
          succeeded = read_data(*sino_stream, viewgram, on_disk_data_type, scale, on_disk_byte_order);
        }
      else
        {
          warning("ProjDataFromStream::get_viewgram: unsupported storage order");
          succeeded = Succeeded::no;
        }
    }
  catch (...)
    {
      succeeded = Succeeded::no;
    }
  // end of critical section
  if (scale != 1)
    error("ProjDataFromStream: error reading data: scale factor returned by read_data should be 1");
  if (succeeded == Succeeded::no)
    error("ProjDataFromStream: error reading data (file truncated?)");

  viewgram *= scale_factor;

  if (make_num_tangential_poss_odd && (get_num_tangential_poss() % 2 == 0))

    {
      const int new_max_tangential_pos = get_max_tangential_pos_num() + 1;

      viewgram.grow(IndexRange2D(get_min_axial_pos_num(segment_num),
                                 get_max_axial_pos_num(segment_num),

                                 get_min_tangential_pos_num(),
                                 new_max_tangential_pos));
    }
  return viewgram;
}

float
ProjDataFromStream::get_bin_value(const Bin& this_bin) const
{
  if (is_null_ptr(sino_stream))
    {
      error("ProjDataFromStream::get_bin_value: stream ptr is 0\n");
    }
  if (!*sino_stream)
    {
      error("ProjDataFromStream::get_bin_value: error in stream state before reading\n");
    }

  detail::checked_seekg("get_bin_value", *sino_stream, get_offset(this_bin));

  Array<1, float> value(1);
  float scale = float(1);

  if (read_data(*sino_stream, value, on_disk_data_type, scale, on_disk_byte_order) == Succeeded::no)
    error("ProjDataFromStream: error reading data\n");
  if (scale != 1.f)
    error("ProjDataFromStream: error reading data: scale factor returned by read_data should be 1\n");

  value *= scale_factor;

  return value[0];
}

void
ProjDataFromStream::set_bin_value(const Bin& this_bin)
{
  if (is_null_ptr(sino_stream))
    {
      error("ProjDataFromStream::set_bin_value: stream ptr is 0\n");
    }
  if (!*sino_stream)
    {
      error("ProjDataFromStream::set_bin_value: error in stream state before writing");
    }

  detail::checked_seekp("set_bin_value", *sino_stream, get_offset(this_bin));

  Array<1, float> value(1);
  value[0] = this_bin.get_bin_value();
  float scale = float(1);
  // Now the storage order is not more important. Just read.
  if (write_data(*sino_stream, value, on_disk_data_type, scale, on_disk_byte_order) == Succeeded::no)
    error("ProjDataFromStream: error writing data\n");
  if (scale != 1.f)
    error("ProjDataFromStream: error writing data: scale factor returned by write_data should be 1\n");
}

Succeeded
ProjDataFromStream::set_viewgram(const Viewgram<float>& v)
{
  if (is_null_ptr(sino_stream))
    {
      warning("ProjDataFromStream::set_viewgram: stream ptr is 0\n");
      return Succeeded::no;
    }
  if (!*sino_stream)
    {
      warning("ProjDataFromStream::set_viewgram: error in stream state before writing\n");
      return Succeeded::no;
    }

  // KT 03/07/2001 modified handling of scale_factor etc.
  if (on_disk_data_type.id != NumericType::FLOAT)
    {
      warning("ProjDataFromStream::set_viewgram: non-float output uses original "
              "scale factor %g which might not be appropriate for the current data\n",
              scale_factor);
    }

  if (get_num_tangential_poss() != v.get_proj_data_info_sptr()->get_num_tangential_poss())
    {
      warning("ProjDataFromStream::set_viewgram: num_bins is not correct\n");
      return Succeeded::no;
    }

  if (get_num_axial_poss(v.get_segment_num()) != v.get_num_axial_poss())
    {
      warning("ProjDataFromStream::set_viewgram: number of axial positions is not correct\n");
      return Succeeded::no;
    }

  if (*get_proj_data_info_sptr() != *(v.get_proj_data_info_sptr()))
    {
      warning("ProjDataFromStream::set_viewgram: viewgram has incompatible ProjDataInfo member\n"
              "Original ProjDataInfo: %s\n"
              "ProjDataInfo From viewgram: %s",
              this->get_proj_data_info_sptr()->parameter_info().c_str(),
              v.get_proj_data_info_sptr()->parameter_info().c_str());

      return Succeeded::no;
    }
  const int segment_num = v.get_segment_num();
  const int view_num = v.get_view_num();
  const int timing_pos = v.get_timing_pos_num();
  Bin bin(segment_num, view_num, this->get_min_axial_pos_num(segment_num), this->get_min_tangential_pos_num(), timing_pos);

  float scale = scale_factor;
  Succeeded succeeded = Succeeded::yes;

#ifdef STIR_OPENMP
#  pragma omp critical(PROJDATAFROMSTREAMIO)
#endif
  try
    {
      if (get_storage_order() == Segment_AxialPos_View_TangPos || get_storage_order() == Timing_Segment_AxialPos_View_TangPos)
        {
          for (bin.axial_pos_num() = get_min_axial_pos_num(segment_num);
               bin.axial_pos_num() <= get_max_axial_pos_num(segment_num);
               bin.axial_pos_num()++)
            {
              detail::checked_seekp("set_viewgram", *sino_stream, get_offset(bin));
              if (write_data(*sino_stream, v[bin.axial_pos_num()], on_disk_data_type, scale, on_disk_byte_order) == Succeeded::no
                  || scale != scale_factor)
                {
                  succeeded = Succeeded::no;
                  break;
                }
            }
        }
      else if (get_storage_order() == Segment_View_AxialPos_TangPos
               || get_storage_order() == Timing_Segment_View_AxialPos_TangPos)
        {
          // write in one go (skipping the extra seek)
          detail::checked_seekp("set_viewgram", *sino_stream, get_offset(bin));
          if (write_data(*sino_stream, v, on_disk_data_type, scale, on_disk_byte_order) == Succeeded::no || scale != scale_factor)
            {
              succeeded = Succeeded::no;
            }
        }
      else
        {
          warning("ProjDataFromStream::set_viewgram: unsupported storage order");
          succeeded = Succeeded::no;
        }
      // flush the stream, see the class documentation
      sino_stream->flush();
    }
  catch (...)
    {
      succeeded = Succeeded::no;
    }
  // end of critical section
  if (succeeded == Succeeded::no)
    error("ProjDataFromStream::set_viewgram: viewgram (view=%d, segment=%d, timing_pos=%d)"
          " corrupted due to problems with writing or the scale factor (out of disk space?)",
          view_num,
          segment_num,
          timing_pos);

  return succeeded;
}

std::streamoff
ProjDataFromStream::get_offset(const Bin& this_bin) const
{

  if (!(this_bin.segment_num() >= get_min_segment_num() && this_bin.segment_num() <= get_max_segment_num()))
    error("ProjDataFromStream::get_offset: segment_num out of range : %d", this_bin.segment_num());

  if (!(this_bin.axial_pos_num() >= get_min_axial_pos_num(this_bin.segment_num())
        && this_bin.axial_pos_num() <= get_max_axial_pos_num(this_bin.segment_num())))
    error("ProjDataFromStream::get_offset: axial_pos_num out of range : %d", this_bin.axial_pos_num());
  if (!(this_bin.timing_pos_num() >= get_min_tof_pos_num() && this_bin.timing_pos_num() <= get_max_tof_pos_num()))
    error("ProjDataFromStream::get_offset: timing_num out of range : %d", this_bin.timing_pos_num());

  const int index = static_cast<int>(std::find(segment_sequence.begin(), segment_sequence.end(), this_bin.segment_num())
                                     - segment_sequence.begin());

  streamoff num_axial_pos_offset = 0;

  for (int i = 0; i < index; i++)
    num_axial_pos_offset += get_num_axial_poss(segment_sequence[i]);

  streamoff segment_offset = offset
                             + static_cast<streamoff>(num_axial_pos_offset * get_num_tangential_poss() * get_num_views()
                                                      * on_disk_data_type.size_in_bytes());

  // Now we are just in front of  the correct segment
  if (get_storage_order() == Segment_AxialPos_View_TangPos || get_storage_order() == Timing_Segment_AxialPos_View_TangPos)
    {
      if (proj_data_info_sptr->get_num_tof_poss() > 1)
        {
          // The timing offset will be added to the segment offset to minimise the changes
          const int timing_index
              = static_cast<int>(std::find(timing_poss_sequence.begin(), timing_poss_sequence.end(), this_bin.timing_pos_num())
                                 - timing_poss_sequence.begin());

          assert(offset_3d_data > 0);
          segment_offset += static_cast<streamoff>(timing_index) * offset_3d_data;
        }
      // skip axial positions
      const streamoff ax_pos_offset = (this_bin.axial_pos_num() - get_min_axial_pos_num(this_bin.segment_num())) * get_num_views()
                                      * get_num_tangential_poss() * on_disk_data_type.size_in_bytes();

      // sinogram location

      // find view
      const streamoff view_offset
          = (this_bin.view_num() - get_min_view_num()) * get_num_tangential_poss() * on_disk_data_type.size_in_bytes();

      // find tang pos
      const streamoff tang_offset
          = (this_bin.tangential_pos_num() - get_min_tangential_pos_num()) * on_disk_data_type.size_in_bytes();

      return segment_offset + ax_pos_offset + view_offset + tang_offset;
    }
  else if (get_storage_order() == Segment_View_AxialPos_TangPos || get_storage_order() == Timing_Segment_View_AxialPos_TangPos)
    {
      if (proj_data_info_sptr->get_num_tof_poss() > 1)
        {
          // The timing offset will be added to the segment offset. This approach we minimise the changes
          const int timing_index
              = static_cast<int>(std::find(timing_poss_sequence.begin(), timing_poss_sequence.end(), this_bin.timing_pos_num())
                                 - timing_poss_sequence.begin());

          assert(offset_3d_data > 0);
          segment_offset += static_cast<streamoff>(timing_index) * offset_3d_data;
        }
      // Skip views
      const streamoff view_offset = (this_bin.view_num() - get_min_view_num()) * get_num_axial_poss(this_bin.segment_num())
                                    * get_num_tangential_poss() * on_disk_data_type.size_in_bytes();

      // find axial pos
      const streamoff ax_pos_offset = (this_bin.axial_pos_num() - get_min_axial_pos_num(this_bin.segment_num()))
                                      * get_num_tangential_poss() * on_disk_data_type.size_in_bytes();

      // find tang pos
      const streamoff tang_offset
          = (this_bin.tangential_pos_num() - get_min_tangential_pos_num()) * on_disk_data_type.size_in_bytes();

      return segment_offset + ax_pos_offset + view_offset + tang_offset;
    }
  else
    {
      error("ProjDataFromStream::get_offset: unsupported storage order");
      return streamoff(0); // return something to avoid compiler warning
    }
}

Sinogram<float>
ProjDataFromStream::get_sinogram(const int ax_pos_num,
                                 const int segment_num,
                                 const bool make_num_tangential_poss_odd,
                                 const int timing_pos) const
{
  if (is_null_ptr(sino_stream))
    {
      error("ProjDataFromStream::get_sinogram: stream ptr is 0");
    }
  if (!*sino_stream)
    {
      error("ProjDataFromStream::get_sinogram: error in stream state before reading");
    }

  Sinogram<float> sinogram(proj_data_info_sptr, ax_pos_num, segment_num, timing_pos);
  float scale = float(1);
  Succeeded succeeded = Succeeded::yes;
  Bin bin(segment_num, this->get_min_view_num(), ax_pos_num, this->get_min_tangential_pos_num(), timing_pos);

#ifdef STIR_OPENMP
#  pragma omp critical(PROJDATAFROMSTREAMIO)
#endif
  try
    {
      if (get_storage_order() == Segment_AxialPos_View_TangPos || get_storage_order() == Timing_Segment_AxialPos_View_TangPos)
        {
          detail::checked_seekg("get_sinogram", *sino_stream, get_offset(bin));
          succeeded = read_data(*sino_stream, sinogram, on_disk_data_type, scale, on_disk_byte_order);
        }
      else if (get_storage_order() == Segment_View_AxialPos_TangPos
               || get_storage_order() == Timing_Segment_View_AxialPos_TangPos)
        {
          for (bin.view_num() = get_min_view_num(); bin.view_num() <= get_max_view_num(); bin.view_num()++)
            {
              detail::checked_seekg("get_sinogram", *sino_stream, get_offset(bin));
              if ((succeeded = read_data(*sino_stream, sinogram[bin.view_num()], on_disk_data_type, scale, on_disk_byte_order))
                  == Succeeded::no)
                break;
              if (scale != 1)
                break;
            }
        }
      else
        {
          warning("ProjDataFromStream::get_sinogram: unsupported storage order");
          succeeded = Succeeded::no;
        }
    }
  catch (...)
    {
      succeeded = Succeeded::no;
    }
  // end of critical section
  if (scale != 1)
    error("ProjDataFromStream: error reading data: scale factor returned by read_data should be 1");
  if (succeeded == Succeeded::no)
    error("ProjDataFromStream: error reading data");

  sinogram *= scale_factor;

  if (make_num_tangential_poss_odd && (get_num_tangential_poss() % 2 == 0))
    {
      int new_max_tangential_pos = get_max_tangential_pos_num() + 1;

      sinogram.grow(IndexRange2D(get_min_view_num(), get_max_view_num(), get_min_tangential_pos_num(), new_max_tangential_pos));
    }

  return sinogram;
}

Succeeded
ProjDataFromStream::set_sinogram(const Sinogram<float>& s)
{
  if (is_null_ptr(sino_stream))
    {
      warning("ProjDataFromStream::set_sinogram: stream ptr is 0\n");
      return Succeeded::no;
    }
  if (!*sino_stream)
    {
      warning("ProjDataFromStream::set_sinogram: error in stream state before writing\n");
      return Succeeded::no;
    }
  // KT 03/07/2001 modified handling of scale_factor etc.
  if (on_disk_data_type.id != NumericType::FLOAT)
    {
      warning("ProjDataFromStream::set_sinogram: non-float output uses original "
              "scale factor %g which might not be appropriate for the current data\n",
              scale_factor);
    }

  if (*get_proj_data_info_sptr() != *(s.get_proj_data_info_sptr()))
    {
      warning("ProjDataFromStream::set_sinogram: Sinogram<float> has incompatible ProjDataInfo member.\n"
              "Original ProjDataInfo: %s\n"
              "ProjDataInfo from sinogram: %s",
              this->get_proj_data_info_sptr()->parameter_info().c_str(),
              s.get_proj_data_info_sptr()->parameter_info().c_str());

      return Succeeded::no;
    }
  int segment_num = s.get_segment_num();
  int ax_pos_num = s.get_axial_pos_num();
  int timing_pos = s.get_timing_pos_num();
  Bin bin(segment_num, this->get_min_view_num(), ax_pos_num, this->get_min_tangential_pos_num(), timing_pos);
  float scale = scale_factor;

  Succeeded succeeded = Succeeded::yes;
#ifdef STIR_OPENMP
#  pragma omp critical(PROJDATAFROMSTREAMIO)
#endif
  try
    {
      if (get_storage_order() == Segment_AxialPos_View_TangPos || get_storage_order() == Timing_Segment_AxialPos_View_TangPos)
        {
          detail::checked_seekp("set_sinogram", *sino_stream, get_offset(bin));
          if (write_data(*sino_stream, s, on_disk_data_type, scale, on_disk_byte_order) == Succeeded::no || scale != scale_factor)
            {
              warning("ProjDataFromStream::set_sinogram: sinogram (ax_pos=%d, segment=%d)"
                      " corrupted due to problems with writing or the scale factor \n",
                      ax_pos_num,
                      segment_num);
              succeeded = Succeeded::no;
            }
        }
      else if (get_storage_order() == Segment_View_AxialPos_TangPos
               || get_storage_order() == Timing_Segment_View_AxialPos_TangPos)
        {
          for (bin.view_num() = get_min_view_num(); bin.view_num() <= get_max_view_num(); bin.view_num()++)
            {
              detail::checked_seekp("set_sinogram", *sino_stream, get_offset(bin));
              if (write_data(*sino_stream, s[bin.view_num()], on_disk_data_type, scale, on_disk_byte_order) == Succeeded::no
                  || scale != scale_factor)
                {
                  warning("ProjDataFromStream::set_sinogram: sinogram (ax_pos=%d, segment=%d)"
                          " corrupted due to problems with writing or the scale factor \n",
                          ax_pos_num,
                          segment_num);
                  succeeded = Succeeded::no;
                  break;
                }
            }
        }
      else
        {
          warning("ProjDataFromStream::set_sinogram: unsupported storage order");
          succeeded = Succeeded::no;
        }
      // flush the stream, see the class documentation
      sino_stream->flush();
    }
  catch (...)
    {
      succeeded = Succeeded::no;
    }
  // end of critical section
  return succeeded;
}

SegmentBySinogram<float>
ProjDataFromStream::get_segment_by_sinogram(const int segment_num, const int timing_num) const
{
  if (is_null_ptr(sino_stream))
    {
      error("ProjDataFromStream::get_segment_by_sinogram: stream ptr is 0\n");
    }
  if (!*sino_stream)
    {
      error("ProjDataFromStream::get_segment_by_sinogram: error in stream state before reading\n");
    }

  if (get_storage_order() == Segment_AxialPos_View_TangPos || get_storage_order() == Timing_Segment_AxialPos_View_TangPos)
    {
      SegmentBySinogram<float> segment(proj_data_info_sptr, segment_num, timing_num);
      float scale = float(1);
      Succeeded succeeded = Succeeded::yes;
      const Bin bin(segment_num,
                    this->get_min_view_num(),
                    this->get_min_axial_pos_num(segment_num),
                    this->get_min_tangential_pos_num(),
                    timing_num);
#ifdef STIR_OPENMP
#  pragma omp critical(PROJDATAFROMSTREAMIO)
#endif
      try
        {
          detail::checked_seekg("get_segment_by_sinogram", *sino_stream, get_offset(bin));
          succeeded = read_data(*sino_stream, segment, on_disk_data_type, scale, on_disk_byte_order);
        }
      catch (...)
        {
          succeeded = Succeeded::no;
        }
      // end of critical section
      if (succeeded == Succeeded::no)
        error("ProjDataFromStream: error reading data\n");
      if (scale != 1)
        error("ProjDataFromStream: error reading data: scale factor returned by read_data should be 1\n");

      segment *= scale_factor;
      return segment;
    }
  else
    {
      // TODO rewrite in terms of get_viewgram
      return SegmentBySinogram<float>(get_segment_by_view(segment_num, timing_num));
    }
}

SegmentByView<float>
ProjDataFromStream::get_segment_by_view(const int segment_num, const int timing_pos) const
{
  if (is_null_ptr(sino_stream))
    {
      error("ProjDataFromStream::get_segment_by_view: stream ptr is 0\n");
    }
  if (!*sino_stream)
    {
      error("ProjDataFromStream::get_segment_by_view: error in stream state before reading\n");
    }

  if (get_storage_order() == Segment_View_AxialPos_TangPos || get_storage_order() == Timing_Segment_View_AxialPos_TangPos)
    {
      SegmentByView<float> segment(proj_data_info_sptr, segment_num, timing_pos);
      float scale = float(1);
      Succeeded succeeded = Succeeded::yes;
      const Bin bin(segment_num,
                    this->get_min_view_num(),
                    this->get_min_axial_pos_num(segment_num),
                    this->get_min_tangential_pos_num(),
                    timing_pos);
#ifdef STIR_OPENMP
#  pragma omp critical(PROJDATAFROMSTREAMIO)
#endif
      try
        {
          detail::checked_seekg("get_segment_by_view", *sino_stream, get_offset(bin));
          succeeded = read_data(*sino_stream, segment, on_disk_data_type, scale, on_disk_byte_order);
        }
      catch (...)
        {
          succeeded = Succeeded::no;
        }
      // end of critical section
      if (succeeded == Succeeded::no)
        error("ProjDataFromStream: error reading data");
      if (scale != 1)
        error("ProjDataFromStream: error reading data: scale factor returned by read_data should be 1\n");

      segment *= scale_factor;
      return segment;
    }
  else
    // TODO rewrite in terms of get_sinogram as this doubles memory temporarily
    return SegmentByView<float>(get_segment_by_sinogram(segment_num, timing_pos));
}

Succeeded
ProjDataFromStream::set_segment(const SegmentBySinogram<float>& segmentbysinogram_v)
{
  if (is_null_ptr(sino_stream))
    {
      error("ProjDataFromStream::set_segment: stream ptr is 0\n");
    }
  if (!*sino_stream)
    {
      error("ProjDataFromStream::set_segment: error in stream state before writing\n");
    }

  if (get_num_tangential_poss() != segmentbysinogram_v.get_num_tangential_poss())
    {
      warning("ProjDataFromStream::set_segmen: num_bins is not correct\n");
      return Succeeded::no;
    }
  if (get_num_views() != segmentbysinogram_v.get_num_views())
    {
      warning("ProjDataFromStream::set_segment: num_views is not correct\n");
      return Succeeded::no;
    }

  const int segment_num = segmentbysinogram_v.get_segment_num();
  const Bin bin(segment_num,
                this->get_min_view_num(),
                this->get_min_axial_pos_num(segment_num),
                this->get_min_tangential_pos_num(),
                segmentbysinogram_v.get_timing_pos_num());

  if (get_storage_order() == Segment_AxialPos_View_TangPos || get_storage_order() == Timing_Segment_AxialPos_View_TangPos)
    {
      // KT 03/07/2001 handle scale_factor appropriately
      if (on_disk_data_type.id != NumericType::FLOAT)
        {
          warning("ProjDataFromStream::set_segment: non-float output uses original "
                  "scale factor %g which might not be appropriate for the current data\n",
                  scale_factor);
        }
      float scale = scale_factor;
      Succeeded succeeded = Succeeded::yes;
#ifdef STIR_OPENMP
#  pragma omp critical(PROJDATAFROMSTREAMIO)
#endif
      try
        {
          detail::checked_seekp("set_segment", *sino_stream, get_offset(bin));
          if (write_data(*sino_stream, segmentbysinogram_v, on_disk_data_type, scale, on_disk_byte_order) == Succeeded::no
              || scale != scale_factor)
            {
              warning("ProjDataFromStream::set_segment: segment (%d) tof bin (%d)"
                      " corrupted due to problems with writing or the scale factor \n",
                      segment_num,
                      segmentbysinogram_v.get_timing_pos_num());
              succeeded = Succeeded::no;
            }
          // flush the stream, see the class documentation
          sino_stream->flush();
        }
      catch (...)
        {
          succeeded = Succeeded::no;
        }
      // end of critical section
      return succeeded;
    }
  else
    {
      // TODO rewrite in terms of set_viewgram
      const SegmentByView<float> segmentbyview = SegmentByView<float>(segmentbysinogram_v);

      return set_segment(segmentbyview);
    }
}

Succeeded
ProjDataFromStream::set_segment(const SegmentByView<float>& segmentbyview_v)
{
  if (is_null_ptr(sino_stream))
    {
      error("ProjDataFromStream::set_segment: stream ptr is 0\n");
    }
  if (!*sino_stream)
    {
      error("ProjDataFromStream::set_segment: error in stream state before writing\n");
    }

  if (get_num_tangential_poss() != segmentbyview_v.get_num_tangential_poss())
    {
      warning("ProjDataFromStream::set_segment: num_bins is not correct\n");
      return Succeeded::no;
    }
  if (get_num_views() != segmentbyview_v.get_num_views())
    {
      warning("ProjDataFromStream::set_segment: num_views is not correct\n");
      return Succeeded::no;
    }

  const int segment_num = segmentbyview_v.get_segment_num();
  const Bin bin(segment_num,
                this->get_min_view_num(),
                this->get_min_axial_pos_num(segment_num),
                this->get_min_tangential_pos_num(),
                segmentbyview_v.get_timing_pos_num());

  if (get_storage_order() == Segment_View_AxialPos_TangPos || get_storage_order() == Timing_Segment_View_AxialPos_TangPos)
    {
      // KT 03/07/2001 handle scale_factor appropriately
      if (on_disk_data_type.id != NumericType::FLOAT)
        {
          warning("ProjDataFromStream::set_segment: non-float output uses original "
                  "scale factor %g which might not be appropriate for the current data\n",
                  scale_factor);
        }
      float scale = scale_factor;
      Succeeded succeeded = Succeeded::yes;
#ifdef STIR_OPENMP
#  pragma omp critical(PROJDATAFROMSTREAMIO)
#endif
      try
        {
          detail::checked_seekp("set_segment", *sino_stream, get_offset(bin));
          if (write_data(*sino_stream, segmentbyview_v, on_disk_data_type, scale, on_disk_byte_order) == Succeeded::no
              || scale != scale_factor)
            {
              warning("ProjDataFromStream::set_segment: segment (%d) tof bin (%d)"
                      " corrupted due to problems with writing or the scale factor \n",
                      segment_num,
                      segmentbyview_v.get_timing_pos_num());
              succeeded = Succeeded::no;
            }
          // flush the stream, see the class documentation
          sino_stream->flush();
        }
      catch (...)
        {
          succeeded = Succeeded::no;
        }
      // end of critical section
      return succeeded;
    }
  else
    {
      // TODO rewrite in terms of set_sinogram
      const SegmentBySinogram<float> segmentbysinogram = SegmentBySinogram<float>(segmentbyview_v);
      return set_segment(segmentbysinogram);
    }
}

#if 0
ProjDataFromStream* ProjDataFromStream::ask_parameters(const bool on_disk)
{

 shared_ptr<iostream> p_in_stream;


    char filename[256];

    ask_filename_with_extension(
      filename,
      "Enter file name of 3D sinogram data : ", ".scn");


    // KT 03/07/2001 initialise to avoid compiler warnings
    ios::openmode  open_mode=ios::in;
    switch(ask_num("Read (1), Create and write(2), Read/Write (3) : ", 1,3,1))
    {
      case 1: open_mode=ios::in; break;
      case 2: open_mode=ios::out; break;
      case 3: open_mode=ios::in | ios::out; break;
      }

    if (on_disk)
    {

      //fstream * p_fstream = new fstream;
      p_in_stream.reset(new fstream (filename, open_mode | ios::binary));
      if (!p_in_stream->good())
       {
         error("ProjDataFromStream::ask_parameters: error opening file %s\n",filename);
       }
      //open_read_binary(*p_fstream, filename);
      //p_in_stream = p_fstream;
    }
    else
    {
      streamsize file_size = 0;
      char *memory = 0;
      {
        fstream input;
        open_read_binary(input, filename);
        memory = (char *)read_stream_in_memory(input, file_size);
      }

      // TODO this does allocate and copy 2 times
          // TODO file_size could be longer than what size_t allows, but string doesn't take anything longer
      p_in_stream.reset(new std::stringstream (string(memory, std::size_t(file_size)),
                                           open_mode | ios::binary));

      delete[] memory;

    } // else 'on_disk'


    // KT 03/07/2001 initialise to avoid compiler warnings
    ProjDataFromStream::StorageOrder storage_order =
      Segment_AxialPos_View_TangPos;
    {
    int data_org = ask_num("Type of data organisation:\n\
      0: Segment_AxialPos_View_TangPos, 1: Segment_View_AxialPos_TangPos",
                           0,
                           1,0);

    switch (data_org)
    {
    case 0:
      storage_order = ProjDataFromStream::Segment_AxialPos_View_TangPos;
      break;
    case 1:
      storage_order =ProjDataFromStream::Segment_View_AxialPos_TangPos;
      break;
    }
    }

    NumericType data_type;
    {
    int data_type_sel = ask_num("Type of data :\n\
      0: signed 16bit int, 1: unsigned 16bit int, 2: 4bit float ", 0,2,2);
    switch (data_type_sel)
    {
    case 0:
      data_type = NumericType::SHORT;
      break;
    case 1:
      data_type = NumericType::USHORT;
      break;
    case 2:
      data_type = NumericType::FLOAT;
      break;
    }
    }


    ByteOrder byte_order;
    {
      byte_order =
        ask("Little endian byte order ?",
        ByteOrder::get_native_order() == ByteOrder::little_endian) ?
        ByteOrder::little_endian :
      ByteOrder::big_endian;
    }

    std::streamoff offset_in_file ;
    {
      // find file size
      p_in_stream->seekg(static_cast<std::streamoff>(0), ios::beg);
      streamsize file_size = find_remaining_size(*p_in_stream);

      offset_in_file = ask_num("Offset in file (in bytes)",
                               static_cast<std::streamoff>(0),
                               static_cast<std::streamoff>(file_size),
                               static_cast<std::streamoff>(0));
    }
    float scale_factor =1;

    shared_ptr<ProjDataInfo> data_info_ptr(ProjDataInfo::ask_parameters());

    vector<int> segment_sequence_in_stream;
    segment_sequence_in_stream = vector<int>(data_info_ptr->get_num_segments());
    segment_sequence_in_stream[0] =  0;

    for (int i=1; i<= data_info_ptr->get_num_segments()/2; i++)
    {
      segment_sequence_in_stream[2*i-1] = i;
      segment_sequence_in_stream[2*i] = -i;
    }


    cerr << "Segment<float> sequence :";
    for (unsigned int i=0; i<segment_sequence_in_stream.size(); i++)
      cerr << segment_sequence_in_stream[i] << "  ";
    cerr << endl;



    ProjDataFromStream* proj_data_ptr =
      new
      ProjDataFromStream (exam_info_sptr,
              data_info_ptr,
                          p_in_stream, offset_in_file,
                          segment_sequence_in_stream,
                          storage_order,data_type,byte_order,
                          scale_factor);

    cerr << "writing Interfile header for "<< filename << endl;
    write_basic_interfile_PDFS_header(filename, *proj_data_ptr);

    return proj_data_ptr;

}

#endif

float
ProjDataFromStream::get_scale_factor() const
{
  return scale_factor;
}

END_NAMESPACE_STIR
