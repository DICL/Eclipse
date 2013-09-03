
const int nbins = 10;
const int bandwidth = 0;
const int MAX_HILBERT_VALUE = 10;

void schedule (Query& q, int boundary [], int nservers) {
 int index = hilbert_value (q);

 for (int s = 0; s < nservers; s++) {
  if (index <= boundary [s]) {
   send (S,Q);
   update (index);
 } 
}

void update (int index) {
 int selected_bin = floor (index/bin_width);
 int freq_sum = 0;

 for (int b = 0; b < nbins; b++) {
  // IF b lies in a interval with center in selected bin
  if (b >= (selected_bin - floor (bandwidth/2)) && b <= (selected_bin + floor (bandwidth/2)))
	 frequency [b] = (frequency [b] * (1 - alpha)) + (alpha/(bandwidth+1));
		
	else
   frequency [b] *= (1 - alpha);

  freq_sum += frequency [b];
 } 
 
 double freq_per_server = freq_sum / nservers;
 int bin_width = MAX_HILBERT_VALUE/nbins;
 int f = frequency [0];
 int i = 0, s = 0, boundary = 0, interpolation = 0;
 freq_sum = 0;

 while (i < nbins) {
  if ((freq_sum + f) <= freq_per_server) { //! If so far freq_sum is less then the estimated per each server
   freq_sum += f;                          //! If so, increment the freq_sum
   f = frequency [++i];                    //! f for the next bin
   bin_width = MAX_HILBER_VALUE/nbins;     //! equal bin_width
   boundary = bin_width * i;               //! boundary for next bin is exact 

  } else { 
   interpolation = (freq_per_server - freq_sum) / f; //! Since previous condition it should be positive
   boundary += interpolation * bin_width;            //! Adjust the boundary 
   boundary [s++] = boundary;                        //Set the boundary
   f -= (freq_per_server - freq_sum);                //Add the diference between this two var
   bin_width -= interpolation * bin_width;           //Since the boundary is now smaller the binwidth should be smaller
   freq_sum = 0;
  }
 } 
}
