/* photoresize - resample a Tk photo image 
 * The code is loosely based on pamscale from the netpbm package
 * see the copyright notice below */

/* pamscale.c - rescale (resample) a PNM image

   This program evolved out of Jef Poskanzer's program Pnmscale from
   his Pbmplus package (which was derived from Poskanzer's 1989
   Ppmscale).  The resampling logic was taken from Michael Reinelt's
   program Pnmresample, somewhat recoded to follow Netpbm conventions.
   Michael submitted that for inclusion in Netpbm in December 2003.
   The frame of the program is by Bryan Henderson, and the old scaling
   algorithm is based on that in Jef Poskanzer's Pnmscale, but
   completely rewritten by Bryan Henderson ca. 2000.  Plenty of other
   people contributed code changes over the years.

   Copyright (C) 2003 by Michael Reinelt <reinelt@eunet.at>
  
   Copyright (C) 1989, 1991 by Jef Poskanzer.
  
   Permission to use, copy, modify, and distribute this software and its
   documentation for any purpose and without fee is hereby granted, provided
   that the above copyright notice appear in all copies and that both that
   copyright notice and this permission notice appear in supporting
   documentation.  This software is provided "as is" without express or
   implied warranty.
*/
#include "photoresize.hpp"
//#include <iostream>


const double EPSILON = 1e-10;

const double radius_gauss=1.1;

static double 
filter_gauss(double x)
{
    return exp(-2.5*x*x); 
}


double sinc(double x) {
  if (abs(x)<EPSILON) {
   return 1;
  } else {
   return sin(M_PI*x)/(M_PI*x);
  }
}

const double radius_lanczos=3.0;

double filter_lanczos (double x) {
  if (abs(x)>radius_lanczos) { return 0; 
   //cout<<"hit"<<endl;
   }
  return sinc(x)*sinc(x/radius_lanczos);
}

class resample1d {
  std::vector<float> weights;
  std::vector<int> windowleft;
  int sourcelength, targetlength;
  int leftborder, rightborder;
  float filterradius;
  int taps;
public:
  resample1d(int sourcelength_, int targetlength_);
  ~resample1d() { }
  void print_weights();
  template <typename P> void operator () (const P* const sourcearray, P* targetarray, int sskip, int tskip);
  template <typename P> void operator () (const P* const sourcearray, P* targetarray);
};

resample1d::resample1d(int sourcelength_, int targetlength_) :sourcelength(sourcelength_), targetlength(targetlength_) {
  double scale = double (targetlength-1)/(sourcelength-1);
  double fscale;
  if (scale <1.0) {
    filterradius=radius_gauss/scale;
    fscale=scale;
  } else {
    filterradius=radius_gauss;
    fscale=1.0;
  } 
  taps=int(floor(2*filterradius + EPSILON));

  windowleft.reserve(targetlength);
  weights.reserve(taps*targetlength);

  //cout << "# Radius "<<filterradius<<" Taps "<<taps<<endl;

  leftborder=0;
  rightborder=targetlength;

  for (int pos=0; pos<targetlength; pos++) {
    // compute weights
    double windowcenter=pos/scale;
    int windowstart=int(ceil(windowcenter-filterradius-EPSILON));
    windowleft[pos]=windowstart;

    for (int wpos=0; wpos<taps; wpos++) {
      weights[pos*taps+wpos]=filter_gauss((windowstart+wpos-windowcenter)*fscale);
    }

    // normalize
    double norm =0;
    for (int wpos=0; wpos<taps; wpos++) {
      int sourcepos=wpos+windowstart;
      if (sourcepos>=0 && sourcepos<sourcelength) {
        norm+=weights[pos*taps+wpos];
      } else if (sourcepos<0) {
        weights[pos*taps+wpos]=0;
        leftborder=pos+1;
      } else if (sourcepos>=sourcelength) {
        weights[pos*taps+wpos]=0;
        if (rightborder==targetlength) rightborder=pos-1;
      }	
    }

    if (abs(norm)>EPSILON) {
      for (int wpos=0; wpos<taps; wpos++) {
        weights[pos*taps+wpos]/=abs(norm);
      }
    }
  }

  if (rightborder<leftborder) {
    leftborder=targetlength;
    rightborder=targetlength;
  }  

}

void resample1d::print_weights() {
  //cout<<"# Leftborder "<<leftborder<<"Rightborder "<<rightborder<<endl;
  for (int pos=0; pos<targetlength; pos++) {
    //cout<<"# Position "<<pos<<endl;
    for (int wpos=0; wpos<taps; wpos++) {
      //cout<<weights[taps*pos+wpos]<<"\t";
    }
    //cout<<endl;
    for (int wpos=0; wpos<taps; wpos++) {
      // cout<<wpos+windowleft[pos]<<"\t";
    }
    //cout<<endl;
  }  
}

// Accumulator for standard values
template <typename P>
class accum {
  P sum;
public:
  accum() { sum=0; }
  void add (float weight, P value) {
    sum+=weight*value;
  }
  void store (P &where) {
    // typecast operator
    where=sum;
  }  
};



