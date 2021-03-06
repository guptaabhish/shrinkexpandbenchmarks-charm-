#include <limits>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <cmath>
#include <cstring>
#include <queue>
#include <set>
#include <vector>
#include <algorithm>
#include <limits>
#include "charm++.h"
#include "trace-projections.h"
#include "PowerLog.decl.h"
//#include <boost/assign/list_of.hpp>
//#include "boost/filesystem.hpp"

using namespace std;

#include "charm++.h"
#include "liveViz.h"
#include "Constants.h"
#include "QuadIndex.h"
#include "Advection.h"
#include "Main.decl.h"
#include "Main.h"

CProxy_Advection qtree;
CProxy_Main mainProxy;

int array_height;
int array_width;

int num_chare_rows;
int num_chare_cols;

int block_height;
int block_width;

int min_depth, max_depth;
int max_iterations, refine_frequency;


#define wrap_x(a) (((a)+num_chare_rows)%num_chare_rows)
#define wrap_y(a) (((a)+num_chare_cols)%num_chare_cols)

#ifndef AMR_REVISION
#define AMR_REVISION Unknown
#endif
#define _QUOTEIT(x) #x
#define INQUOTES(x) _QUOTEIT(x)

const char amrRevision[] = INQUOTES(AMR_REVISION);

int nframe;
double xmin, xmax, ymin, ymax;
double xctr, yctr, radius;
double dx, dy, v;
double ap, an;
double tmax, t, dt, cfl;

char* decimal_to_binary_string(int num, int len){
  char* _ret = new char[len+1];
  int i=0;
  _ret[len]=0;
  while(num){
    _ret[len-i-1] = (num&1==1)?'1':'0';
    num = num>>1;
    i++;
  };
  for(; i<len; i++)
    _ret[len-i-1]='0';
  return _ret;
}

double start_time, end_time;

Main::Main(CkArgMsg* m){
  ckout<<"Running amr code revision: "<<amrRevision<<endl;

  mainProxy = thisProxy;
  //boost::filesystem::remove_all("out"); boost::filesystem::remove_all("log");
  //boost::filesystem::create_directory("out"); boost::filesystem::create_directory("log");
  //boost::filesystem::remove_all("/intrepid-fs0/users/alanger/scratch/amr/out"); boost::filesystem::remove_all("/intrepid-fs0/users/alanger/scratch/amr/log");
  //boost::filesystem::create_directory("/intrepid-fs0/users/alanger/scratch/amr/out"); boost::filesystem::create_directory("/intrepid-fs0/users/alanger/scratch/amr/log");

  iterations = 0;

  if(m->argc < 4){
    ckout << "Usage: " << m->argv[0] << "[max_depth] [block_size] [iterations] [array_dim]?" << endl; 
    CkExit();
  }

  if (m->argc >= 5) {
    array_height = array_width = atoi(m->argv[4]);
  } else {
    array_height = array_width = 256;
  }
  
  block_height = block_width = atoi(m->argv[2]);
  max_iterations = atoi(m->argv[3]);

  if(array_width%block_width < 0 || array_width < block_width){
    ckout << "Incompatible arguments" << endl;
    CkExit();
  }

  num_chare_rows = num_chare_cols = array_height/block_height;
    
  /******** Do Some Initialization *********/
  xmin = 0;
  xmax = 1;
  ymin = 0;
  ymax = 1;
  t= 0;
  tmax = 10000;
  cfl = 0.4;
  v = 0.1;
  io_outnum = 0;
  io_tnext = 0;
  nframe = 20;
  io_tout = tmax/nframe;
  refine_frequency = 3;

  //call colormap
  dx = (xmax - xmin)/double(array_width);//ckout << "dx: " << dx << endl;
  dy = (ymax - ymin)/double(array_height);//ckout << "dy: " << dy << endl;
  //ckout << min(dx, dy) << endl;	
  //ckout << "dt: " << dt << endl;
  xctr = 0.3;
  yctr = 0.5;
  radius = 0.2;

  ap = max(v, 0.0);
  an = min(v, 0.0);
  //ckout << "ap: " <<ap << endl;
  //ckout << "an: " <<an << endl;

  /*****End Initialization **********/

  CProxy_AdvMap map = CProxy_AdvMap::ckNew();
  CkArrayOptions opts;
  opts.setMap(map);
  ppc = CProxy_PerProcessorChare::ckNew();
  qtree = CProxy_Advection::ckNew(opts);
	//qtree.setstTime();

	CProxy_PowerLogger pLog = CProxy_PowerLogger::ckNew(6);

  //save the total number of worker chares we have in this simulation
  num_chares = num_chare_rows*num_chare_cols;
  double fdepth = (log(num_chares)/log(4));
  int depth = (fabs(fdepth - ceil(fdepth)) < 0.000001)?ceil(fdepth):floor(fdepth);
  min_depth = depth;
  CkAssert(min_depth >= 4);
  // To maintain the semantics of "max_depth" that set it relative to
  // a grid fo 256, offset by 4
  max_depth = atoi(m->argv[1]) + min_depth - 4;

  CkPrintf("Running Advection on %d processors with (%d,%d) elements, minDepth = %d, maxDepth = %d, blockSize = %d, maxIter = %d\n",
           CkNumPes(), array_width, array_height, min_depth, max_depth, block_height, max_iterations);

  /*max_depth = 9;*/

  dt = min(dx,dy)/v * cfl;
  dt /= pow(2., max_depth - min_depth);
  if ((t + dt) >= tmax )
    dt = tmax - t;
  t = t+dt;

  QuadIndex qindex;
  for(int i=0; i < num_chares; i++){
    char* str = decimal_to_binary_string(i,  2*depth);
    qindex = QuadIndex(str);
    qtree[qindex].insert(xmin, xmax, ymin, ymax);
  }
  qtree.doneInserting();

  CkStartQD(CkCallback(CkIndex_Main::startMeshGeneration(), thisProxy));

  //CkCallback *cb = new CkCallback(CkIndex_Main::terminate(), thisProxy);
  //CkCallback *cb = new CkCallback(CkIndex_Advection::startStep(), qtree);
  //qtree.ckSetReductionClient(cb);//sets the default callback for the array

  //CkStartQD(*new CkCallback(CkIndex_Main::terminate(), mainProxy));
  /*queue<QuadIndex> q;
    q.push("");
       
    for(int i=0; i<depth; i++){
    int size = q.size();
    for(int j=0; j<size; j++){
    qindex = q.front(); q.pop();
    qtree(qindex).refine();
    qtree.doneInserting();
    for(int dir=0; dir<4; dir++)
    q.push(qindex.getChild(dir));
    }
    }
    CkVec<QuadIndex> v = *new CkVec<QuadIndex>();
    qtree[qindex].refine();
    v.push_back(qindex);
    v.push_back(qindex.getChild("00")); v.push_back(qindex.getChild("01"));
    v.push_back(qindex.getChild("10")); v.push_back(qindex.getChild("11"));

    qindex = qindex.getChild("00");
    qtree[qindex].refine();
    v.push_back(qindex.getChild("00")); v.push_back(qindex.getChild("01"));
    v.push_back(qindex.getChild("10")); v.push_back(qindex.getChild("11"));
    
    thisProxy.printTreeInformation(v);*/
  //setup - liveViz
  CkCallback c(CkIndex_Advection::requestNextFrame(0), qtree);
    liveVizConfig cfg(liveVizConfig::pix_color, true);
    liveVizInit(cfg, qtree, c);
      
  /*int size = q.size();
    ckout << "Size of Queue " << size << endl;
    for(int i=0; i<size; i++){
    qindex = q.front(); q.pop();
    ckout << "Calling doStep for " << qindex.getIndexString() << endl;
    qtree[qindex].doStep();
    }*/
  /*qtree[qindex].doStep();*/
}

