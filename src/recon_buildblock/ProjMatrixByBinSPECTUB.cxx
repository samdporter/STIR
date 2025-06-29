/*
    Copyright (C) 2013, Institute for Bioengineering of Catalonia
    Copyright (C) Biomedical Image Group (GIB), Universitat de Barcelona, Barcelona, Spain.
    Copyright (C) 2013-2014, 2019, 2020, 2023 University College London
    Copyright (C) 2023 National Physical Laboratory
    This file is part of STIR.

    SPDX-License-Identifier: Apache-2.0

    See STIR/LICENSE.txt for details
*/
/*!
  \file
  \ingroup projection

  \brief Implementation of class stir::ProjMatrixByBinSPECTUB

  \author Berta Marti Fuster
  \author Carles Falcon
  \author Kris Thielemans
  \author Daniel Deidda
*/

#include "stir/recon_buildblock/ProjMatrixByBinSPECTUB.h"
#include "stir/recon_buildblock/TrivialDataSymmetriesForBins.h"
#include "stir/ProjDataInfoCylindricalArcCorr.h"
#include "stir/IO/read_from_file.h"
#include "stir/ProjDataInfo.h"
#include "stir/VoxelsOnCartesianGrid.h"
#include "stir/Succeeded.h"
#include "stir/is_null_ptr.h"
#include "stir/Coordinate3D.h"
#include "stir/info.h"
#include "stir/warning.h"
#include "stir/error.h"
#include "stir/format.h"
#include "stir/CPUTimer.h"
#ifdef STIR_OPENMP
#  include "stir/num_threads.h"
#endif

//#include "boost/cstdint.hpp"
//#include "boost/scoped_ptr.hpp"
#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/algorithm/string.hpp>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cmath>

//... user defined libraries .............................................................

#include "stir/recon_buildblock/SPECTUB_Weight3d.h"

using namespace SPECTUB;
START_NAMESPACE_STIR

const char* const ProjMatrixByBinSPECTUB::registered_name = "SPECT UB";

ProjMatrixByBinSPECTUB::ProjMatrixByBinSPECTUB()
{
  set_defaults();
}

void
ProjMatrixByBinSPECTUB::initialise_keymap()
{
  parser.add_start_key("Projection Matrix By Bin SPECT UB Parameters");
  ProjMatrixByBin::initialise_keymap();

  // no longer parse this
  // parser.add_key("minimum weight", &minimum_weight);
  parser.add_key("maximum number of sigmas", &maximum_number_of_sigmas);
  // no longer parse this
  // parser.add_key("spatial resolution PSF", &spatial_resolution_PSF);
  parser.add_key("psf type", &psf_type);
  parser.add_key("collimator sigma 0(cm)", &collimator_sigma_0);
  parser.add_key("collimator slope", &collimator_slope);
  parser.add_key("attenuation type", &attenuation_type);
  parser.add_key("attenuation map", &attenuation_map);
  parser.add_key("mask type", &mask_type);
  parser.add_key("mask file", &mask_file);
  parser.add_key("keep_all_views_in_cache", &keep_all_views_in_cache);

  parser.add_stop_key("End Projection Matrix By Bin SPECT UB Parameters");
}

void
ProjMatrixByBinSPECTUB::set_defaults()
{
  ProjMatrixByBin::set_defaults();

  this->already_setup = false;

  this->keep_all_views_in_cache = false;
  minimum_weight = 0.0;
  maximum_number_of_sigmas = 2.;
  spatial_resolution_PSF = 0.00001;
  psf_type = "Geometrical";
  collimator_slope = 0.;
  collimator_sigma_0 = 0.;
  attenuation_type = "no";
  attenuation_map = "";
  mask_type = "no";
  mask_file = "";
}

bool
ProjMatrixByBinSPECTUB::post_processing()
{
  if (ProjMatrixByBin::post_processing() == true)
    return true;

  this->set_attenuation_type(this->attenuation_type);
  if (!this->attenuation_map.empty())
    this->set_attenuation_image_sptr(this->attenuation_map);
  else
    this->attenuation_image_sptr.reset();

  this->already_setup = false;

  return false;
}

bool
ProjMatrixByBinSPECTUB::get_keep_all_views_in_cache() const
{
  return this->keep_all_views_in_cache;
}

