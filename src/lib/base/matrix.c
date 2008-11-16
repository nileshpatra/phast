/***************************************************************************
 * PHAST: PHylogenetic Analysis with Space/Time models
 * Copyright (c) 2002-2005 University of California, 2006-2009 Cornell 
 * University.  All rights reserved.
 *
 * This source code is distributed under a BSD-style license.  See the
 * file LICENSE.txt for details.
 ***************************************************************************/

/* $Id: matrix.c,v 1.9 2008-11-16 02:32:54 acs Exp $ */

/** \file matrix.c
    Matrices of real numbers (doubles)
    \ingroup base
*/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <matrix.h>

#ifdef VECLIB
#include <vecLib/clapack.h>
#define doublereal __CLPK_doublereal
#else
#ifndef SKIP_LAPACK
#include <f2c.h>
#include <clapack.h>
#endif
#endif

#include <math.h>
#include <misc.h>

Matrix *mat_new(int nrows, int ncols) {
  int i;
  Matrix *m = smalloc(sizeof(Matrix));
  m->data = smalloc(nrows * sizeof(void*));
  for (i = 0; i < nrows; i++)
    m->data[i] = smalloc(ncols * sizeof(double));
  m->nrows = nrows;
  m->ncols = ncols;
  return m;
}

Matrix *mat_new_from_array(double **array, int nrows, int ncols) {
  int i, j;
  Matrix *m = mat_new(nrows, ncols);
  for (i = 0; i < nrows; i++)
    for (j = 0; j < ncols; j++)
      m->data[i][j] = array[i][j];
  return m;
}

void mat_free(Matrix *m) {
  int i;
  for (i = 0; i < m->nrows; i++)
    free(m->data[i]);
  free(m->data);
  free(m);
}

double mat_get(Matrix *m, int row, int col) {
  return m->data[row][col];
}

Vector *mat_get_row(Matrix *m, int row) {
  int j;
  Vector *v = vec_new(m->ncols);
  for (j = 0; j < m->ncols; j++)
    v->data[j] = m->data[row][j];
  return v;
}

Vector *mat_get_col(Matrix *m, int col) {
  int i;
  Vector *v = vec_new(m->nrows);
  for (i = 0; i < m->nrows; i++)
    v->data[i] = m->data[i][col];
  return v;
}

void mat_set(Matrix *m, int row, int col, double val) {
  m->data[row][col] = val;
}

void mat_set_identity(Matrix *m) {
  int i, j;
  for (i = 0; i < m->nrows; i++)
    for (j = 0; j < m->ncols; j++)
      m->data[i][j] = (i == j ? 1 : 0);
}

void mat_zero(Matrix *m) {
  int i, j;
  for (i = 0; i < m->nrows; i++)
    for (j = 0; j < m->ncols; j++)
      m->data[i][j] = 0;
}

void mat_set_all(Matrix *m, double val) {
  int i, j;
  for (i = 0; i < m->nrows; i++)
    for (j = 0; j < m->ncols; j++)
      m->data[i][j] = val;
}

void mat_copy(Matrix *dest, Matrix *src) {
  int i, j;
  assert(dest->nrows == src->nrows && dest->ncols == src->ncols);
  for (i = 0; i < dest->nrows; i++)
    for (j = 0; j < dest->ncols; j++)
      dest->data[i][j] = src->data[i][j];
}

Matrix *mat_create_copy(Matrix *src) {
  Matrix *dest = mat_new(src->nrows, src->ncols);
  mat_copy(dest, src);
  return dest;
}

Matrix *mat_transpose(Matrix *src) {
  int i, j;
  Matrix *retval = mat_new(src->ncols, src->nrows);
  for (i = 0; i < src->nrows; i++)
    for (j = 0; j < src->ncols; j++)
      retval->data[j][i] = src->data[i][j];
  return retval;
}

void mat_scale(Matrix *m, double scale_factor) {
  int i, j;
  for (i = 0; i < m->nrows; i++)
    for (j = 0; j < m->ncols; j++)
      m->data[i][j] *= scale_factor;
}

void mat_print(Matrix *m, FILE *F) {
  int i, j;
  char *formatstr = "%11.6f ";
  double min = INFTY;

  /* find minimum non-zero absolute value; if it is very small, then
     print with exponential notation */
  for (i = 0; i < m->nrows; i++) {
    for (j = 0; j< m->ncols; j++) {
      double val = fabs(m->data[i][j]);
      if (val != 0 && val < min) min = val;
    }
  }
  if (min < 1e-3) formatstr = "%14.6e ";

  for (i = 0; i < m->nrows; i++) {
    for (j = 0; j < m->ncols; j++) 
      fprintf(F, formatstr, m->data[i][j]);
    fprintf(F, "\n");
  }
}

void mat_read(Matrix *m, FILE *F) {
  int i, j;
  for (i = 0; i < m->nrows; i++)
    for (j = 0; j < m->ncols; j++)
      fscanf(F, "%lf ", &m->data[i][j]);
}

Matrix *mat_new_from_file(FILE *F, int nrows, int ncols) {
  Matrix *m = mat_new(nrows, ncols);
  mat_read(m, F);
  return m;
}