void Main::startMeshGeneration() {
  //CkStartQD(CkCallback(CkIndex_Main::terminate(), mainProxy));
  start_time = CkWallTimer();
  qtree.generateMesh();
  //CkStartQD(CkCallback(CkIndex_Main::startRunning(), mainProxy));
}

void Main::startRunning(){
  start_time = CkWallTimer();
  qtree.doStep();
}

void Main::terminate(){
  ckout << "simulation time: " << CkWallTimer() - start_time << " s" << endl;
  ppc.collectCascades(CkCallback(CkReductionTarget(Main, reportCascadeStats),
                                  thisProxy));
}

void Main::reportCascadeStats(int *cascade_lengths, int size) {
  ckout << "Cascade lengths: ";
  for (int i = 0; i < size; ++i)
    ckout << cascade_lengths[i] << ", ";
  ckout << endl;
  ppc.reduceLatencies();
}

void Main::qdlatency(double* elems, int size) {
  for (unsigned i = 0; i < size; i++) {
    if (elems[i] != std::numeric_limits<double>::max())
      CkPrintf("iteration %u, QD latency = %0.20f\n", i, elems[i]);
  }
}

void Main::remeshlatency(double* elems, int size) {
  for (unsigned i = 0; i < size; i++) {
    if (elems[i] != std::numeric_limits<double>::max())
      CkPrintf("iteration %u, Remesh latency = %0.20f\n", i, elems[i]);
  }
  ppc.reduceWorkUnits();
}

void Main::totalWorkUnits(int total) {
  CkPrintf("total work units = %d\n", total);
  CkExit();
}

void Main::printTreeInformation(CkVec<QuadIndex> list){
  for(int i=0; i<list.size(); i++){
    QuadIndex qindex = list[i];
    qtree[qindex].printState();
  }   
}

#define GOLDEN_RATIO_PRIME_64 0x9e37fffffffc0001ULL

struct AdvMap : public CBase_AdvMap {
  int bits;
  AdvMap() : bits(log2(CkNumPes())) { }

  int procNum(int arrayHdl, const CkArrayIndex& i) {
    int numPes = CkNumPes();
    const QuadIndex& idx = *reinterpret_cast<const QuadIndex*>(i.data());
    int baseBits = 8;

    unsigned long long val = idx.bitVector >> (sizeof(unsigned int)*8 - baseBits);
    unsigned long long hash = GOLDEN_RATIO_PRIME_64 * val;

    int basePE = hash >> (64 - bits);

    unsigned long validBits = idx.bitVector & ((1L << 24) - 1);
    validBits += (1L << 22);
    unsigned long offset = validBits >> (sizeof(unsigned int)*8 - idx.nbits);
    offset += (idx.nbits == 8);

    int pe = (basePE + offset - 1) % numPes;

    // std::cout << "index = " << str
    //           << ", base = " << base
    //           << ", baseIndex = " << baseIndex.getIndexString()
    //           << ", basePE = " << basePE
    //           << ", offset = " << offset
    //           << ", pe = " << pe
    //           << std::endl;

    return pe;
  }
};

#include "Main.def.h"