void
ProjMatrixByBinSPECTUB::set_keep_all_views_in_cache(bool value)
{
  if (this->keep_all_views_in_cache != value)
    {
      this->keep_all_views_in_cache = value;
      this->already_setup = false;
    }
}

std::string
ProjMatrixByBinSPECTUB::get_attenuation_type() const
{
  return this->attenuation_type;
}

void
ProjMatrixByBinSPECTUB::set_attenuation_type(const std::string& value)
{
  if (this->attenuation_type != boost::algorithm::to_lower_copy(value))
    {
      this->attenuation_type = boost::algorithm::to_lower_copy(value);
      if (this->attenuation_type != "no" && this->attenuation_type != "simple" && this->attenuation_type != "full")
        error("attenuation_type has to be No, Simple or Full");
      this->already_setup = false;
    }
}

shared_ptr<const DiscretisedDensity<3, float>>
ProjMatrixByBinSPECTUB::get_attenuation_image_sptr() const
{
  return this->attenuation_image_sptr;
}

void
ProjMatrixByBinSPECTUB::set_attenuation_image_sptr(const shared_ptr<const DiscretisedDensity<3, float>> value)
{
  this->attenuation_image_sptr = value;
  this->attenuation_map = "";
  if (this->attenuation_type == "no")
    {
      info("Setting attenuation type to 'simple'");
      this->set_attenuation_type("simple");
    }
  this->already_setup = false;
}

void
ProjMatrixByBinSPECTUB::set_attenuation_image_sptr(const std::string& value)
{
  this->attenuation_map = value;
  shared_ptr<DiscretisedDensity<3, float>> im_sptr(read_from_file<DiscretisedDensity<3, float>>(this->attenuation_map));
  set_attenuation_image_sptr(im_sptr);
}

void
ProjMatrixByBinSPECTUB::set_resolution_model(const float collimator_sigma_0_in_mm,
                                             const float collimator_slope,
                                             const bool full_3D)
{
  // convert sigma_0 to cm. slope is dimensionless
  this->collimator_sigma_0 = collimator_sigma_0_in_mm / 10;
  this->collimator_slope = collimator_slope;
  if (collimator_slope == 0.F && collimator_sigma_0 == 0.F)
    {
      this->psf_type = "geometrical";
    }
  else if (full_3D)
    {
      this->psf_type = "3d";
    }
  else
    {
      this->psf_type = "2d";
    }
  this->already_setup = false;
}