void mat_mult(Matrix *prod, Matrix *m1, Matrix *m2) {
  assert(m1->ncols == m2->nrows && m1->nrows == m2->ncols && 
         prod->nrows == m1->nrows && prod->ncols == m2->ncols);
  int i, j, k;
  for (i = 0; i < prod->nrows; i++) {
    for (j = 0; j < prod->ncols; j++) {
      prod->data[i][j] = 0;
      for (k = 0; k < m1->ncols; k++) 
	prod->data[i][j] += m1->data[i][k] * m2->data[k][j];
    }
  }
}

void mat_vec_mult(Vector *prod, Matrix *m, Vector *v) {
  int i, j;
  assert(m->nrows == v->size && v->size == prod->size);

  for (i = 0; i < m->nrows; i++) {
    prod->data[i] = 0;
    for (j = 0; j < m->ncols; j++) {
      prod->data[i] += m->data[i][j] * v->data[j];
    }
  }
}

void mat_plus_eq(Matrix *thism, Matrix *addm) {
  int i, j;
  assert(thism->nrows == addm->nrows && thism->ncols == addm->ncols);
  for (i = 0; i < thism->nrows; i++)
    for (j = 0; j < thism->ncols; j++)  
      thism->data[i][j] += addm->data[i][j];
}

void mat_minus_eq(Matrix *thism, Matrix *subm) {
  int i, j;
  assert(thism->nrows == subm->nrows && thism->ncols == subm->ncols);
  for (i = 0; i < thism->nrows; i++)
    for (j = 0; j < thism->ncols; j++)  
      thism->data[i][j] -= subm->data[i][j];
}

void mat_linear_comb(Matrix *dest, Matrix *src1, double coef1, 
                     Matrix *src2, double coef2) {
  int i, j;
  assert(dest->nrows == src1->nrows && dest->ncols == src1->ncols &&
         dest->nrows == src2->nrows && dest->ncols == src2->ncols);
  for (i = 0; i < dest->nrows; i++)
    for (j = 0; j < dest->ncols; j++)  
      dest->data[i][j] = coef1*src1->data[i][j] + coef2*src2->data[i][j];
}

void mat_resize(Matrix *m, int nrows, int ncols) {
  int i;
  for (i = nrows; i < m->nrows; i++) free(m->data[i]);
  m->data = srealloc(m->data, nrows * sizeof(void*));
  for (i = 0; i < nrows; i++)
    m->data[i] = srealloc(m->data[i], ncols * sizeof(double));      
  m->nrows = nrows;
  m->ncols = ncols;
}

/* Invert square, real, nonsymmetric matrix.  Uses LU decomposition
   (LAPACK routines dgetrf and dgetri).  Returns 0 on success, 1 on
   failure. */
int mat_invert(Matrix *M_inv, Matrix *M) {
#ifdef SKIP_LAPACK
  die("ERROR: LAPACK required for matrix inversion.\n");
#else
  int i, j;
  long int info, n = M->nrows;
  long int ipiv[n];
  double tmp[n][n];
  long int lwork = n;
  double work[lwork];

  assert(M->nrows == M->ncols && M_inv->nrows == M_inv->ncols && 
         M->nrows == M_inv->nrows);

  for (i = 0; i < n; i++) 
    for (j = 0; j < n; j++) 
      tmp[i][j] = mat_get(M, j, i);

  dgetrf_(&n, &n, (doublereal*)tmp, &n, ipiv, &info);

  if (info != 0) {
    fprintf(stderr, "ERROR: unable to compute LU factorization of matrix (for matrix inversion); dgetrf returned value of %d.\n", (int)info); 
    return 1;
  }

  dgetri_(&n, (doublereal*)tmp, &n, ipiv, work, &lwork, &info);

  if (info != 0) {
    if (info > 0)
      fprintf(stderr, "ERROR: matrix is singular -- cannot invert.\n");
    else
      fprintf(stderr, "ERROR: unable to invert matrix.  Element %d had an illegal value (according to dgetri).\n", (int)info); 
    return 1;
  }

  for (i = 0; i < M->nrows; i++) 
    for (j = 0; j < M->nrows; j++) 
      mat_set(M_inv, i, j, tmp[j][i]);

#endif
  return 0;
}

/* Compute A = B * C * D where A, B, C, D are square matrices of the
   same dimension, and C is diagonal.  C is described by a vector
   representing its diagonal elements.  */
void mat_mult_diag(Matrix *A, Matrix *B, Vector *C, Matrix *D) {
  int i, j, k;
  int size = C->size;

  assert(A->nrows == A->ncols && A->nrows == B->nrows &&
         B->nrows == B->ncols && B->nrows == C->size && 
         C->size == D->nrows && D->nrows == D->ncols);

  for (i = 0; i < size; i++) {
    for (j = 0; j < size; j++) {
      A->data[i][j] = 0;
      for (k = 0; k < size; k++) 
        A->data[i][j] += B->data[i][k] * C->data[k] * D->data[k][j];
    }
  }
}
