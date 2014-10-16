#include <glog/logging.h>

#include <vector>
#include "garray.h"

int GArray::Nmachine = -1;
int GArray::Mid = -1;

void GArray::init()
{
    //errorReport(_CFUNC,"need to be replaced by comments");
    GA_Initialize();
    Mid     = GA_Nodeid();
    Nmachine = GA_Nnodes();
}


GArray::GArray(const Shape& shape, int mode):
data_(0),dim_(shape.dim()), myshape_(shape),local_(shape),isvalid_(true) {
  LOG(ERROR)<<"constructor of garray";
  int *dims = new int[dim_];
  int *chunks = new int[dim_];
  for(int i = 0; i < dim_; i++) {
    dims[i] = shape[i];
    chunks[i] = PM();
  }
  if(mode == 1) {
    chunks[0] = 1;
    chunks[1] = dims[1];
    if(dadebugmode && dim_ != 2)
      errorReport(_CFUNC,"not 2dim for comm array");
  }
  //errorReport(_CFUNC,"need to be replaced by comments");//
  LOG(ERROR)<<"before nga create of garray";
  data_ = NGA_Create(C_FLOAT, dim_, dims,"ga", chunks);
  LOG(ERROR)<<"after nga create of garray";
  int *lo = new int[dim_];
  int *hi = new int[dim_];
  //errorReport(_CFUNC,"need to be replaced by comments");//
  LOG(ERROR)<<"before distribution";
  NGA_Distribution(data_, Mid, lo, hi);
  LOG(ERROR)<<"after distribution";
  std::vector<Range> vshape;
  for(int i = 0; i < dim_; i++) {
    if(lo[i] < 0)lo[i] = 0;
    hi[i]++;
    if(hi[i] < 0)hi[i] = 0;
    vshape.push_back(Range(lo[i],hi[i]));
  }
  local_ = Area(vshape);
  //errorReport(_CFUNC,"need to be replaced");
}

void GArray::DeleteStore()
{
    isvalid_ = false;
    //errorReport(_CFUNC,"need to be replaced by comments");//
    GA_Destroy(data_);
}

LArray* GArray::Fetch(const Area& area)const
{
    if(!isvalid())
        errorReport(_CFUNC,"this GA is not valid!");
    int *lo = new int[dim_];
    int *hi = new int[dim_];
    int *ld = new int[dim_-1];
    area.copytoarray(lo,hi,ld);
    area.daout("getch");
    LArray* res = new LArray(area.Areashape());
    float * buff = res->loc();
    //errorReport(_CFUNC,"need to be replaced by comments");//
    LOG(ERROR)<<"before nga get "<<ld[0];
    for (int i = 0; i < dim_; i++) {
      LOG(ERROR)<<lo[i]<<" "<<hi[i];
    }
    NGA_Get(data_, lo, hi, buff, ld);
    LOG(ERROR)<<"after nga get";
    return res;
}

void GArray::Put(LArray& src,const Area& area)
{
    if(!isvalid())
        errorReport(_CFUNC,"this GA is not valid!");
    int *lo = new int[dim_];
    int *hi = new int[dim_];
    int *ld = new int[dim_-1];
    area.copytoarray(lo,hi,ld);
    float * buff = src.loc();
    //errorReport(_CFUNC,"need to be replaced by comments");//
    LOG(ERROR)<<"before nga put"<<lo[0]<<" "<<lo[1]
      <<" "<<hi[0]<<" "<<hi[1]<<" "<<ld[0];
    NGA_Put(data_, lo, hi, buff, ld);
    LOG(ERROR)<<"after nga put";
}