void
ProjMatrixByBinSPECTUB::set_up(const shared_ptr<const ProjDataInfo>& proj_data_info_ptr_v,
                               const shared_ptr<const DiscretisedDensity<3, float>>& density_info_ptr // TODO should be Info only
)
{

  ProjMatrixByBin::set_up(proj_data_info_ptr_v, density_info_ptr);

#ifdef STIR_OPENMP
  if (!this->keep_all_views_in_cache)
    {
      warning("SPECTUB matrix can currently only use single-threaded code unless all views are kept. Setting num_threads to 1");
      set_num_threads(1);
    }
#endif

  const VoxelsOnCartesianGrid<float>* image_info_ptr = dynamic_cast<const VoxelsOnCartesianGrid<float>*>(density_info_ptr.get());

  if (image_info_ptr == NULL)
    error("ProjMatrixByBinFromFile set-up with a wrong type of DiscretisedDensity\n");

  if (this->already_setup)
    {
      if (this->densel_range == image_info_ptr->get_index_range() && this->voxel_size == image_info_ptr->get_voxel_size()
          && this->origin == image_info_ptr->get_origin() && *proj_data_info_ptr_v == *this->proj_data_info_ptr)
        {
          // stored matrix should be compatible, so we can just reuse it
          return;
        }
      else
        {
          this->clear_cache();
          this->delete_UB_SPECT_arrays();
        }
    }

  this->proj_data_info_ptr = proj_data_info_ptr_v;
  symmetries_sptr.reset(new TrivialDataSymmetriesForBins(proj_data_info_ptr_v));

  this->densel_range = image_info_ptr->get_index_range();
  this->voxel_size = image_info_ptr->get_voxel_size();
  this->origin = image_info_ptr->get_origin();

  const ProjDataInfoCylindricalArcCorr* proj_Data_Info_Cylindrical
      = dynamic_cast<const ProjDataInfoCylindricalArcCorr*>(this->proj_data_info_ptr.get());

  CPUTimer timer;
  timer.start();

  //... fill prj structure from projection data info

  prj.Nbin = this->proj_data_info_ptr->get_num_tangential_poss();
  prj.szcm = this->proj_data_info_ptr->get_scanner_ptr()->get_default_bin_size() / 10.;
  prj.Nang = this->proj_data_info_ptr->get_num_views();

  //... fill vol structure from image_info_ptr
  vol.Ncol = image_info_ptr->get_x_size();               // Image: number of columns
  vol.Nrow = image_info_ptr->get_y_size();               // Image: number of rows
  vol.Nsli = image_info_ptr->get_z_size();               // Image: and projections: number of slices
  vol.szcm = image_info_ptr->get_voxel_size().x() / 10.; // Image: voxel size (cm)
  vol.thcm = image_info_ptr->get_voxel_size().z() / 10.; // Image: slice thickness (cm)

  //..... geometrical and other derived parameters of the volume structure...............
  vol.Npix = vol.Ncol * vol.Nrow;
  vol.Nvox = vol.Npix * vol.Nsli;

  vol.Ncold2 = (float)vol.Ncol / (float)2.; // half of the matrix Nvox (integer or semi-integer)
  vol.Nrowd2 = (float)vol.Nrow / (float)2.; // half of the matrix NbOS (integer or semi-integer)
  vol.Nslid2 = (float)vol.Nsli / (float)2.; // half of the number of slices (integer or semi-integer)

  vol.Xcmd2 = vol.Ncold2 * vol.szcm; // Half of the size of the image volume, dimension x (cm);
  vol.Ycmd2 = vol.Nrowd2 * vol.szcm; // Half of the size of the image volume, dimension y (cm);
  vol.Zcmd2 = vol.Nslid2 * vol.thcm; // Half of the size of the image volume, dimension z (cm);

  vol.x0 = ((float)0.5 - vol.Ncold2) * vol.szcm; // coordinate x of the first voxel
  vol.y0 = ((float)0.5 - vol.Nrowd2) * vol.szcm; // coordinate y of the first voxel
  vol.z0 = ((float)0.5 - vol.Nslid2) * vol.thcm; // coordinate z of the first voxel

  vol.first_sl = 0;       // Image: first slice to take into account (no weight bellow)
  vol.last_sl = vol.Nsli; // Image: last slice to take into account (no weights above)

  wmh.vol = vol;

  //...... geometrical dimensions of the voxel structure .................
  vox.szcm = vol.szcm;
  vox.thcm = vol.thcm;

  //... projecction parameters ..........................................
  prj.ang0 = this->proj_data_info_ptr->get_scanner_ptr()->get_intrinsic_azimuthal_tilt() * float(180 / _PI);
  prj.incr = proj_Data_Info_Cylindrical->get_azimuthal_angle_sampling() * float(180 / _PI);
  prj.thcm = proj_Data_Info_Cylindrical->get_axial_sampling(0) / 10;

  //.......geometrical and other derived parameters of projection structure...........
  prj.Nsli = proj_Data_Info_Cylindrical->get_num_axial_poss(0); // number of slices
  prj.lngcm = prj.Nbin * prj.szcm;                              // length in cm of the detection line
  prj.Nbp = prj.Nbin * prj.Nsli;                                // number of bins for each projection angle (2D-projection)
  prj.Nbt = prj.Nbp * prj.Nang;                                 // total number of bins considering all the projection angles
  prj.Nbind2 = (float)prj.Nbin / (float)2.; // half of the number of bins in a detection line (integer or semi-integer)
  prj.lngcmd2 = prj.lngcm / (float)2.;      // half of the length of detection line (cm)
  prj.Nslid2 = (float)prj.Nsli / (float)2.; // half of the number of slices (integer or semi-integer)

  //... number of subsets (always 1 in STIR) ................................................
  prj.NOS = prj.Nang;              // number of subset in which to split the weight matrix
  prj.NangOS = prj.Nang / prj.NOS; // number of angles of projection in each subset
  prj.NbOS = prj.Nbt / prj.NOS;    // total number of bins in each subset

  wmh.prj = prj;
  // wmh.NpixAngOS = vol.Npix * prj.NangOS;

  if (std::abs(wmh.prj.thcm - vox.thcm) > .01F)
    error(format("SPECTUB Matrix (probably) only works with equal z-sampling for projection data ({}) and image ({})",
                 (wmh.prj.thcm * 10),
                 (vol.thcm * 10)));
  if (std::abs(wmh.prj.Nsli - vol.Nsli) > .01F)
    error(format("SPECTUB Matrix (probably) only works with equal number of slices for projection data ({}) and image ({})",
                 wmh.prj.Nsli,
                 vol.Nsli));
  //....rotation radius .................................................
  const VectorWithOffset<float> radius_all_views = proj_Data_Info_Cylindrical->get_ring_radii_for_all_views();

  {
    const auto max_radius = *std::max_element(radius_all_views.begin(), radius_all_views.end());
    const auto max_im_radius = std::max(vol.Xcmd2, vol.Ycmd2) * 10;
    if (max_im_radius > max_radius)
      {
        warning("Image radius (" + std::to_string(max_im_radius) + " is larger than max detector radius ("
                + std::to_string(max_radius) + "). Are you sure this is correct? (Proceeding anyway)");
      }
  }
  Rrad = new float[wmh.prj.Nang];
  for (int i = 0; i < wmh.prj.Nang; i++)
    {
      // note: convert to cm for UB SPECT library
      Rrad[i] = radius_all_views[i] / 10;
    }

  //... resolution parameters ..............................................
  wmh.min_w = minimum_weight;
  wmh.maxsigm = maximum_number_of_sigmas;
  wmh.psfres = spatial_resolution_PSF;

  bin.szcm = wmh.prj.szcm;
  bin.szcmd2 = bin.szcm / (float)2.;
  bin.thcm = wmh.prj.thcm;
  bin.thcmd2 = bin.thcm / (float)2.;
  bin.szdx = bin.szcm / wmh.psfres;
  bin.thdx = bin.thcm / wmh.psfres;

  std::stringstream info_stream;

  //....PSF and collimator parameters ........................................
  boost::algorithm::to_lower(psf_type);
  if (psf_type == "geometrical")
    {
      wmh.do_psf = false;
      wmh.do_psf_3d = false;
    }
  else
    {
      wmh.do_psf = true;

      if (psf_type == "3d")
        {
          wmh.do_psf_3d = true;
          info_stream << "3D PSF Correction. Parallel geometry" << std::endl;
        }
      else
        {
          if (psf_type == "2d")
            {
              wmh.do_psf_3d = false;
              info_stream << "2D PSF Correction. Parallel geometry" << std::endl;
            }
          else
            {
              // error_wm_SPECT( 120, psf_type );
              error("PSF type has to be 2D, 3D or Geometrical");
            }
        }
    }

  wmh.predef_col = false;
  wmh.COL.A = collimator_slope;
  wmh.COL.B = collimator_sigma_0;
  // no fan-beam
  wmh.COL.do_fb = false;

  if (!wmh.do_psf)
    // code to enable fan-beam collimator at some point... (not validated)
    // before enabling, you will have to change the parameter file to give parameters of the fan-beam
    // int num = collimator_number;
    // if ( wmh.COL.do_fb ==0 ) {
    info_stream << "No correction for PSF. Parallel geometry" << std::endl;
  //}
  // else {
  //	info_stream << "No correction for PSF. Fanbeam geometry with focal distance = " << wmh.COL.F << " cm " << std::endl;
  //}

  //... attenuation parameters .........................
  boost::algorithm::to_lower(attenuation_type);
  wmh.att_fn = this->attenuation_map;

  if (attenuation_type == "no")
    {
      wmh.do_full_att = wmh.do_att = false;
    }
  else
    {
      wmh.do_att = true;

      if (attenuation_type == "simple")
        wmh.do_full_att = false;
      else
        {
          if (attenuation_type == "full")
            wmh.do_full_att = true;
          else
            {
              // error_wm_SPECT( 123, attenuation_type );
              error("attenuation_type has to be No, Simple or Full");
            }
        }
    }

  //... masking parameters.............................
  boost::algorithm::to_lower(mask_type);
  if (mask_type == "no")
    {
      wmh.do_msk = wmh.do_msk_cyl = wmh.do_msk_att = wmh.do_msk_file = false;
    }
  else
    {
      wmh.do_msk = true;
      if (mask_type == "cylinder")
        {
          wmh.do_msk_cyl = true;
          wmh.do_msk_att = wmh.do_msk_file = false;
        }
      else
        {
          if (mask_type == "attenuation map")
            {
              wmh.do_msk_att = true;
              wmh.do_msk_cyl = wmh.do_msk_file = false;
            }
          else
            {
              if (mask_type == "explicit mask")
                {
                  wmh.do_msk_file = true;
                  wmh.do_msk_cyl = wmh.do_msk_att = false;
                  wmh.msk_fn = mask_file;

                  info_stream << "MASK filename = " << wmh.msk_fn << std::endl;
                }
              else
                {
                  // error_wm_SPECT( 125, mask_type);
                  error("mask_type has to be No, Cylinder, Attenuation Map or Explicit Mask");
                }
            }
        }
    }

  wmh.do_msk_slc = false;

  if (vol.first_sl > 0 || vol.last_sl < vol.Nsli)
    {
      wmh.do_msk = true;
      wmh.do_msk_slc = true;
    }

  wm.do_save_STIR = true;

  //:: Control of read parameters
  info_stream << "" << std::endl;
  info_stream << "Parameters of SPECT UB matrix: (in cm)" << std::endl;
  info_stream << "Image grid side row: " << wmh.vol.Nrow << "\tcol: " << wmh.vol.Ncol
              << "\ttransverse voxel_size: " << wmh.vol.szcm << std::endl;
  info_stream << "Number of slices: " << wmh.vol.Nsli << "\tslice_thickness: " << wmh.vol.thcm << std::endl;
  info_stream << "Number of bins: " << wmh.prj.Nbin << "\tbin size: " << wmh.prj.szcm << "\taxial size: " << wmh.prj.thcm
              << std::endl;
  info_stream << "Number of angles: " << wmh.prj.Nang << "\tAngle increment: " << wmh.prj.incr
              << "\tFirst angle: " << wmh.prj.ang0 << std::endl;
  info_stream << "Number of subsets: " << wmh.prj.NOS << std::endl;
  if (wmh.do_att)
    {
      info_stream << "Correction for attenuation: " << wmh.att_fn << "\t\tdo_msk_att: " << wmh.do_msk_att << std::endl;
      info_stream << "Attenuation map: " << wmh.att_fn << std::endl;
    }
  info_stream << "Rotation radii: {" << Rrad[0];
  for (int i = 1; i < prj.Nang; ++i)
    {
      info_stream << ", " << Rrad[i];
    }
  info_stream << "}\n";
  info_stream << "Minimum weight: " << wmh.min_w << std::endl;

  info(info_stream.str());

  //... to sort angles into subsets ......................................

  prj.order = new int[prj.Nang];
  index_calc(prj.order, wmh);

  //... to fill ang structure ............................................

  ang = new angle_type[prj.Nang];
  fill_ang(ang, wmh, Rrad);

  //... to fill high resolution discrete distribution functions ..............

  if (!wmh.do_psf)
    {

      //... trapezoid projection of a square voxel on a line .................

      for (int i = 0; i < prj.Nang; i++)
        {

          ang[i].vxprj.val = new float[ang[i].vxprj.lng];
          ang[i].vxprj.acu = new float[ang[i].vxprj.lng];

          calc_vxprj(&ang[i]);
        }
    }
  else
    {

      //... Gaussian density and distribution functions ........................

      gaussdens.lngd2
          = (int)(wmh.maxsigm / wmh.psfres); // half of the length of the gaussian density function (in resolution elements)
      gaussdens.lng = gaussdens.lngd2 * 2;   // length of the gaussian density function (in resolution elements).
      gaussdens.res = wmh.psfres;

      gaussdens.val = new float[gaussdens.lng + 1]; // density function allocation
      gaussdens.acu = new float[gaussdens.lng];     // distribution function allocation
      calc_gauss(&gaussdens);                       // to calculate N(0,1) density function and distribution function
    }

  //... to read attenuation map ..................................................

  if (wmh.do_att || wmh.do_msk_att)
    {
      if (is_null_ptr(attenuation_image_sptr))
        error("Attenation image not set");
      std::string explanation;
      if (!density_info_ptr->has_same_characteristics(*attenuation_image_sptr, explanation))
        error("Currently the attenuation map and emission image must have the same dimension, orientation and voxel size:\n"
              + explanation);

      attmap = new float[vol.Nvox];
      std::copy(attenuation_image_sptr->begin_all(), attenuation_image_sptr->end_all(), attmap);
      for (int i = 0; i < wmh.vol.Nvox; i++)
        {
          if ((boost::math::isnan)(attmap[i]))
            {
              attmap[i] = 0;
            }
        }
      // read_att_map( attmap );
    }
  else
    attmap = NULL;

  //... to generate mask..........................................................

  if (wmh.do_msk)
    {
      msk_3d = new bool[vol.Nvox];
      msk_2d = new bool[vol.Npix];
      if (!wmh.do_msk_att && wmh.do_msk_file)
        {
          shared_ptr<DiscretisedDensity<3, float>> mask_sptr(read_from_file<DiscretisedDensity<3, float>>(wmh.msk_fn));
          if (!density_info_ptr->has_same_characteristics(*mask_sptr))
            error("Currently the mask image and emission image must have the same dimension, orientation and voxel size");
          float* mask_from_file = new float[vol.Nvox];
          std::copy(mask_sptr->begin_all(), mask_sptr->end_all(), mask_from_file);
          // call UB generate_msk pretending that this mask is an attenuation image
          // we do this to avoid using its own read_msk_file
          wmh.do_msk_file = false;
          wmh.do_msk_att = true;
          generate_msk(msk_3d, msk_2d, mask_from_file, &vol, wmh);
          delete[] mask_from_file;
        }
      else
        {
          generate_msk(msk_3d, msk_2d, attmap, &vol, wmh);
        }
    }
  else
    msk_2d = msk_3d = NULL;

  //... Initialization and memory allocation for the weight matrix ...................

  wm.NbOS = prj.NbOS; // number of rows in the weight matrix
  wm.Nvox = vol.Nvox; // number of columnes in the weight matrix

  //... setting PSF maximum size (in bins) and memory allocation for PSF values .......

  this->maxszb = max_psf_szb(ang, wmh); // maximum PSF size (horizontal component of PSF)
  NITEMS = new int*[prj.NOS];
  for (int kOS = 0; kOS < prj.NOS; ++kOS)
    {
      NITEMS[kOS] = new int[wm.NbOS];
    }

  //... double array wm.val and wm.col .....................................................

  if ((wm.val = new (std::nothrow) float*[wm.NbOS]) == NULL)
    {
      // error_wm_SPECT( 200, "wm.val[]" );
      error("Error allocating space to store values for SPECTUB matrix");
    }
  if ((wm.col = new (std::nothrow) int*[wm.NbOS]) == NULL)
    {
      // error_wm_SPECT( 200, "wm.col[]" );
      error("Error allocating space to store column indices for SPECTUB matrix");
    }

  //... array wm.ne .........................................................................

  if ((wm.ne = new (std::nothrow) int[wm.NbOS + 1]) == 0)
    {
      // error_wm_SPECT(200,"wm.ne[]");
      error("Error allocating space to store number of elements for SPECTUB matrix");
    }

  //... STIR indices .......................................................................

  if (wm.do_save_STIR)
    {
      wm.ns = new int[prj.NbOS];
      wm.nb = new int[prj.NbOS];
      wm.na = new int[prj.NbOS];

      wm.nx = new short int[vol.Nvox];
      wm.ny = new short int[vol.Nvox];
      wm.nz = new short int[vol.Nvox];
    }

  //... memory allocation for wmh .........................................................

  wmh.index = new int[wmh.prj.NangOS];
  wmh.Rrad = new float[wmh.prj.NangOS];

  //..........................................................................................
  //... CALCULATION OF MATRICES ..............................................................
  //..........................................................................................

  //... LOOP: Subsets .................................................................
  subset_already_processed.assign(prj.NOS, false);
  for (int kOS = 0; kOS < prj.NOS; kOS++)
    {
      wmh.subset_ind = kOS;

      for (int i = 0; i < prj.NangOS; i++)
        {

          wmh.index[i] = prj.order[i + kOS * prj.NangOS];
          wmh.Rrad[i] = Rrad[wmh.index[i]];
          if (wmh.Rrad[i] != wmh.Rrad[0])
            wmh.fixed_Rrad = false;
        }

      //... NITEMS initialization  ......................

      for (int i = 0; i < prj.NbOS; i++)
        NITEMS[kOS][i] = 1;

      //... size estimations ........................................................

      wm_size_estimation(kOS, ang, vox, bin, vol, prj, msk_3d, msk_2d, maxszb, &gaussdens, NITEMS[kOS], wmh, Rrad);

      // cout << "\nwm_SPECT. Size estimation done. time (s): " << double( clock()-ini )/CLOCKS_PER_SEC <<std::endl;

      // compute_one_subset(kOS);
    } // end of LOOP: Subsets

  // delete_UB_SPECT_arrays();
  info(format("Done estimating size of matrix. Execution (CPU) time {} s ", timer.value()), 2);
  // wm_SPECT ends here ---------------------------------------------------------------------------------------------

  this->already_setup = true;
}

