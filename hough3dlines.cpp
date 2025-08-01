//
// hough3dlines.cpp
//     Main program implementing the iterative Hough transform
//
// Author:  Tilman Schramke, Christoph Dalitz
// Date:    2020-01-15
// License: see License-BSD2
//

#include "vector3d.h"
#include "pointcloud.h"
#include "hough.h"
#include "hough3dlines.h"

#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <Eigen/Dense>
// #include <Eigen/Geometry>

using Eigen::MatrixXf;
using namespace std;

namespace hough3d
{
// usage message
const char* usage = "Usage:\n"
  "\though3dlines [options] <infile>\n"
  "Options (defaults in brackets):\n"
  "\t-o <outfile>   write results to <outfile> [stdout]\n"
  "\t-dx <dx>       step width in x'y'-plane [0]\n"
  "\t               when <dx>=0, it is set to 1/64 of total width\n"
  "\t-nlines <nl>   maximum number of lines returned [0]\n"
  "\t               when <nl>=0, all lines are returned\n"
  "\t-minvotes <nv> only lines with at least <nv> points are returned [0]\n"
  "\t-gnuplot       print result as a gnuplot command\n"
  "\t-raw           print plot data in easily machine-parsable format\n"
  "\t-delim <char>  use <char> as field delimiter in input file [,]\n"
  "\t-v             be verbose and print Hough space size to stdout\n"
  "\t-vv            be even more verbose and print Hough lines (before LSQ)\n"
  "Version:\n"
  "\t1.2 from 2020-01-15\n";

//--------------------------------------------------------------------
// utility functions
//--------------------------------------------------------------------

//
// orthogonal least squares fit with libeigen
// rc = largest eigenvalue
//
double orthogonal_LSQ(const PointCloud &pc, Vector3d* a, Vector3d* b){
  double rc = 0.0;

  // anchor point is mean value
  *a = pc.meanValue();

  // copy points to libeigen matrix
  Eigen::MatrixXf points = Eigen::MatrixXf::Constant(pc.points.size(), 3, 0);
  for (int i = 0; i < points.rows(); i++) {
    points(i,0) = pc.points.at(i).x;
    points(i,1) = pc.points.at(i).y;
    points(i,2) = pc.points.at(i).z;
  }

  // compute scatter matrix ...
  MatrixXf centered = points.rowwise() - points.colwise().mean();
  MatrixXf scatter = (centered.adjoint() * centered);

  // ... and its eigenvalues and eigenvectors
  Eigen::SelfAdjointEigenSolver<Eigen::MatrixXf> eig(scatter);
  Eigen::MatrixXf eigvecs = eig.eigenvectors();

  // we need eigenvector to largest eigenvalue
  // libeigen yields it as LAST column
  b->x = eigvecs(0,2); b->y = eigvecs(1,2); b->z = eigvecs(2,2);
  rc = eig.eigenvalues()(2);

  return (rc);
}


//--------------------------------------------------------------------
// main program
//--------------------------------------------------------------------

int main(int argc, char ** argv) {

  // default values for command line options
  double opt_dx = 0.0;
  int opt_nlines = 0;
  int opt_minvotes = 0;
  enum Outformat { format_normal, format_gnuplot, format_raw };
  Outformat opt_outformat = format_normal;
  char opt_delim = ',';
  int opt_verbose = 0;
  char* infile_name = NULL;
  char* outfile_name = NULL;

  // number of icosahedron subdivisions for direction discretization
  int granularity = 4;
  // int num_directions[7] = {12, 21, 81, 321, 1281, 5121, 20481};

  // IO files
  //FILE* infile = NULL;
  FILE* outfile = stdout;

  // // bounding box of point cloud
  // Vector3d minP, maxP, minPshifted, maxPshifted;
  // // diagonal length of point cloud
  // double d;

  // parse command line
  for (int i=1; i<argc; i++) {
    if (0 == strcmp(argv[i], "-o")) {
      i++;
      if (i<argc) outfile_name = argv[i];
    }
    else if (0 == strcmp(argv[i], "-dx")) {
      i++;
      if (i<argc) opt_dx = atof(argv[i]);
    }
    else if (0 == strcmp(argv[i], "-nlines")) {
      i++;
      if (i<argc) opt_nlines = atoi(argv[i]);
    }
    else if (0 == strcmp(argv[i], "-minvotes")) {
      i++;
      if (i<argc) opt_minvotes = atoi(argv[i]);
    }
    else if (0 == strcmp(argv[i], "-gnuplot")) {
      opt_outformat = format_gnuplot;
    }
    else if (0 == strcmp(argv[i], "-raw")) {
      opt_outformat = format_raw;
    }
    else if (0 == strcmp(argv[i], "-delim")) {
      i++;
      if (i<argc) opt_delim = argv[i][0];
    }
    else if (0 == strcmp(argv[i], "-v")) {
      opt_verbose = 1;
    }
    else if (0 == strcmp(argv[i], "-vv")) {
      opt_verbose = 2;
    }

    else if (argv[i][0] == '-') {
      fprintf(stderr, "%s", usage);
      return 1;
    }
    else {
      infile_name = argv[i];
    }
  }

  // plausibilty checks
  if (!infile_name) {
    fprintf(stderr, "Error: no infile given!\n%s", usage);
    return 1;
  }
  if (opt_dx < 0){
    fprintf(stderr, "Error: dx < 0!\n%s", usage);
    return 1;
  }
  if (opt_nlines < 0){
    fprintf(stderr, "Error: nlines < 0!\n%s", usage);
    return 1;
  }
  if (opt_minvotes < 0){
    fprintf(stderr, "Error: minvotes < 0!\n%s", usage);
    return 1;
  }
  if (opt_minvotes < 2){
    opt_minvotes = 2;
  }

  // open in/out files
  if (outfile_name) {
    outfile = fopen(outfile_name, "w");
    if (!outfile) {
      fprintf(stderr, "Error: cannot open outfile '%s'!\n", outfile_name);
      return 1;
    }
  }

  // read point cloud from file
  PointCloud X;
  int errorcode = X.readFromFile(infile_name, opt_delim);
  if (2 == errorcode) {
    fprintf(stderr, "Error: wrong file format of infile '%s'!\n", infile_name);
    return 1;
  }
  else if (0 != errorcode) {
    fprintf(stderr, "Error: cannot read infile '%s'!\n", infile_name);
    return 1;
  }
  if (X.points.size() < 2) {
    fprintf(stderr, "Error: point cloud has less than two points\n");
    return 1;
  }

  return 0;
}


std::pair<std::vector<std::pair<std::array<Vector3d, 2>, PointCloud>>, Vector3d> hough3dlines(PointCloud X, double opt_dx, int granularity, int opt_nlines, int opt_minvotes, int opt_verbose){

  int num_directions[7] = {12, 21, 81, 321, 1281, 5121, 20481};
    // bounding box of point cloud
  Vector3d minP, maxP, minPshifted, maxPshifted;
  std::vector<PointCloud> stored_pcd_points;
  // std::vector<array<Vector3d, 2>> stored_lines;
  // diagonal length of point cloud

  std::vector<std::pair<std::array<Vector3d, 2>, PointCloud>> segmented_pcd;
  double d;


  // center cloud and compute new bounding box
  X.getMinMax3D(&minP, &maxP);
  d = (maxP-minP).norm();
  if (d == 0.0) {
    fprintf(stderr, "Error: all points in point cloud identical\n");
    // return 1; //TODO fix this
  }
  X.shiftToOrigin();
  X.getMinMax3D(&minPshifted, &maxPshifted);

  // estimate size of Hough space
  if (opt_dx == 0.0) {
    opt_dx = d / 64.0;
  }
  else if (opt_dx >= d) {
    fprintf(stderr, "Error: dx too large\n");
    // return 1; //TODO fix this 
  }
  double num_x = floor(d / opt_dx + 0.5);
  // double num_x = 128; //TODO add lattice as a parameter
  double num_cells = num_x * num_x * num_directions[granularity];
  if (opt_verbose) {
    printf("info: x'y' value range is %f in %.0f steps of width dx=%f\n",
           d, num_x, opt_dx);
    printf("info: Hough space has %.0f cells taking %.2f MB memory space\n",
           num_cells, num_cells * sizeof(unsigned int) / 1000000.0);
  }
#ifdef WEBDEMO
  if (num_cells > 1E8) {
    fprintf(stderr, "Error: program was compiled in WEBDEMO mode, "
            "which does not permit %.0f cells in Hough space\n", num_cells);
    return 2;
  }
#endif

  // first Hough transform
  Hough* hough;
  try {
    hough = new Hough(minPshifted, maxPshifted, opt_dx, granularity);
  } catch (const std::exception &e) {
    fprintf(stderr, "Error: cannot allocate memory for %.0f Hough cells"
            " (%.2f MB)\n", num_cells, 
            (double(num_cells) / 1000000.0) * sizeof(unsigned int));
    // return 2; //TODO fix this
  }
  hough->add(X);

  
  // iterative Hough transform
  // (Algorithm 1 in IPOL paper)
  PointCloud Y;	// points close to line
  double rc;
  unsigned int nvotes;
  int nlines = 0;
  do {
    // std::pair<std::array<Vector3d, 2>, PointCloud> pcd_line_pair;
    Vector3d a; // anchor point of line
    Vector3d b; // direction of line

    hough->subtract(Y); // do it here to save one call

    nvotes = hough->getLine(&a, &b);
    if (opt_verbose > 1) {
      Vector3d p = a + X.shift;
      printf("info: highest number of Hough votes is %i for the following "
             "line:\ninfo: a=(%f,%f,%f), b=(%f,%f,%f)\n",
             nvotes, p.x, p.y, p.z, b.x, b.y, b.z);
    }

    X.pointsCloseToLine(a, b, opt_dx, &Y);

    rc = orthogonal_LSQ(Y, &a, &b);
    if (rc==0.0) break;

    X.pointsCloseToLine(a, b, opt_dx, &Y);
    nvotes = Y.points.size();
    if (nvotes < (unsigned int)opt_minvotes) break;

    rc = orthogonal_LSQ(Y, &a, &b);
    if (rc==0.0) break;

    a = a + X.shift;
    

    nlines++;

    

    // Y.shiftPCD(X.shift);
    std::array<Vector3d, 2> temp_line = {a, b};
    segmented_pcd.push_back(std::make_pair(temp_line, Y));
    std::cout<< X.shift <<std::endl;
    X.removePoints(Y);
    // PointCloud copy_pcd = Y;

    // copy_pcd.shiftPCD(X.shift);


  } while ((X.points.size() > 1) && 
           ((opt_nlines == 0) || (opt_nlines > nlines)));

  // X.shiftPCD(X.shift);
  return std::make_pair(segmented_pcd, X.shift);
};
}