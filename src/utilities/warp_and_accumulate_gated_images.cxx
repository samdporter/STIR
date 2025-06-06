//
/*
 Copyright (C) 2009 - 2013, King's College London
 This file is part of STIR.

 SPDX-License-Identifier: Apache-2.0

 See STIR/LICENSE.txt for details
 */
/*!
 \file
 \ingroup utilities
 \ingroup spatial_transformation

 \brief This program corrects the motion from an image.
 \author Charalampos Tsoumpas
 */
#include "stir/IO/OutputFileFormat.h"
#include "stir/DiscretisedDensity.h"
#include "stir/GatedDiscretisedDensity.h"
#include "stir/spatial_transformation/GatedSpatialTransformation.h"
#include "stir/Succeeded.h"
#include <fstream>
#include <stdio.h>
#include <stdlib.h>

using std::cerr;

USING_NAMESPACE_STIR

using namespace BSpline;

int
main(int argc, char** argv)
{
  if (argc < 3 || argc > 4)
    {
      cerr << "Usage: " << argv[0] << " <output filename> <filename prefix> <motion vectors prefix> \n";
      exit(EXIT_FAILURE);
    }
  //	GatedDiscretisedDensity tmp;
  const GatedDiscretisedDensity gated_density(argv[2]);
  GatedSpatialTransformation transformation;
  if (argc == 3)
    transformation.read_from_files(argv[2]);
  else if (argc == 4)
    transformation.read_from_files(argv[3]);
  shared_ptr<DiscretisedDensity<3, float>> corrected_image_sptr((gated_density[1]).get_empty_copy());
  transformation.warp_image(*corrected_image_sptr, gated_density);

  const Succeeded res
      = OutputFileFormat<DiscretisedDensity<3, float>>::default_sptr()->write_to_file(argv[1], *corrected_image_sptr);
  return res == Succeeded::yes ? EXIT_SUCCESS : EXIT_FAILURE;
}
