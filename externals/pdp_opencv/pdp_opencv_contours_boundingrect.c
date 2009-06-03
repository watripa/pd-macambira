/*
 *   Pure Data Packet module.
 *   Copyright (c) by Tom Schouten <pdp@zzz.kotnet.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <dlfcn.h>
#include <math.h>

#include "pdp.h"

#ifndef _EiC
#include "cv.h"
#endif

#define MAX_MARKERS 100

typedef struct pdp_opencv_contours_boundingrect_struct
{
    t_object x_obj;
    t_float x_f;

    t_outlet *x_outlet0;
    t_outlet *x_dataout;
    t_outlet *x_countout;
    t_atom    rlist[5];
    int x_packet0;
    int x_packet1;
    int x_dropped;
    int x_queue_id;
    int x_xmark[MAX_MARKERS];
    int x_ymark[MAX_MARKERS];
    int x_found[MAX_MARKERS];
    int x_ftolerance;
    int x_mmove;

    int x_width;
    int x_height;
    int x_size;

    int x_infosok; 

    int minarea;
    int maxarea;
    IplImage *image, *gray, *cnt_img;
    CvFont font;
    
} t_pdp_opencv_contours_boundingrect;

static int pdp_opencv_contours_boundingrect_mark(t_pdp_opencv_contours_boundingrect *x, t_floatarg fx, t_floatarg fy )
{
  int i;

    if ( ( fx < 0.0 ) || ( fx > x->x_width ) || ( fy < 0 ) || ( fy > x->x_height ) )
    {
       return -1;
    }

    for ( i=0; i<MAX_MARKERS; i++)
    {
       if ( x->x_xmark[i] == -1 )
       {
          x->x_xmark[i] = (int)fx;
          x->x_ymark[i] = (int)fy;
          x->x_found[i] = x->x_ftolerance;
          return i;
       }
    }

    post( "pdp_opencv_contours_boundingrect : max markers reached" );
    return -1;
}

static void pdp_opencv_contours_boundingrect_process_rgb(t_pdp_opencv_contours_boundingrect *x)
{
    t_pdp     *header = pdp_packet_header(x->x_packet0);
    short int *data   = (short int *)pdp_packet_data(x->x_packet0);
    t_pdp     *newheader = pdp_packet_header(x->x_packet1);
    short int *newdata = (short int *)pdp_packet_data(x->x_packet1); 
    char tindex[4];
    int i = 0;                   // Indicator of cycles.
    int im = 0;                  // Indicator of markers.

    if ((x->x_width != (t_int)header->info.image.width) || 
        (x->x_height != (t_int)header->info.image.height)) 
    {

    	post("pdp_opencv_contours_boundingrect :: resizing plugins");
	

    	x->x_width = header->info.image.width;
    	x->x_height = header->info.image.height;
    	x->x_size = x->x_width*x->x_height;
    
    	//Destroy cv_images
	cvReleaseImage(&x->image);
    	cvReleaseImage(&x->gray);
    	cvReleaseImage(&x->cnt_img);
    
	//create the orig image with new size
        x->image = cvCreateImage(cvSize(x->x_width,x->x_height), IPL_DEPTH_8U, 3);

    	// Create the output images with new sizes
    	x->gray = cvCreateImage(cvSize(x->image->width,x->image->height), IPL_DEPTH_8U, 1);
    	x->cnt_img = cvCreateImage(cvSize(x->image->width,x->image->height), IPL_DEPTH_8U, 3);
    
    }
    
    newheader->info.image.encoding = header->info.image.encoding;
    newheader->info.image.width = x->x_width;
    newheader->info.image.height = x->x_height;

    // FEM UNA COPIA DEL PACKET A image->imageData ... http://www.cs.iit.edu/~agam/cs512/lect-notes/opencv-intro/opencv-intro.html aqui veiem la estructura de IplImage
    memcpy( x->image->imageData, data, x->x_size*3 );
    
    // Convert to grayscale
    cvCvtColor(x->image, x->gray, CV_BGR2GRAY);

    CvSeq* contours;
    CvMemStorage* stor02;
    stor02 = cvCreateMemStorage(0);

    //TODO nous objectes ::: llegeixo el OpenCVRefenceManual i al capitol 11 Structural Analysis Reference
    // m'en adono que
    //de fet aquest objecte no s'ha de dir pdp_opencv_contours sino pdp_opencv_convexity
    //el pdp_opencv_contours et donaria una llista de punts en els outles que serien els punts del contorn (poligonal o no)i
    //i seria la base per a una serie de objectes basats en contorns
    //el pdp_opencv_convexHull, el  mateix pdp_opencv_convexity
    //depres nhi ha un altre que surtiria d'aqui :: pdp_opencv_MinAreaRect i el pdp_opencv_MinEnclosingCircle
    //ContourBoundingRect
    //

    // TODO afegir parametres
    // Retrieval mode.
    // CV_RETR_TREE || CV_RETR_CCOMP || CV_RETR_LIST || CV_RETR_EXTERNAL
    // Approximation method.
    // CV_CHAIN_APPROX_SIMPLE || CV_CHAIN_CODE || CV_CHAIN_APPROX_NONE || CV_CHAIN_APPROX_TC89_L1 || CV_CHAIN_APPROX_TC89_KCOS || CV_LINK_RUNS
    cvFindContours( x->gray, stor02, &contours, sizeof(CvContour), CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, cvPoint(0,0) );
    // TODO afegir parametres
    // aqui es fa una aproximacio del contorn per a que sigui mes polinomic i no tingui tants punts
    // els ultims dos parametres han de ser variables
    //           precision , recursive
    if (contours) contours = cvApproxPoly( contours, sizeof(CvContour), stor02, CV_POLY_APPROX_DP, 3, 1 );

    //TODO afegir parametre
    //si volem veure la imatge original o un fons negre
    cvCopy(x->image, x->cnt_img, NULL);
    // cvZero( x->cnt_img );
    
    for ( im=0; im<MAX_MARKERS; im++ )
    {
      if ( x->x_xmark[im] != -1.0 )
      {
        x->x_found[im]--;
      }
    }

    for( ; contours != 0; contours = contours->h_next )
    {
        int count = contours->total; // This is number point in contour
        CvRect rect;
        int oi, found;

	rect = cvContourBoundingRect( contours, 1);
	if ( ( (rect.width*rect.height) > x->minarea ) && ( (rect.width*rect.height) < x->maxarea ) ) {

            found = 0;
            oi = -1;
            for ( im=0; im<MAX_MARKERS; im++ )
            {
              // check if the object is already known
              if ( ( abs( rect.x - x->x_xmark[im] ) < x->x_mmove ) && ( abs( rect.y - x->x_ymark[im] ) < x->x_mmove ) )
              {
                 oi=im;
                 found=1;
                 x->x_found[im] = x->x_ftolerance;
                 x->x_xmark[im] = rect.x;
                 x->x_ymark[im] = rect.y;
                 break;
              }
            }
            // new object detected
            if ( !found )
            {
               oi = pdp_opencv_contours_boundingrect_mark(x, rect.x, rect.y );
            }

	    cvRectangle( x->cnt_img, cvPoint(rect.x,rect.y), cvPoint(rect.x+rect.width,rect.y+rect.height), CV_RGB(255,0,0), 2, 8 , 0 );
            sprintf( tindex, "%d", i );
            cvPutText( x->cnt_img, tindex, cvPoint(rect.x,rect.y), &x->font, CV_RGB(255,255,255));

            SETFLOAT(&x->rlist[0], i);
            SETFLOAT(&x->rlist[1], rect.x);
            SETFLOAT(&x->rlist[2], rect.y);
            SETFLOAT(&x->rlist[3], rect.width);
            SETFLOAT(&x->rlist[4], rect.height);

    	    outlet_list( x->x_dataout, 0, 5, x->rlist );
	    i++;
        }
    }
    outlet_float( x->x_countout, i );

    // delete lost objects
    for ( im=0; im<MAX_MARKERS; im++ )
    {
       if ( x->x_found[im] < 0 )
       {
         x->x_xmark[im] = -1.0;
         x->x_ymark[im] = -1,0;
         x->x_found[im] = x->x_ftolerance;
         post( "deleted point %d", im );
       }
    }

    cvReleaseMemStorage( &stor02 );

    memcpy( newdata, x->cnt_img->imageData, x->x_size*3 );
 
    return;
}

static void pdp_opencv_contours_boundingrect_param(t_pdp_opencv_contours_boundingrect *x, t_floatarg f1, t_floatarg f2)
{

}

static void pdp_opencv_contours_boundingrect_minarea(t_pdp_opencv_contours_boundingrect *x, t_floatarg f)
{
	x->minarea = (int)f;
}

static void pdp_opencv_contours_boundingrect_maxarea(t_pdp_opencv_contours_boundingrect *x, t_floatarg f)
{
	x->maxarea = (int)f;
}

static void pdp_opencv_contours_boundingrect_ftolerance(t_pdp_opencv_contours_boundingrect *x, t_floatarg f)
{
        if ((int)f>=1) x->x_ftolerance = (int)f;
}

static void pdp_opencv_contours_boundingrect_mmove(t_pdp_opencv_contours_boundingrect *x, t_floatarg f)
{
        if ((int)f>=1) x->x_mmove = (int)f;
}

static void pdp_opencv_contours_boundingrect_delete(t_pdp_opencv_contours_boundingrect *x, t_floatarg findex )
{
  int i;

    if ( ( findex < 1.0 ) || ( findex > 10 ) )
    {
       return;
    }

    x->x_xmark[(int)findex-1] = -1;
    x->x_ymark[(int)findex-1] = -1;
}

static void pdp_opencv_contours_boundingrect_clear(t_pdp_opencv_contours_boundingrect *x )
{
  int i;

    for ( i=0; i<MAX_MARKERS; i++)
    {
      x->x_xmark[i] = -1;
      x->x_ymark[i] = -1;
      x->x_found[i] = x->x_ftolerance;
    }
}


static void pdp_opencv_contours_boundingrect_sendpacket(t_pdp_opencv_contours_boundingrect *x)
{
    /* release the packet */
    pdp_packet_mark_unused(x->x_packet0);
    x->x_packet0 = -1;

    /* unregister and propagate if valid dest packet */
    pdp_packet_pass_if_valid(x->x_outlet0, &x->x_packet1);
}

