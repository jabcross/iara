//
// Created by orenaud on 4/14/25.
//
#include "casacore_wrapper.h"
#include "common.h"

#include <casacore/casa/Arrays/IPosition.h>
#include <casacore/casa/Arrays/Vector.h>
#include <casacore/casa/OS/File.h>
#include <casacore/ms/MeasurementSets/MSColumns.h>
#include <casacore/ms/MeasurementSets/MeasurementSet.h>
#include <casacore/tables/Tables/ArrayColumn.h>
#include <casacore/tables/Tables/Table.h>
#include <cmath>
#include <fftw3.h>
#include <iostream>

#define SPEED_OF_LIGHT 299792458.0

#ifndef ABS
  #define ABS(x) ((x) < 0 ? -(x) : (x))
#endif

extern "C" void load_visibilities_from_ms(const char *ms_path_c,
                                          int num_vis,
                                          Config *config,
                                          float3 *uvw_coords) {
  std::string ms_path(ms_path_c);
  // std::cout << "UPDATE >>> Loading visibilities from MS file " << ms_path <<
  // "...\n";

  if (!casacore::File(ms_path).exists()) {
    std::cerr << "ERROR >>> MeasurementSet " << ms_path << " does not exist!\n";
    std::exit(EXIT_FAILURE);
  }

  casacore::MeasurementSet ms(ms_path);
  casacore::MSColumns msCols(ms);

  int total_rows = ms.nrow();
  if (num_vis > total_rows) {
    std::cerr << "WARNING >>> Requested " << num_vis
              << " visibilities, but only " << total_rows
              << " available. Truncating to " << total_rows << ".\n";
    num_vis = total_rows;
  }

  casacore::ArrayColumn<casacore::Double> uvwCol(ms, "UVW");
  size_t nrows = ms.nrow();

  // std::cout << "Number of visibilities : " << nrows << std::endl;
  //  std::cout << "🌌 CUVW coordinates of the first 5 rows:" << std::endl;

  /*
  for (size_t i = 0; i < std::min(nrows, size_t(5)); ++i) {
      casacore::Vector<casacore::Double> uvw;
      uvwCol.get(i, uvw);
      std::cout << "Visibility " << i << " : "
                << "u = " << uvw[0] << " m, "
                << "v = " << uvw[1] << " m, "
                << "w = " << uvw[2] << " m" << std::endl;
  }
  */

  auto uvw_col = msCols.uvw().getColumn();
  auto data_col = msCols.data().getColumn();
  auto weight_col = msCols.weight().getColumn();

  auto uvw_vec = uvw_col.tovector();
  auto data_vec = data_col.tovector();
  auto weight_vec = weight_col.tovector();

  auto shape = data_col.shape();

  /*
   std::cout << "Shape of DATA cell: [";
  for (size_t i = 0; i < shape.size(); ++i)
      std::cout << shape[i] << (i < shape.size() - 1 ? ", " : "");
  std::cout << "]\n";
   */

  int nchan = shape[0];
  int npol = shape[1];

  for (int i = 0; i < num_vis; ++i) {
    casacore::Vector<casacore::Double> uvw;
    uvwCol.get(i, uvw);
    double u = uvw[0];
    double v = uvw[1];
    double w = uvw[2];

    if (config->right_ascension) {
      u *= -1.0;
      w *= -1.0;
    }

    uvw_coords[i].x = u;
    uvw_coords[i].y = v;
    uvw_coords[i].z = w;

    auto cpx = data_vec[i * nchan * npol]; // premier canal/pola
    float weight = config->force_weight_to_one ? 1.0f : weight_vec[i];

    //        measured_vis[i].real = cpx.real() * weight;
    //        measured_vis[i].imaginary = cpx.imag() * weight;
  }
  // std::cout << "UPDATE >>> Loaded " << num_vis << " visibilities from
  // MeasurementSet.\n"; std::cout << "nchan:" << nchan << " npol:" << npol << "
  // num_vis: " << num_vis << " -> number of visibilities samples: " <<
  // nchan*npol*num_vis << "\n";
}