ProjMatrixByBinSPECTUB*
ProjMatrixByBinSPECTUB::clone() const
{
  // we deleted the copy constructor as it's not safe with all those bare pointers, so cannot do this
  // return new ProjMatrixByBinSPECTUB(*this);
  error("ProjMatrixByBinSPECTUB::clone not implemented yet");
  return 0;
}

ProjMatrixByBinSPECTUB::~ProjMatrixByBinSPECTUB()
{
  delete_UB_SPECT_arrays();
}

void
ProjMatrixByBinSPECTUB::delete_UB_SPECT_arrays()
{
  if (!this->already_setup)
    return;
  //... freeing matrix memory....................................
  delete[] Rrad;

  if (!wmh.do_psf)
    {
      for (int i = 0; i < prj.Nang; i++)
        {
          delete[] ang[i].vxprj.val;
          delete[] ang[i].vxprj.acu;
        }
    }

  delete[] wm.val;
  delete[] wm.col;
  delete[] wm.ne;

  //... freeing memory .............................................

  delete[] prj.order;
  delete[] ang;
  for (int kOS = 0; kOS < prj.NOS; ++kOS)
    delete[] NITEMS[kOS];
  delete[] NITEMS;
  delete[] wmh.index;
  delete[] wmh.Rrad;

  if (wmh.do_psf)
    {
      delete[] gaussdens.val;
      delete[] gaussdens.acu;
    }
  if (wmh.do_att)
    delete[] attmap;

  if (wmh.do_msk)
    {
      delete[] msk_3d;
      delete[] msk_2d;
    }

  if (wm.do_save_STIR)
    {
      delete[] wm.ns;
      delete[] wm.nb;
      delete[] wm.na;
      delete[] wm.nx;
      delete[] wm.ny;
      delete[] wm.nz;
    }
}
void
ProjMatrixByBinSPECTUB::compute_one_subset(const int kOS, const float* Rrad) const
{

  CPUTimer timer;
  timer.start();
  // cout << "\n\n--- Processing subset: " << kOS+1 << "/" << prj.NOS << " ----------------------------------------\n" << endl;

  //... to fill wmh fields related to the subset ..................................

  this->wmh.subset_ind = kOS;

  for (int i = 0; i < prj.NangOS; i++)
    {

      this->wmh.index[i] = prj.order[i + kOS * prj.NangOS];
      this->wmh.Rrad[i] = Rrad[this->wmh.index[i]];
    }

  //... NITEMS initialization  ......................

  // for ( int i = 0 ; i < prj.NbOS ; i++ ) NITEMS[ i ] = 1;

  //... size esmitations ........................................................

  // wm_size_estimation ( kOS,  ang, vox, bin, vol, prj, msk_3d, msk_2d, maxszb, &gaussdens, NITEMS );

  // cout << "\nwm_SPECT. Size estimation done. time (s): " << double( clock()-ini )/CLOCKS_PER_SEC <<endl;

  int ne = 0;

  for (int i = 0; i < this->wmh.prj.NbOS; i++)
    ne += NITEMS[kOS][i];

  //... size information ....................................................................

  info(format("total number of non-zero weights in this view: {}, estimated size: {} MB",
              ne,
              (this->wm.do_save_STIR ? (ne + 10 * prj.NbOS) / 104857.6 : ne / 131072)),
       2);

  //... memory allocation for wm float arrays ...................................

  for (int i = 0; i < this->wmh.prj.NbOS; i++)
    {

      if ((this->wm.val[i] = new (std::nothrow) float[NITEMS[kOS][i]]) == NULL)
        {
          // error_wm_SPECT( 200, "wm.val[][]" );
          error("Error allocating space to store values for SPECTUB matrix");
        }

      if ((this->wm.col[i] = new (std::nothrow) int[NITEMS[kOS][i]]) == NULL)
        {
          // error_wm_SPECT( 200, "wm.col[]" );
          error("Error allocating space to store column indices for SPECTUB matrix");
        }
    }

  //... to initialize wm to zero ......................

  for (int i = 0; i < this->wm.NbOS; i++)
    {

      this->wm.ne[i] = 0;

      for (int j = 0; j < NITEMS[kOS][i]; j++)
        {

          this->wm.val[i][j] = (float)0.;
          this->wm.col[i][j] = 0;
        }
    }
  this->wm.ne[this->wm.NbOS] = 0;

  //... wm calculation for this subset ...........................

  wm_calculation(
      kOS, ang, vox, bin, vol, prj, attmap, msk_3d, msk_2d, maxszb, &gaussdens, NITEMS[kOS], this->wm, this->wmh, Rrad);
  info(format("Weight matrix calculation done. time {} (s)", timer.value()), 2);

  //... fill lor .........................

  for (int j = 0; j < this->wm.NbOS; j++)
    {
      ProjMatrixElemsForOneBin lor;
      Bin bin;
      bin.segment_num() = 0;
      bin.view_num() = this->wm.na[j];
      bin.axial_pos_num() = this->wm.ns[j];
      bin.tangential_pos_num() = this->wm.nb[j];
      bin.set_bin_value(0);
      lor.set_bin(bin);

      lor.reserve(this->wm.ne[j]);
      for (int i = 0; i < this->wm.ne[j]; i++)
        {

          const ProjMatrixElemsForOneBin::value_type elem(Coordinate3D<int>(this->wm.nz[this->wm.col[j][i]],
                                                                            this->wm.ny[this->wm.col[j][i]],
                                                                            this->wm.nx[this->wm.col[j][i]]),
                                                          this->wm.val[j][i]);
          lor.push_back(elem);
        }

      delete[] this->wm.val[j];
      delete[] this->wm.col[j];

      this->cache_proj_matrix_elems_for_one_bin(lor);
    }

  info(format("Total time after transfering to ProjMatrixElemsForOneBin. time {} (s)", timer.value()), 2);
}
void
ProjMatrixByBinSPECTUB::calculate_proj_matrix_elems_for_one_bin(ProjMatrixElemsForOneBin& lor) const
{
  // error("ProjMatrixByBinSPECTUB element not found in cache (and hence file)");

  const int view_num = lor.get_bin().view_num();
  // find which "UB-subset" this view is in
  int kOS = 0;
  for (kOS = 0; kOS < prj.NOS; ++kOS)
    {
      // see initialisation of wmh.index
      if (prj.order[kOS] == view_num)
        break;
    }
#ifdef STIR_OPENMP
#  pragma omp critical(PROJMATRIXBYBINUBONEVIEW)
#endif
  if (!subset_already_processed[kOS])
    {
      if (!this->keep_all_views_in_cache)
        {
          this->clear_cache();
          subset_already_processed.assign(prj.NOS, false);
        }
      info(format("Computing matrix elements for view {}", view_num),
           2); // potentially pass a wm, wmh[threadh] not sure if works then in setup we need an array of wmh
      compute_one_subset(kOS, Rrad);
      subset_already_processed[kOS] = true;
    }
  lor.erase();
}

END_NAMESPACE_STIR