static void pdp_opencv_contours_boundingrect_process(t_pdp_opencv_contours_boundingrect *x)
{
   int encoding;
   t_pdp *header = 0;
   char *parname;
   unsigned pi;
   int partype;
   float pardefault;
   t_atom plist[2];
   t_atom tlist[2];
   t_atom vlist[2];

   /* check if image data packets are compatible */
   if ( (header = pdp_packet_header(x->x_packet0))
	&& (PDP_BITMAP == header->type)){
    
	/* pdp_opencv_contours_boundingrect_process inputs and write into active inlet */
	switch(pdp_packet_header(x->x_packet0)->info.image.encoding){

	case PDP_BITMAP_RGB:
            x->x_packet1 = pdp_packet_clone_rw(x->x_packet0);
            pdp_queue_add(x, pdp_opencv_contours_boundingrect_process_rgb, pdp_opencv_contours_boundingrect_sendpacket, &x->x_queue_id);
	    break;

	default:
	    /* don't know the type, so dont pdp_opencv_contours_boundingrect_process */
	    break;
	    
	}
    }

}

static void pdp_opencv_contours_boundingrect_input_0(t_pdp_opencv_contours_boundingrect *x, t_symbol *s, t_floatarg f)
{
    /* if this is a register_ro message or register_rw message, register with packet factory */

    if (s == gensym("register_rw")) 
       x->x_dropped = pdp_packet_convert_ro_or_drop(&x->x_packet0, (int)f, pdp_gensym("bitmap/rgb/*") );

    if ((s == gensym("process")) && (-1 != x->x_packet0) && (!x->x_dropped))
    {
        /* add the process method and callback to the process queue */
        pdp_opencv_contours_boundingrect_process(x);
    }
}