template <typename P> 
void resample1d::operator () (const P* const sourcearray, P* targetarray, int sskip, int tskip) {
  
  for (int pos=0; pos<leftborder; pos++) {
    accum<P> sum;
    int left=windowleft[pos];
    int right=std::min(taps, sourcelength-left);
    for (int wpos=-left; wpos<right; wpos++) {
      sum.add(weights[taps*pos+wpos],sourcearray[(wpos+left)*sskip]);
    }
    sum.store(*targetarray);
    targetarray+=tskip;
  }
  

  for (int pos=leftborder; pos<rightborder; pos++) {
    accum<P> sum;
    int left=windowleft[pos];
    for (int wpos=0; wpos<taps; wpos++) {
      sum.add(weights[taps*pos+wpos],sourcearray[(wpos+left)*sskip]);
    }
    sum.store(*targetarray);
    targetarray+=tskip;
  }


  for (int pos=rightborder; pos<targetlength; pos++) {
     accum<P> sum;
    int left=windowleft[pos];
    for (int wpos=0; wpos<sourcelength-left; wpos++) {
      sum.add(weights[taps*pos+wpos],sourcearray[(wpos+left)*sskip]);
    }
    sum.store(*targetarray);
    targetarray+=tskip;
  }

}

template <typename P> 
void resample1d::operator () (const P* const sourcearray, P* targetarray) {
  
  for (int pos=0; pos<leftborder; pos++) {
    accum<P> sum;
    int left=windowleft[pos];
    int right=std::min(taps, sourcelength-left);
    for (int wpos=-left; wpos<right; wpos++) {
      sum.add(weights[taps*pos+wpos],sourcearray[wpos+left]);
    }
    sum.store(*targetarray++);
  }
  

  for (int pos=leftborder; pos<rightborder; pos++) {
    accum<P> sum;
    int left=windowleft[pos];
    for (int wpos=0; wpos<taps; wpos++) {
      sum.add(weights[taps*pos+wpos],sourcearray[wpos+left]);
    }
    sum.store(*targetarray++);
  }


  for (int pos=rightborder; pos<targetlength; pos++) {
    accum<P> sum;
    int left=windowleft[pos];
    for (int wpos=0; wpos<sourcelength-left; wpos++) {
      sum.add(weights[taps*pos+wpos],sourcearray[wpos+left]);
    }
    sum.store(*targetarray++);
  }

}






typedef unsigned char sample;
const sample maxval=0xFF;
const int depth=4;

struct  tuple {
  sample col[3];
  sample alpha;
};



inline sample
floatToSample(float const value) {

    /* Take care here, the conversion of any floating point value <=
       -1.0 to an unsigned type is _undefined_.  See ISO 9899:1999
       section 6.3.1.4.  Not only is it undefined it also does the
       wrong thing in actual practice, EG on Darwin PowerPC (my iBook
       running OS X) negative values clamp to maxval.  We get negative
       values because some of the filters (EG catrom) have negative
       weights.  
    */

    return sample(std::min(int(maxval), (int)(std::max(0.0, (value + 0.5)))));
}

// Accumulator for tuples
template <>
class accum<tuple> {
  float colsum[3];
  float alphasum;
public:
  accum() : alphasum(0) { for (int c=0; c<3; c++) colsum[c]=0; }

  void add (float weight, const tuple &pxl) {
    alphasum+=pxl.alpha*weight;
    for (int i=0; i<3; i++)
      colsum[i]+=pxl.col[i]*pxl.alpha*weight;
  }

  void store (tuple &where) {
    // typecast operator
    // compute the color value weighted by alpha

    if (alphasum < EPSILON) {
      for (int c=0; c<3; c++) {
       where.col[c]=floatToSample(colsum[c]);
      }
      where.alpha=floatToSample(alphasum);
    } else { 
      for (int c=0; c<3; c++) {
        // all colors except alpha
        where.col[c]=floatToSample(colsum[c]/alphasum);
      }  
      where.alpha=floatToSample(alphasum);
    }
  
  }  
};

