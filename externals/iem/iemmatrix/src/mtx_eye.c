/*
 *  iemmatrix
 *
 *  objects for manipulating simple matrices
 *  mostly refering to matlab/octave matrix functions
 *
 * (c) IOhannes m zm�lnig, forum::f�r::uml�ute
 * 
 * IEM, Graz
 *
 * this code is published under the LGPL
 *
 */
#include "iemmatrix.h"

/* ------------------------------------------------------------------------------------- */

/* mtx_eye */
static t_class *mtx_eye_class;
static void *mtx_eye_new(t_symbol *s, int argc, t_atom *argv)
{
  t_matrix *x = (t_matrix *)pd_new(mtx_eye_class);
  int col=0, row=0;
  outlet_new(&x->x_obj, 0);
  x->row = x->col = 0;
  x->atombuffer   = 0;
  switch(argc) {
  case 0:
    break;
  case 1:
    col=row=atom_getfloat(argv);
    break;
  default:
    row=atom_getfloat(argv++);
    col=atom_getfloat(argv);
  }
  if(col<0)col=0;
  if(row<0)row=0;
  if (col*row){
    int n = (col<row)?col:row;
    x->atombuffer = (t_atom *)getbytes((col*row+2)*sizeof(t_atom));
    setdimen(x, row, col);
    matrix_set(x, 0);
    while(n--)SETFLOAT(x->atombuffer+2+n*(1+col), 1);
  }
  return (x);
}
void mtx_eye_setup(void)
{
  mtx_eye_class = class_new(gensym("mtx_eye"), (t_newmethod)mtx_eye_new, 
			    (t_method)matrix_free, sizeof(t_matrix), 0, A_GIMME, 0);
  class_addlist(mtx_eye_class, matrix_eye);
  class_addbang(mtx_eye_class, matrix_bang);
  class_addmethod(mtx_eye_class, (t_method)matrix_eye, gensym("matrix"), A_GIMME, 0);

  class_sethelpsymbol(mtx_eye_class, gensym("iemmatrix/mtx_special"));
}
void iemtx_eye_setup(void){
  mtx_eye_setup();
}