static void pdp_opencv_contours_boundingrect_free(t_pdp_opencv_contours_boundingrect *x)
{
  int i;

    pdp_queue_finish(x->x_queue_id);
    pdp_packet_mark_unused(x->x_packet0);
    
    //Destroy cv_images
    cvReleaseImage(&x->image);
    cvReleaseImage(&x->gray);
    cvReleaseImage(&x->cnt_img);
}

t_class *pdp_opencv_contours_boundingrect_class;


void *pdp_opencv_contours_boundingrect_new(t_floatarg f)
{
    int i;

    t_pdp_opencv_contours_boundingrect *x = (t_pdp_opencv_contours_boundingrect *)pd_new(pdp_opencv_contours_boundingrect_class);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("minarea"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("maxarea"));

    x->x_outlet0 = outlet_new(&x->x_obj, &s_anything); 
    x->x_dataout = outlet_new(&x->x_obj, &s_anything); 
    x->x_countout = outlet_new(&x->x_obj, &s_float); 

    x->x_packet0 = -1;
    x->x_packet1 = -1;
    x->x_queue_id = -1;

    x->x_width  = 320;
    x->x_height = 240;
    x->x_size   = x->x_width * x->x_height;

    x->x_infosok = 0;

    x->minarea   = 10*10;
    x->maxarea   = 320*240;

    x->x_ftolerance  = 5;
    x->x_mmove   = 5;

    x->image = cvCreateImage(cvSize(x->x_width,x->x_height), IPL_DEPTH_8U, 3);
    x->gray = cvCreateImage(cvSize(x->image->width,x->image->height), IPL_DEPTH_8U, 1);
    x->cnt_img = cvCreateImage(cvSize(x->image->width,x->image->height), IPL_DEPTH_8U, 3);

    // initialize font
    cvInitFont( &x->font, CV_FONT_HERSHEY_PLAIN, 1.0, 1.0, 0, 1, 8 );

    //contours = 0;
    return (void *)x;
}


