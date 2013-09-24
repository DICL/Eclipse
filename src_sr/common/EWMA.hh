#ifndef __EWMA_HH_
#define __EWMA_HH_
/*
 * EWMA ema (initial_val, alpha);
 * double new_ema = ema (next_value);
 */

class EWMA {
 public:
  EWMA (double initial_val, double alpha) {
   this->ewma  = initial_val;
   this->alpha = alpha;
  }

  double operator () (int point) {
   ewma += alpha * (point - ewma); 
   return ewma;
  }
  
  double operator () (void) {
   return ewma; 
  } 

 private:
  double ewma, alpha;
};

#endif 
