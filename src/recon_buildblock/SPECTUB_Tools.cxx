/*
    Copyright (c) 2013, Biomedical Image Group (GIB), Universitat de Barcelona, Barcelona, Spain.
    Copyright (c) 2013, 2025, University College London
    This file is part of STIR.

    SPDX-License-Identifier: Apache-2.0

    See STIR/LICENSE.txt for details

  \author Carles Falcon
*/

// system libraries
#include <stdlib.h>
#include <math.h>
using namespace std;

//... user defined libraries .......................................

#include "stir/recon_buildblock/SPECTUB_Tools.h"
#include "stir/error.h"

namespace SPECTUB
{

#define EPSILON 1e-12

#define maxim(a, b) ((a) >= (b) ? (a) : (b))
#define minim(a, b) ((a) <= (b) ? (a) : (b))
#define abs(a) ((a) >= 0 ? (a) : (-a))
#define SIGN(a) (a < -EPSILON ? -1 : (a > EPSILON ? 1 : 0))

//=============================================================================
//=== index_calc ==============================================================
//=============================================================================

void
index_calc(int* indexs, const SPECTUB::wmh_type& wmh)
{
  if (wmh.prj.NOS == 1)
    {
      for (int i = 0; i < wmh.prj.Nang; i++)
        {
          indexs[i] = i; // when one single matrix, sequential order
        }
    }
  else
    {
      int j, *ple, *iOS, *a, *sa, *dif;

      iOS = new int[wmh.prj.NOS];
      ple = new int[wmh.prj.NOS];
      a = new int[wmh.prj.NOS];
      sa = new int[wmh.prj.NOS];
      dif = new int[wmh.prj.NOS + 1];

      //... to initialize variables ..................................

      for (int i = 0; i < wmh.prj.NOS; i++)
        {
          iOS[i] = ple[i] = a[i] = sa[i] = dif[i] = 0;
        }
      dif[wmh.prj.NOS] = 0;
      ple[0] = 1;

      //... to fill differences vector .................................

      int OS2 = wmh.prj.NOS * wmh.prj.NOS;

      for (int i = 1; i <= wmh.prj.NOS; i++)
        {
          dif[i] += 2 * (i * (i - wmh.prj.NOS)) + OS2;
        }

      //... first angle for each subset: angle having a maximum distance with all precedent angles ...

      int im = 0; // first index is always set to zero

      for (int k = 1; k < wmh.prj.NOS; k++)
        {

          for (int i = 1; i < wmh.prj.NOS; i++)
            {
              if (!ple[i])
                {
                  j = i - im;
                  a[i] = dif[abs(j)];
                }
            }

          for (int i = 0; i < wmh.prj.NOS; i++)
            {
              a[i] *= (1 - ple[i]);
              sa[i] *= (1 - ple[i]);
              sa[i] += a[i];
            }

          int m = 0;
          int n = 0;

          for (int i = 0; i < wmh.prj.NOS; i++)
            {
              m = maxim(m, sa[i]);
            }

          for (int i = 1; i < wmh.prj.NOS; i++)
            {
              if (!ple[i])
                {
                  m = minim(m, sa[i]);
                }
            }

          for (int i = 1; i < wmh.prj.NOS; i++)
            {
              if (sa[i] == m)
                {
                  n = maxim(n, a[i]);
                }
            }
          for (int i = wmh.prj.NOS - 1; i > 0; i--)
            {
              if (sa[i] == m)
                {
                  if (a[i] <= n)
                    {
                      n = a[i];
                      im = i;
                    }
                }
            }
          iOS[k] = im;
          ple[im] = 1;
        }

      //... to fill the rest of angles of each subset ................

      for (int i = 0; i < wmh.prj.NOS; i++)
        {

          for (int j = 0; j < wmh.prj.NangOS; j++)
            {

              indexs[i * wmh.prj.NangOS + j] = iOS[i] + wmh.prj.NOS * j;
            }
        }

      delete[] iOS;
      delete[] a;
      delete[] sa;
      delete[] dif;
      delete[] ple;
    }
}

//=============================================================================
//=== col params ==============================================================
//=============================================================================

// void col_params( collim_type *COL )
//{
//	cout << "Using collimator: " <<  COL->num << endl;
//
//	switch(COL->num){
//
//		case 1:  //...................fanbeam: ELSCINT
//			COL->F     = (float)35.5;
//			COL->L     = (float)4.;
//			COL->A_h   = (float)0.3369;
//			COL->A_v   = (float)0.3369;
//			COL->D     = (float)0.8;
//			COL->w     = (float)0.0866;
//			COL->insgm = (float)0.17;
//			COL->do_fb = true;
//			break;
//
//		case 2: //....................fanbeam: ELSCINT D=0
//			COL->F     = (float)35.5;
//			COL->L     = (float)4.;
//			COL->A_h   = (float)0.3369;
//			COL->A_v   = (float)0.3369;
//			COL->D     = (float)0.;
//			COL->w     = (float)0.0866;
//			COL->insgm = (float)0.17;
//			COL->do_fb = true;
//			break;
//
//		case 3: //....................parallel 3: low resolution
//			COL->A     = (float)0.0275;
//			COL->B     = (float)0.2;
//			COL->do_fb = false;
//			break;
//
//		case 4: //....................parallel 4: high resolution
//			COL->A     = (float)0.0172;
//			COL->B     = (float)0.2;
//			COL->do_fb = false;
//			break;
//
//		case 5: //....................parallel 5: (ECAM)
//			COL->A     = (float)0.0167;
//			COL->B     = (float)0.1405;
//			COL->do_fb = false;
//			break;
//
//		case 6: //....................fan_beam: prism3000
//			COL->F     = (float)65.0;
//			COL->L     = (float)2.7;
//			COL->A_h   = (float)0.3575;
//			COL->A_v   = (float)0.3360;
//			COL->D     = (float)0.0;
//			COL->w     = (float)0.0866;
//			COL->insgm = (float)0.17;
//			COL->do_fb = true;
//			break;
//
//		case 10: //...................parallel: ECAM with L=40 mm
//			COL->A     = (float)0.0101;
//			COL->B     = (float)0.0998;
//			COL->do_fb = false;
//			break;
//
//		case 11: //...................fan beam ELSCINT L=2,405 cm
//			COL->F     = (float)35.5;
//			COL->L     = (float)2.405;
//			COL->A_h   = (float)0.3369;
//			COL->A_v   = (float)0.3369;
//			COL->D     = (float)0.8;
//			COL->w     = (float)0.0866;
//			COL->insgm = (float)0.17;
//			COL->do_fb = true;
//			break;
//
//		case 13: //...................parallel: Hammamatsu collimator
//			COL->A     = (float)0.0205;
//			COL->B     = (float)0.10245;
//			COL->do_fb = false;
//			break;
//
//		case 14: //...................parallel. hexagonal holes. apotema=0.57mm, L=24mm, s=0.125mm
//			COL->A     = (float)0.0178;
//			COL->B     = (float)0.0886;
//			COL->do_fb = false;
//			break;
//
//		case 15: //...................parallel. hexagonal holes apotema=0.57mm, L=24mm, s=0.125mm
//			COL->A     = (float)0.0247;
//			COL->B     = (float)0.0752;
//			COL->do_fb = false;
//			break;
//
//		case 16: //...................parallel: Sentinella S102 colimador: experimental parameters
//			COL->A     = (float)0.0166;
//			COL->B     = (float)0.0924;
//			COL->do_fb = false;
//			break;
//
//		case 17: //...................parallel Infinia Hawkeye: experimental parametres
//			COL->A     = (float)0.0163;
//			COL->B     = (float)0.1466;
//			COL->do_fb = false;
//			break;
//
//		default:
//			char p[3];	// auxiliar variable for itoa
//			error_wmtools_SPECT( 21, itoa(COL->num,p));
//	}
// }

//==========================================================================
//=== calc_sigma_v =========================================================
//==========================================================================

float
calc_sigma_v(voxel_type vox, collim_type COL)
{
  float sigma;
  if (COL.do_fb)
    {
      float xc = (float)2. * COL.A_v * COL.w * (vox.dv2dp + COL.L + COL.D) / COL.L;
      sigma = sqrt(COL.insgm * COL.insgm + xc * xc);
    }
  else
    sigma = COL.A * vox.dv2dp + COL.B;

  return (sigma);
}

//=============================================================================
//=== fill_ang ================================================================
//=============================================================================

void
fill_ang(angle_type* ang, const SPECTUB::wmh_type& wmh, const float* Rrad)
{
  const float DX = (float)0.5 / wmh.psfres;
  const float dg2rd = _PI /*boost::math::constants::pi<float>()*/ / (float)180.;

  for (int i = 0; i < wmh.prj.Nang; i++)
    {

      //... ratios calculation .......................................................

      const float deg = wmh.prj.ang0 + (float)i * wmh.prj.incr; // angle in degrees
      ang[i].cos = cos(deg * dg2rd);                            // cosinus of the angle
      ang[i].sin = sin(deg * dg2rd);                            // sinus of the angle

      //... first octave (0->45degrees) equivalent angle and its trigonometric ratios .......

      float angR = fabs(deg);
      int quad = (int)floor(angR / (float)90.); // quadrant

      angR = fabs(angR - (float)90. * (float)quad); // reduced angle: equivalent angle in 0->45degrees interval
      if (angR > (float)45.)
        angR = fabs((float)90. - angR);

      float sinR = (float)sin(angR * dg2rd); // sinus of the reduced angle
      float cosR = (float)cos(angR * dg2rd); // cosinus of the reduced angle

      //... parametres of the oblique projection of a square voxel size 1 (half a trapezoid) .......

      if (!wmh.do_psf)
        {

          if (angR < EPSILON)
            {

              ang[i].p = (float)1.;
              ang[i].N1 = ang[i].N2 = (int)floor(DX);
              ang[i].m = ang[i].n = (float)0.;
            }
          else
            {
              ang[i].p = (float)1. / cosR;                           // plateau highness
              ang[i].m = -wmh.psfres / (sinR * cosR);                // slope of the trapezoid in DX units (negative)
              ang[i].n = (cosR + sinR) * (float)0.5 / (cosR * sinR); // independent term of the slope of the trapezoid (cm)
              ang[i].N1 = (int)floor((float)fabs(cosR - sinR) * DX); // index of the first vertice (end of plateau) in res units
              ang[i].N2 = (int)floor((cosR + sinR) * DX); // index of the second vertice (end of the slope) in res units
            }

          ang[i].vxprj.lngd2 = ang[i].N2;
          ang[i].vxprj.lng = 2 * ang[i].N2;
          ang[i].vxprj.res = wmh.psfres;
        }
      //... rotation radius ................................................................

      ang[i].Rrad = Rrad[i]; // assignation of (variable) rotation radius

      //... coordinates of the first bin of each projection and increments for consecutive bins ....

      if (wmh.do_att)
        {

          ang[i].incx = wmh.prj.szcm * ang[i].cos;
          ang[i].incy = wmh.prj.szcm * ang[i].sin;

          ang[i].xbin0 = -ang[i].Rrad * ang[i].sin - wmh.prj.lngcmd2 * ang[i].cos;
          ang[i].ybin0 = ang[i].Rrad * ang[i].cos - wmh.prj.lngcmd2 * ang[i].sin;
        }
    }
}

//=============================================================================
//=== generate msk ============================================================
//=============================================================================

void
generate_msk(bool* msk_3d, bool* msk_2d, const float* attmap, const volume_type* vol, const SPECTUB::wmh_type& wmh)
{
  //... initialzation of msk to true .........................

  for (int i = 0; i < vol->Nvox; i++)
    {
      msk_3d[i] = true;
    }

  //... initialzation of msk_2d to false .....................

  for (int i = 0; i < vol->Npix; i++)
    {
      msk_2d[i] = false;
    }

  //... to create mask from attenuation map ..................

  if (wmh.do_msk_att)
    {
      for (int i = 0; i < wmh.vol.Nvox; i++)
        {
          msk_3d[i] = (attmap[i] > EPSILON);
        }
    }
  else
    {
      //... to create a cylindrical mask......................

      if (wmh.do_msk_cyl)
        {

          float Rmax2, xi, yi;

          if (vol->Nrow >= vol->Ncol)
            Rmax2 = vol->Nrowd2 * vol->Nrowd2; // Maximum allowed radius (distance from volume centre)
          else
            Rmax2 = vol->Ncold2 * vol->Ncold2;

          int ip = -1; // in-plane index of the voxel

          for (int i = 0; i < vol->Ncol; i++)
            {

              xi = i - vol->Ncold2 + (float)0.5;
              xi *= xi;

              for (int j = 0; j < vol->Nrow; j++)
                {

                  ip++;
                  yi = j - vol->Nrowd2 + (float)0.5;
                  yi *= yi;

                  if ((xi + yi) > Rmax2)
                    {

                      for (int k = 0; k < vol->Nsli; k++)
                        {

                          msk_3d[ip + k * vol->Npix] = false; // loop for all the slices
                        }
                    }
                }
            }
        }

      else
        {
          //... to read a mask from a (int) file ....................

          if (wmh.do_msk_file)
            stir::error("SPECTUB read_msk_file is not supported in STIR");
        }
    }

  //... to apply slice mask (to remove slices from matrix) ..............

  if (wmh.do_msk_slc)
    {
      for (int i = 0; i < wmh.vol.first_sl; i++)
        {
          for (int j = 0; j < wmh.vol.Npix; j++)
            {
              msk_3d[i * wmh.vol.Npix + j] = false;
            }
        }
      for (int i = wmh.vol.last_sl; i < wmh.vol.Nsli; i++)
        {
          for (int j = 0; j < wmh.vol.Npix; j++)
            {
              msk_3d[i * wmh.vol.Npix + j] = false;
            }
        }
    }

  //... to collapse mask to 2d_mask .........

  if (wmh.do_msk_cyl)
    {
      for (int i = 0; i < wmh.vol.Npix; i++)
        {
          msk_2d[i] = msk_3d[i + wmh.vol.first_sl * wmh.vol.Npix];
        }
    }
  else
    {
      for (int i = 0; i < wmh.vol.Npix; i++)
        {

          for (int k = wmh.vol.first_sl; k < wmh.vol.last_sl; k++)
            {

              if (msk_3d[k * wmh.vol.Npix + i])
                {
                  msk_2d[i] = true;
                  break;
                }
            }
        }
    }
}

//==========================================================================
//=== max_psf_szb ==========================================================
//==========================================================================

int
max_psf_szb(const angle_type* ang, const SPECTUB::wmh_type& wmh)
{
  int maxszb;
  float Rrad_max = ang[0].Rrad;

  for (int i = 1; i < wmh.prj.Nang; i++)
    {
      if (ang[i].Rrad > Rrad_max)
        Rrad_max = ang[i].Rrad; // maximum rotation radius
    }

  if (!wmh.do_psf)
    { // NO-PSF

      if (!wmh.COL.do_fb)
        { // parallel
          maxszb = (int)((float)sqrt((float)2.) * wmh.vol.szcm / wmh.prj.szcm) + 3;
        }

      else
        { // fanbeam
          float dpmax = wmh.vol.szcm * maxim(wmh.vol.Ncold2, wmh.vol.Nrowd2) + Rrad_max;

          float lon = wmh.COL.F - dpmax;
          if (lon < EPSILON)
            {
              // error_wmtools_SPECT(46, "");
              stir::error("Error SPECTUB weight3d: there are voxels near or further than the FOCAL length");
            }

          //... maximum lenght of psf in bins ........................

          float f = (int)((float)sqrt((float)2.) * (wmh.vol.szcm / wmh.prj.szcm) * (wmh.COL.F / lon)) + 3;
          maxszb = minim(f, wmh.prj.Nbin);
        }
    }
  else
    { // PSF
      voxel_type vox;

      if (wmh.COL.do_fb)
        {
          vox.costhe = (float)1. / sqrt(wmh.prj.lngcmd2 * wmh.prj.lngcmd2 / (wmh.COL.F * wmh.COL.F) + (float)1.);
        }
      //... maximum length of psf in bins ........................

      vox.dv2dp = Rrad_max + wmh.vol.szcm * maxim(wmh.vol.Ncold2, wmh.vol.Nrowd2) * (float)1.5;
      float sig_h_max_cm = calc_sigma_h(vox, wmh.COL);
      maxszb = (int)floor(wmh.maxsigm * (float)2. * sig_h_max_cm / wmh.prj.szcm) + 3;

      if (wmh.do_psf_3d)
        {
          float sig_v_max_cm = calc_sigma_v(vox, wmh.COL);
          int maxszb_v = (int)floor(wmh.maxsigm * (float)2. * sig_v_max_cm / wmh.prj.thcm) + 3;
          maxszb = maxim(maxszb, maxszb_v);
        }
    }

  return (maxszb);
}

//==========================================================================
//=== calc_sigma_h =========================================================
//==========================================================================

float
calc_sigma_h(voxel_type vox, collim_type COL)
{
  float sigma;

  if (COL.do_fb)
    {
      float denom = sqrt(COL.L * COL.L * (COL.F - vox.dv2dp) * (COL.F - vox.dv2dp)
                         - COL.w * COL.w * (COL.L + (float)2. * vox.dv2dp) * (COL.L + (float)2. * vox.dv2dp));
      float xc = COL.A_h * (vox.dv2dp + COL.L + COL.D) * COL.w * ((float)2. * COL.F + COL.L) / (vox.costhe * denom);
      sigma = sqrt(COL.insgm * COL.insgm + xc * xc);
    }
  else
    sigma = COL.A * vox.dv2dp + COL.B;

  return (sigma);
}

//=============================================================================
//=== free_wm =================================================================
//=============================================================================

void
free_wm(wm_type* f)
{
  delete[] f->ar;
  delete[] f->ja;
  delete[] f->ia;
}

//=============================================================================
//=== free_wm_da ==============================================================
//=============================================================================

void
free_wm_da(wm_da_type* f)
{
  for (int i = 0; i < f->NbOS; i++)
    {
      delete[] f->val[i];
      delete[] f->col[i];
    }
  delete[] f->val;
  delete[] f->col;
  delete[] f->ne;

  if (f->do_save_STIR)
    {
      delete[] f->nb;
      delete[] f->ns;
      delete[] f->na;
      delete[] f->nx;
      delete[] f->ny;
      delete[] f->nz;
    }
}

} // namespace SPECTUB
