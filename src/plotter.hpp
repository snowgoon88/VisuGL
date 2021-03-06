/* -*- coding: utf-8 -*- */

#ifndef PLOTTER_HPP
#define PLOTTER_HPP

/** 
 * Basic characteristics of Plotter
 * _bbox
 * PlotterLisy _plotters
 *
 * update_bbox()
 * render( ratio_x, ratio_y )
 */

#include <iostream>
#include <visugl.hpp>

// ***************************************************************************
// ******************************************************************* Plotter
// ***************************************************************************
class Plotter
{
public:
  // ********************************************************** Plotter::TYPES
  // /** Bounding box around data */
  // using BoundingBox = struct {
  //   double x_min, x_max, y_min, y_max;
  // };
  // ****************************************************** Plotter::creation
  Plotter() : _plotters(), _bbox{0.0, 1.0, 0.0, 1.0}
  {
  }
  Plotter( double x_min, double x_max, double y_min, double y_max) :
    _plotters(),
    _bbox{x_min, x_max, y_min, y_max}
  {
  }
  virtual ~Plotter()
  {
  }

  // *************************************************** Plotter::add_plotters
  /* Cannot add self */
  virtual bool add_plotter( const PlotterPtr plotter )
  {
    if (plotter != this) {
      _plotters.push_back( plotter );
      return true;
    }
    return false;
  }
  // **************************************************** Plotter::update_bbox
  virtual void update_bbox()
  {
    std::cout << "__Plotter::update_bbox: TO IMPLEMENT" << std::endl;
  }
  // ********************************************************* Plotter::render
  virtual void render( float screen_ratio_x = 1.0, float screen_ratio_y = 1.0 )
  {
    std::cout << "__Plotter::render: TO IMPLEMENT" << std::endl;
  }
  // ****************************************************** Plotter::attributs
  /** get BoundingBox */
  const BoundingBox& get_bbox() const {return _bbox;}
  void set_bbox( const BoundingBox& bbox ) { _bbox = bbox; }

  /** Other things to plot */
  PlotterList _plotters;
  // TODO: private
  BoundingBox _bbox;
  
}; // Plotter

#endif // PLOTTER_HPP
