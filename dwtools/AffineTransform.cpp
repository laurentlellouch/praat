/* AffineTransform.c
 * 
 * Copyright (C) 1993-2011 David Weenink
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 djmw 2001
 djmw 20020315 GPL header
 djmw 20041027 Added AffineTransform_extractMatrix
 djmw 20050726 Added AffineTransform_extractTranslationVector
 djmw 20061021 printf expects %ld for 'long int'
 djmw 20071012 Added: o_CAN_WRITE_AS_ENCODING.h
 djmw 20110304 Thing_new
 */

#include "AffineTransform.h"
#include "NUM2.h"


static void classAffineTransform_transform (I, double **in, long nrows, double **out)
{
	iam (AffineTransform);

	for (long i = 1; i <= nrows; i++)
	{
		for (long j = 1; j <= my n; j++)
		{
			double tmp = 0;
			for (long k = 1; k <= my n; k++)
			{
				tmp += in[i][k] * my r[k][j];
			}
			out[i][j] = tmp + my t[j];
		}
	}	
}

static Any classAffineTransform_invert (I)
{
	iam (AffineTransform);
	try {
		autoAffineTransform thee = (AffineTransform) Data_copy (me);
		double tolerance = 0.000001;
	
		NUMpseudoInverse (my r, my n, my n, thy r, tolerance); therror
		for (long i = 1; i <= my n; i++)
		{
			thy t[i] = 0;
			for (long j = 1; j <= thy n; j++)
			{
				thy t[i] -= thy r[i][j] * my t[j];
			}
		}
		return thee.transfer();
	} catch (MelderError) { rethrowmzero (me, " not inverted"); } 
}

#include "oo_DESTROY.h"
#include "AffineTransform_def.h"
#include "oo_COPY.h"
#include "AffineTransform_def.h"
#include "oo_EQUAL.h"
#include "AffineTransform_def.h"
#include "oo_CAN_WRITE_AS_ENCODING.h"
#include "AffineTransform_def.h"
#include "oo_WRITE_TEXT.h"
#include "AffineTransform_def.h"
#include "oo_WRITE_BINARY.h"
#include "AffineTransform_def.h"
#include "oo_READ_TEXT.h"
#include "AffineTransform_def.h"
#include "oo_READ_BINARY.h"
#include "AffineTransform_def.h"
#include "oo_DESCRIPTION.h"
#include "AffineTransform_def.h"

class_methods (AffineTransform, Data)
	class_method_local (AffineTransform, destroy)
	class_method_local (AffineTransform, copy)
	class_method_local (AffineTransform, equal)
	class_method_local (AffineTransform, canWriteAsEncoding)
	class_method_local (AffineTransform, writeText)
	class_method_local (AffineTransform, writeBinary)
	class_method_local (AffineTransform, readText)
	class_method_local (AffineTransform, readBinary)
	class_method_local (AffineTransform, description)
	class_method_local (AffineTransform, transform)
	class_method_local (AffineTransform, invert)
class_methods_end

void AffineTransform_init (I, long n)
{
	try {
		iam (AffineTransform);
		if (n < 1) Melder_throw ("Dimensionality must be at least 1.");
		my n = n;
		my r = NUMmatrix<double> (1, n, 1, n);
		my t = NUMvector<double> (1, n);
	} catch (MelderError) { rethrow; } 
}

AffineTransform AffineTransform_create (long n)
{
	try {
		autoAffineTransform me = Thing_new (AffineTransform);
		AffineTransform_init (me.peek(), n);
		return me.transfer();
	} catch (MelderError) { rethrowmzero ("AffineTransform not created."); }
}

Any AffineTransform_invert (I)
{
	iam (AffineTransform);
	AffineTransform thee = NULL;
	if (our invert) thee = (AffineTransform) our invert (me);
	return thee;
}

TableOfReal AffineTransform_extractMatrix (I)
{
	iam (AffineTransform);
	try {
		autoTableOfReal thee = TableOfReal_create (my n, my n);
		NUMdmatrix_copyElements (my r, thy data, 1, my n, 1, my n);
		for (long i = 1; i <= my n; i++)
		{
			wchar_t label[20];
			(void) swprintf (label, 20, L"%ld", i);
			TableOfReal_setRowLabel (thee.peek(), i, label);
			TableOfReal_setColumnLabel (thee.peek(), i, label);
		}
		return thee.transfer();
	} catch (MelderError) { rethrowmzero (me, ": transformation matrix not extracted."); }
}

TableOfReal AffineTransform_extractTranslationVector (I)
{
	iam (AffineTransform);
	try {
		autoTableOfReal thee = TableOfReal_create (1, my n);
		for (long i = 1; i <= my n; i++) thy data[1][i] = my t[i];
		return thee.transfer();
	} catch (MelderError) { rethrowmzero (me, ": translation vector not extracted."); }
}

/* End of file AffineTransform.c */
