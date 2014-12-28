#ifndef __HISTOGRAM__
#define __HISTOGRAM__

#include <iostream>
#include <mapreduce/definitions.hh>

#define MAX_UINT 4294967295

using namespace std;

class histogram {
  private:
    int numserver; // number of server
    int numbin; // number of bin -> number of histogram bin
    // int digit; // number of digits to represent the problem space
    double* querycount; // the data access count to each
    unsigned* boundaries; // the index of end point of each node
    
  public:
    histogram(); // constructs an uninitialized object
    histogram (int numserver, int numbin);   // number of bin and number of digits
    ~histogram();
    
    void initialize(); // partition the problem space equally to each bin
    void init_count(); // initialize the all query counts to zero
    unsigned get_boundary (int index);
    void set_boundary (int index, unsigned boundary);
    double get_count (int index);
    void set_count (int index, double count);
    int get_index (unsigned query);   // return the dedicated node index of query
    int count_query (unsigned query);
    void updateboundary();
    
    void set_numbin (int num);
    int get_numbin();
    
    void set_numserver (int num);
    int get_numserver();
};

histogram::histogram() {
  numbin = -1;
  numserver = -1;
  boundaries = NULL;
  querycount = NULL;
}

histogram::histogram (int numserver, int numbin) {
  boundaries = NULL;
  querycount = NULL;
  
  this->numserver = numserver;
  this->numbin = numbin;
  boundaries = new unsigned[numserver];
  querycount = new double[numbin];
  
  // initialize
  this->initialize();
}

histogram::~histogram() {
  if (boundaries != NULL)
    delete boundaries;
    
  if (querycount != NULL)
    delete querycount;
}

void histogram::initialize() {
  unsigned max = MAX_UINT;
  
  // initialize the boundary
  for (int i = 0; i < numserver - 1; i++) {
    boundaries[i] = (int) ( ( (double) max / (double) numserver) * ( (double) (i + 1)));
  }
  
  boundaries[numserver - 1] = max;
  
  // initialize the query count
  for (int i = 0; i < numbin; i++) {
    querycount[i] = 1.0 / (double) numbin;
  }
}

void histogram::init_count() {
  for (int i = 0; i < numbin; i++) {
    querycount[i] = 1.0 / (double) numbin;
  }
}

void histogram::set_numbin (int num) {
  numbin = num;
  
  if (querycount != NULL)
    delete querycount;
    
  querycount = new double[num];
}

int histogram::get_numbin() {
  return numbin;
}
void histogram::set_numserver (int num) {
  numserver = num;
  
  if (boundaries != NULL)
    delete boundaries;
    
  boundaries = new unsigned[num];
}

int histogram::get_numserver() {
  return numserver;
}

unsigned histogram::get_boundary (int index) {   // the index starts from 0
  if (index >= numserver) {
    cout << "[histogram]Index requested is out of range" << endl;
    return -1;
    
  } else {
    return boundaries[index];
  }
}

void histogram::set_boundary (int index, unsigned boundary) {
  boundaries[index] = boundary;
}

double histogram::get_count (int index) {
  if (index >= numbin) {
    cout << "[histogram]Index requested is out of range" << endl;
    return -1.0;
    
  } else {
    return querycount[index];
  }
  
  return -1.0;
}

void histogram::set_count (int index, double count) {
  querycount[index] = count;
}

void histogram::updateboundary() { // update the boundary according to the query counts
  // sum up the count of all bin and divide it by number of servers(query per server)
  double qps = 0.0;
  double temp = 0.0;
  double stmeter = 0.0;
  int j = 0;
  
  for (int i = 0; i < numbin; i++) {
    qps += querycount[i];
  }
  
  qps = qps / numserver;
  
  // calculate the new boundary of each server(except the boundary of last server)
  for (int i = 0; i < numserver - 1; i++) {
    while (temp + querycount[j] * (1.0 - stmeter) < qps) {
      temp += querycount[j] * (1.0 - stmeter);
      j++;
      
      if (j >= numbin) {
        j--;
        break;
      }
      
      stmeter = 0.0;
    }
    
    if (stmeter == 0.0)
      stmeter = (qps - temp) / querycount[j];
      
    else
      stmeter += (qps - temp) / querycount[j];
      
    boundaries[i] = ( (double) MAX_UINT) * ( ( (double) j + stmeter) / ( (double) numbin));
    
    temp = 0.0;
  }
  
  // re-initialize the count of each bin
  // don't need to do because it is EM-KDE and old queries are automatically faded out
}

int histogram::count_query (unsigned query) {
  // accumulate the query count to the target bin
  int index = (int) ( ( (double) query / (double) MAX_UINT) * (double) numbin);
  
  if (index == numbin)
    index--;
    
  else if (index > numbin)
    cout << "[histogram]Debugging: Cannot find index of requested query." << endl;
    
  // alpha adjustment
  for (int i = 0; i < numbin; i++) {
    querycount[i] *= (1.0 - ALPHA);
  }
  
  // accumulate the alpha value (approximated kernel density estimation with KERNELWIDTH)
  int indexstart;
  int indexend;
  double increment;
  
  if (index - KERNELWIDTH < 0)
    indexstart = 0;
    
  else
    indexstart = index - KERNELWIDTH;
    
  if (index + KERNELWIDTH > numbin - 1)
    indexend = numbin - 1;
    
  else
    indexend = index + KERNELWIDTH;
    
  increment = ALPHA / ( (double) (indexend - indexstart + 1));
  
  for (int i = indexstart; i <= indexend; i++) {
    querycount[i] += increment;
  }
  
  return index; // returns the target index as representative histogram bin
}

int histogram::get_index (unsigned query) {   // return the server index range of which includes the input query
  for (int i = 0; i < numserver; i++) {
    if (query <= boundaries[i])
      return i;
  }
  
  cout << "[histogram]Debugging: Cannot find index of requested query." << endl;
  return -1;
}

#endif