#ifdef __cplusplus
extern "C"
{
#endif


void pdp_opencv_contours_boundingrect_setup(void)
{

    post( "		pdp_opencv_contours_boundingrect");
    pdp_opencv_contours_boundingrect_class = class_new(gensym("pdp_opencv_contours_boundingrect"), (t_newmethod)pdp_opencv_contours_boundingrect_new,
    	(t_method)pdp_opencv_contours_boundingrect_free, sizeof(t_pdp_opencv_contours_boundingrect), 0, A_DEFFLOAT, A_NULL);

    class_addmethod(pdp_opencv_contours_boundingrect_class, (t_method)pdp_opencv_contours_boundingrect_input_0, gensym("pdp"),  A_SYMBOL, A_DEFFLOAT, A_NULL);
    class_addmethod(pdp_opencv_contours_boundingrect_class, (t_method)pdp_opencv_contours_boundingrect_minarea, gensym("minarea"),  A_FLOAT, A_NULL );   
    class_addmethod(pdp_opencv_contours_boundingrect_class, (t_method)pdp_opencv_contours_boundingrect_maxarea, gensym("maxarea"),  A_FLOAT, A_NULL );   
    class_addmethod(pdp_opencv_contours_boundingrect_class, (t_method)pdp_opencv_contours_boundingrect_ftolerance, gensym("ftolerance"), A_FLOAT, A_NULL );
    class_addmethod(pdp_opencv_contours_boundingrect_class, (t_method)pdp_opencv_contours_boundingrect_mmove, gensym("maxmove"), A_FLOAT, A_NULL );
    class_addmethod(pdp_opencv_contours_boundingrect_class, (t_method)pdp_opencv_contours_boundingrect_clear, gensym("clear"), A_NULL );

}

#ifdef __cplusplus
}
#endif
