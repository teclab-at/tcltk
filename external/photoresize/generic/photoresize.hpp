#ifndef PHOTORESIZE_HPP
#define PHOTORESIZE_HPP


#ifdef SWIG

%init{
  #ifdef USE_TK_STUBS
  if (Tk_InitStubs(interp, (char*)"8.1", 0) == NULL) {
    return TCL_ERROR;
  }
  #endif  

  //Tcl_PkgRequire(interp, "Tk", "", false);
}
%include exception.i
%include typemaps.i
%include std_string.i
%{
#include "photoresize.hpp"
%}
%typemap(in) Tk_PhotoHandle {
  $1 = Tk_FindPhoto (interp, Tcl_GetString($input));
  if ($1==NULL) {
    SWIG_exception(SWIG_RuntimeError, "Photo not found");
  }
}

%typecheck(SWIG_TYPECHECK_POINTER) Tk_PhotoHandle {
  $1 = Tk_FindPhoto (interp, Tcl_GetString($input))!=NULL;
}  

%exception {
  try {
    $function
  } catch (const std::string &msg) {
    SWIG_exception(SWIG_RuntimeError, const_cast<char*>(msg.c_str()));
  } catch (...) {
    SWIG_exception(SWIG_RuntimeError, "Some C++-Error");
  }
}

#else 
 // C-preprocessor
#include <tcl.h>
#include <tk.h>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cassert>
//#include <malloc.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

#define STHROW(msg) { \
	 std::ostringstream err;\
	 err<<msg; \
	 throw err.str(); }



#endif //!SWIG

std::string resizephoto(Tcl_Interp *interp, Tk_PhotoHandle source, Tk_PhotoHandle target, int xsize, int ysize);
std::string resizephoto(Tcl_Interp *interp, Tk_PhotoHandle source, Tk_PhotoHandle target, int x0, int y0, int x1, int y1, int xsize, int ysize);
#endif
