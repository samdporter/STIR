//
//
/*!

  \file
  \ingroup ImageProcessor
  \brief Declaration of class stir::WienerImageFilter2D.h

  \author Dimitra Kyriakopoulou
  \author Kris Thielemans

*/
/*
    Copyright (C) 2024, University College London
    This file is part of STIR.

    SPDX-License-Identifier: Apache-2.0

    See STIR/LICENSE.txt for details
*/

#ifndef __stir_WienerImageFilter2D_H__
#define __stir_WienerImageFilter2D_H__

#include "stir/DataProcessor.h"
#include "stir/WienerArrayFilter2D.h"
#include "stir/DiscretisedDensity.h"
#include "stir/RegisteredParsingObject.h"

START_NAMESPACE_STIR

/*!
  \ingroup ImageProcessor
  \brief A class in the ImageProcessor hierarchy that implements wiener
  filtering.

  As it is derived from RegisteredParsingObject, it implements all the
  necessary things to parse parameter files etc.
 */
template <typename elemT>
class WienerImageFilter2D : public RegisteredParsingObject<WienerImageFilter2D<elemT>,
                                                           DataProcessor<DiscretisedDensity<3, elemT>>,
                                                           DataProcessor<DiscretisedDensity<3, elemT>>>
{
private:
  typedef RegisteredParsingObject<WienerImageFilter2D<elemT>,
                                  DataProcessor<DiscretisedDensity<3, elemT>>,
                                  DataProcessor<DiscretisedDensity<3, elemT>>>
      base_type;

public:
  static const char* const registered_name;

  WienerImageFilter2D();

private:
  WienerArrayFilter2D<elemT> wiener_filter;

  void set_defaults() override;
  void initialise_keymap() override;

  Succeeded virtual_set_up(const DiscretisedDensity<3, elemT>& density) override;
  void virtual_apply(DiscretisedDensity<3, elemT>& density, const DiscretisedDensity<3, elemT>& in_density) const override;
  void virtual_apply(DiscretisedDensity<3, elemT>& density) const override;
};

END_NAMESPACE_STIR

#endif