// Tk interface function for resampling
std::string resizephoto(Tcl_Interp *interp, 
                 Tk_PhotoHandle sourceh, 
		 Tk_PhotoHandle targeth, 
		 int x0, int y0, int x1, int y1, int xsize, int ysize)  {
/* Copy photo source to target,
 * crop to region (x0,y0) - (x1,y1) and resize to xsize*ysize
   with standard filter options 
   -1 can be given for x1 or y1 to extend to the image border */
  
   // Get adress of the binary data
   Tk_PhotoImageBlock sourceblock;
   Tk_PhotoImageBlock outputline;
   Tk_PhotoGetImage(sourceh, &sourceblock);
   int source_xsize=sourceblock.width;
   int source_ysize=sourceblock.height;
   int source_pitch=sourceblock.pitch;
   int source_pixelSize=sourceblock.pixelSize;

   outputline.width=xsize;
   outputline.height=ysize;
   outputline.pitch=xsize*depth;
   outputline.pixelSize=depth;
   for (int i=0; i<depth;i++)
     outputline.offset[i]=i;
   
   std::vector <tuple> pixelPtr(xsize*ysize);
   outputline.pixelPtr=reinterpret_cast<sample*> (&(pixelPtr[0]));
   
   if (x1 == -1) { x1 = source_xsize - 1; }
   if (y1 == -1) { y1 = source_ysize - 1; }

   #define check_bounds(x, size)  \
	if (x < 0) STHROW(#x " coordinate is < 0") \
	if (x >= size) STHROW(#x " coordinate is larger than the image")


   // Check if the input region is reasonable
   check_bounds(x0, source_xsize);
   check_bounds(x1, source_xsize);
   check_bounds(y0, source_ysize);
   check_bounds(y1, source_ysize);


   if (x1 < x0 || y1 < y0) {
		STHROW("x1 > x0 and y1 > y0 not fulfilled");
	}
   
   
   // set source size to crop size
   source_xsize = x1 - x0 + 1;
   source_ysize = y1 - y0 + 1;
   
   // Allocate space for the intermediate interpolated image
   // First resample along x-direction
   // Size = new_x*old_y
   std::vector <tuple> yresized(xsize*source_ysize);
   // Setup resampling filter
   resample1d xsampler(source_xsize, xsize);
   for (int y=0; y<source_ysize; y++) {
     // resample every line
     tuple *sourceline=reinterpret_cast<tuple*> (sourceblock.pixelPtr + (y + y0)*source_pitch + x0*source_pixelSize);
     tuple *targetline=&(yresized[xsize*y]);
     xsampler(sourceline, targetline);
   }

   // Now resample along y-direction
   // Setup resampling filter
   resample1d ysampler(source_ysize, ysize); 
   for (int x=0; x<xsize; x++) {
     //resample every row
     tuple *sourcerow=&(yresized[x]);
     tuple *targetrow=&(pixelPtr[x]);
     ysampler(sourcerow, targetrow, xsize, xsize);
   }

   // Set the output photo to the requested size
   Tk_PhotoSetSize(interp, targeth, xsize, ysize);
   
   Tk_PhotoPutBlock(interp, targeth, &outputline,0,0,xsize,ysize,TK_PHOTO_COMPOSITE_SET);

   return ("All OK");
}


// Tk interface function for resampling
std::string resizephoto(Tcl_Interp *interp, 
                 Tk_PhotoHandle sourceh, 
		 Tk_PhotoHandle targeth, 
		 int xsize, int ysize)  {

	
	return resizephoto(interp, sourceh, targeth, 0, 0, -1, -1, xsize, ysize);
	
/* Copy photo source to target, resample to xsize*ysize
   with standard filter options */
  
   // Get adress of the binary data
   Tk_PhotoImageBlock sourceblock;
   Tk_PhotoImageBlock outputline;
   Tk_PhotoGetImage(sourceh, &sourceblock);
   int source_xsize=sourceblock.width;
   int source_ysize=sourceblock.height;
   int source_pitch=sourceblock.pitch;
   int source_pixelSize=sourceblock.pixelSize;

   outputline.width=xsize;
   outputline.height=ysize;
   outputline.pitch=xsize*depth;
   outputline.pixelSize=depth;
   for (int i=0; i<depth;i++)
     outputline.offset[i]=i;
   
   std::vector <tuple> pixelPtr(xsize*ysize);
   outputline.pixelPtr=reinterpret_cast<sample*> (&(pixelPtr[0]));
   
   //cout<<"Resampling from ("<<source_xsize<<", "<<source_ysize<<") to ("<<xsize<<", "<<ysize<<") "<<endl;

   // Allocate space for the intermediate interpolated image
   // First resample along x-direction
   // Size = new_x*old_y
   std::vector <tuple> yresized(xsize*source_ysize);
   // Setup resampling filter
   resample1d xsampler(source_xsize, xsize);
   for (int y=0; y<source_ysize; y++) {
     // resample every line
     tuple *sourceline=reinterpret_cast<tuple*> (sourceblock.pixelPtr + y*source_pitch);
     tuple *targetline=&(yresized[xsize*y]);
     xsampler(sourceline, targetline);
   }

   // Now resample along y-direction
   // Setup resampling filter
   resample1d ysampler(source_ysize, ysize); 
   for (int x=0; x<xsize; x++) {
     //resample every row
     tuple *sourcerow=&(yresized[x]);
     tuple *targetrow=&(pixelPtr[x]);
     ysampler(sourcerow, targetrow, xsize, xsize);
   }

   // Set the output photo to the requested size
   Tk_PhotoSetSize(interp, targeth, xsize, ysize);
   
   Tk_PhotoPutBlock(interp, targeth, &outputline,0,0,xsize,ysize,TK_PHOTO_COMPOSITE_SET);

   return ("All OK");
}


/****************************/
/****************************/
/**** end of resampling *****/
/****************************/
/****************************/



