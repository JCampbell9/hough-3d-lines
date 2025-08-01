#ifndef HOUGH3DLINES_H_
#define HOUGH3DLINES_H_

#include <vector>
#include <array>
#include "vector3d.h"
#include "pointcloud.h"

namespace hough3d
{
    
std::pair<std::vector<std::pair<std::array<Vector3d, 2>, PointCloud>>, Vector3d> hough3dlines(PointCloud X, double opt_dx=0.0, int granularity = 4, int opt_nlines=0, int opt_minvotes=0, int opt_verbose=0);

}

#endif