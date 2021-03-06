/* -*- coding: utf-8 -*- */

/**
 * test-011-layer.cpp
 * 
 * Verifie serialize/unserialize de Layer.
 *
 * Crée un Reservoir+Layer et fait forward
 */

#include <iostream>                   // std::cout
#include <fstream>                    // std::ofstream
#include <rapidjson/document.h>       // rapidjson

#include <gsl/gsl_matrix.h>         // gsl Matrices

#include <reservoir.hpp>
#include <layer.hpp>

#include <utils.hpp>                  // various str_xxx
using namespace utils::rj;
// ******************************************************************** Global
#define LAY_FILE "layer.data"
//******************************************************************************
int main( int argc, char *argv[] )
{
  Reservoir res( 2, 3, 0.2);
  res.set_spectral_radius( 0.1 );
  std::cout << "***** RESERVOIR **" << "\n";
  std::cout << res.str_dump() << std::endl;

  Layer lay( 3, 1 );
  std::cout << "***** LAYER **" << std::endl;
  std::cout << lay.str_dump() << std::endl;
  std::cout << "----- JSON --" << std::endl;
  rapidjson::Document doc;
  rapidjson::Value obj = lay.serialize( doc );
  std::cout << str_obj( obj ) << std::endl;

  // Write in a file
  std::ofstream ofile(LAY_FILE);
  ofile << str_obj( obj ) << std::endl;
  ofile.close();

  // Read from file
  std::ifstream ifile(LAY_FILE);
  Layer lay_read( ifile );
  std::cout << "***** LAY_READ ***" << "\n";
  std::cout << lay_read.str_dump() << std::endl;
  std::cout << "***** LAY_end ****" << "\n";

  
  // try Input
  Reservoir::Tinput in = {1.2, -2.0};
  auto out_res = res.forward( in );
  std::cout << "out_res = ";
  for( auto& v: out_res) {
    std::cout << v << "; ";
  }
  std::cout << std::endl;
  
  auto out = lay.forward( out_res );
  std::cout << "out = ";
  for( auto& v: out) {
    std::cout << v << "; ";
  }
  std::cout << std::endl;

  // Copy test
  std::cout << "** COPY" << std::endl;
  Layer l1(lay);
  auto weights = lay.weights();
  gsl_matrix_set(weights, 0, 0, 2.0);
  std::cout << "== ORIGIN " << lay.str_dump() << std::endl;
  std::cout << "== COPY   " << l1.str_dump() << std::endl;

  Layer l2 = lay;
  weights = l2.weights();
  gsl_matrix_set(weights, 0, 0, 3.0);
  std::cout << "== ORIGIN " << lay.str_dump() << std::endl;
  std::cout << "== COPY   " << l2.str_dump() << std::endl;


  return 0;
}
