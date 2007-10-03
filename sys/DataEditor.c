/* DataEditor.c
 *
 * Copyright (C) 1995-2007 Paul Boersma
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * pb 1998/05/17 mac 64-bit floats only
 * pb 1998/05/18 "verticalScrollBar"
 * pb 1998/10/22 removed now duplicate "parent" attribute
 * pb 1998/11/04 removed BUG: added assignment to "my parent"
 * pb 1999/06/21 removed BUG: dcomplex read %lf
 * pb 2003/05/19 Melder_atof
 * pb 2005/12/04 wider names
 * pb 2007/06/10 wchar_t
 * pb 2007/08/12 wchar_t
 */

#define NAME_X  30
#define TEXT_X  250
#define BUTTON_X  250
#define EDITOR_WIDTH  700
#define EDITOR_HEIGHT  (LIST_Y + MAXNUM_ROWS * ROW_HEIGHT + 29 + Machine_getMenuBarHeight ())
#define LIST_Y  40
#define LIST_TOP_MARGIN  11
#define ROW_HEIGHT  31

#define SCROLL_BAR_WIDTH  Machine_getScrollBarWidth ()

#include "DataEditor.h"
#include "EditorM.h"
#include "Collection.h"
#include "machine.h"

#define MAXNUM_ROWS  12

/*static const char * typeStrings [] = { "none",
	"byte", "short", "int", "long", "ubyte", "ushort", "uint", "ulong", "bool",
	"float", "double", "fcomplex", "dcomplex", "char", "wchar",
	"enum", "lenum", "boolean", "question", "string", "stringw", "lstring", "lstringw",
	"struct", "widget", "object", "collection" };*/
static int stringLengths [] = { 0,
	4, 6, 6, 11, 3, 5, 5, 10, 1,
	15, 27, 35, 59, 4, 6,
	33, 33, 8, 6, 60, 60, 60, 60 };

typedef struct structDataSubEditor_FieldData {
	Widget label, button, text;
	void *address;
	Data_Description description;
	long minimum, maximum, min2, max2;
	wchar_t *history;   /* The full prefix of the members. */
	int rank;   /* Should the button open a StructEditor (0) or VectorEditor (1) or MatrixEditor (2) ? */
} *DataSubEditor_FieldData;

#define DataSubEditor_members Editor_members \
	DataEditor root; \
	void *address; \
	Data_Description description; \
	Widget scrollBar; \
	int irow, topField, numberOfFields; \
	struct structDataSubEditor_FieldData fieldData [1 + MAXNUM_ROWS];
#define DataSubEditor_methods Editor_methods \
	long (*countFields) (I); \
	void (*showMembers) (I);
class_create (DataSubEditor, Editor);

#define VectorEditor_members DataSubEditor_members \
	long minimum, maximum;
#define VectorEditor_methods DataSubEditor_methods
class_create (VectorEditor, DataSubEditor);

static VectorEditor VectorEditor_create (DataEditor root, const wchar_t *title, void *address,
	Data_Description description, long minimum, long maximum);

#define MatrixEditor_members DataSubEditor_members \
	long minimum, maximum, min2, max2;
#define MatrixEditor_methods DataSubEditor_methods
class_create (MatrixEditor, DataSubEditor);

static MatrixEditor MatrixEditor_create (DataEditor root, const wchar_t *title, void *address,
	Data_Description description, long min1, long max1, long min2, long max2);

#define StructEditor_members DataSubEditor_members
#define StructEditor_methods DataSubEditor_methods
class_create (StructEditor, DataSubEditor);

static StructEditor StructEditor_create (DataEditor root, const wchar_t *title, void *address, Data_Description description);

#define ClassEditor_members StructEditor_members
#define ClassEditor_methods StructEditor_methods
class_create (ClassEditor, StructEditor);

static ClassEditor ClassEditor_create (DataEditor root, const wchar_t *title, void *address, Data_Description description);

#define DataEditor_members ClassEditor_members \
	Collection children;
#define DataEditor_methods ClassEditor_methods
class_create_opaque (DataEditor, ClassEditor);

/********** DataSubEditor **********/

static void classDataSubEditor_destroy (I) {
	iam (DataSubEditor);
	for (int i = 1; i <= MAXNUM_ROWS; i ++)
		Melder_free (my fieldData [i]. history);
	if (my root && my root -> children) for (int i = my root -> children -> size; i > 0; i --)
		if (my root -> children -> item [i] == me)
			Collection_subtractItem (my root -> children, i);
	inherited (DataSubEditor) destroy (me);
}

static void update (DataSubEditor me) {

	/* Hide all the existing widgets. */

	for (int i = 1; i <= MAXNUM_ROWS; i ++) {
		my fieldData [i]. address = NULL;
		my fieldData [i]. description = NULL;
		XtUnmanageChild (my fieldData [i]. label);
		XtUnmanageChild (my fieldData [i]. button);
		XtUnmanageChild (my fieldData [i]. text);
	}

	my irow = 0;
	our showMembers (me);
}

static Data_Description DataSubEditor_findNumberUse (DataSubEditor me, const wchar_t *number) {
	Data_Description structDescription, result;
	wchar_t string [100];
	if (my methods == (DataSubEditor_Table) classMatrixEditor) return NULL;   /* No structs inside. */
	if (my methods == (DataSubEditor_Table) classVectorEditor) {
		if (my description -> type != structwa) return NULL;   /* No structs inside. */
		structDescription = (Data_Description) my description -> tagType;
	} else { /* StructEditor or ClassEditor or DataEditor. */
		structDescription = my description;
	}
	swprintf (string, 100, L"my %ls", number);
	if ((result = Data_Description_findNumberUse (structDescription, string)) != NULL) return result;
	swprintf (string, 100, L"my %ls - 1", number);
	if ((result = Data_Description_findNumberUse (structDescription, string)) != NULL) return result;
	return NULL;
}

MOTIF_CALLBACK (cb_change)
	iam (DataSubEditor);
	int i;
	for (i = 1; i <= MAXNUM_ROWS; i ++) if (XtIsManaged (my fieldData [i]. text)) {
		int type = my fieldData [i]. description -> type;
		wchar_t *text;
		if (type > maxsingletypewa) continue;
		text = GuiText_getStringW (my fieldData [i]. text);
		switch (type) {
			case bytewa: {
				signed char oldValue = * (signed char *) my fieldData [i]. address, newValue = wcstol (text, NULL, 10);
				if (newValue != oldValue) {
					Data_Description numberUse = DataSubEditor_findNumberUse (me, my fieldData [i]. description -> name);
					if (numberUse) {
						Melder_error5 (L"Changing field \"", my fieldData [i]. description -> name,
							L"\" would damage the array \"", numberUse -> name, L"\".");
						Melder_flushError (NULL);
					} else {
						* (signed char *) my fieldData [i]. address = newValue;
					}
				}
			} break;
			case shortwa: {
				short oldValue = * (short *) my fieldData [i]. address, newValue = wcstol (text, NULL, 10);
				if (newValue != oldValue) {
					Data_Description numberUse = DataSubEditor_findNumberUse (me, my fieldData [i]. description -> name);
					if (numberUse) {
						Melder_error5 (L"Changing field \"", my fieldData [i]. description -> name,
							L"\" would damage the array \"", numberUse -> name, L"\".");
						Melder_flushError (NULL);
					} else {
						* (short *) my fieldData [i]. address = newValue;
					}
				}
			} break;
			case intwa: {
				int oldValue = * (int *) my fieldData [i]. address, newValue = wcstol (text, NULL, 10);
				if (newValue != oldValue) {
					Data_Description numberUse = DataSubEditor_findNumberUse (me, my fieldData [i]. description -> name);
					if (numberUse) {
						Melder_error5 (L"Changing field \"", my fieldData [i]. description -> name,
							L"\" would damage the array \"", numberUse -> name, L"\".");
						Melder_flushError (NULL);
					} else {
						* (int *) my fieldData [i]. address = newValue;
					}
				}
			} break;
			case longwa: {
				long oldValue = * (long *) my fieldData [i]. address, newValue = wcstol (text, NULL, 10);
				if (newValue != oldValue) {
					Data_Description numberUse = DataSubEditor_findNumberUse (me, my fieldData [i]. description -> name);
					if (numberUse) {
						Melder_error5 (L"Changing field \"", my fieldData [i]. description -> name,
							L"\" would damage the array \"", numberUse -> name, L"\".");
						Melder_flushError (NULL);
					} else {
						* (long *) my fieldData [i]. address = newValue;
					}
				}
			} break;
			case ubytewa: { * (unsigned char *) my fieldData [i]. address = wcstoul (text, NULL, 10); } break;
			case ushortwa: { * (unsigned short *) my fieldData [i]. address = wcstoul (text, NULL, 10); } break;
			case uintwa: { * (unsigned int *) my fieldData [i]. address = wcstoul (text, NULL, 10); } break;
			case ulongwa: { * (unsigned long *) my fieldData [i]. address = wcstoul (text, NULL, 10); } break;
			case boolwa: { * (bool *) my fieldData [i]. address = wcstol (text, NULL, 10); } break;
			case floatwa: { * (float *) my fieldData [i]. address = Melder_atofW (text); } break;
			case doublewa: { * (double *) my fieldData [i]. address = Melder_atofW (text); } break;
			case fcomplexwa: { fcomplex *x = (fcomplex *) my fieldData [i]. address;
				swscanf (text, L"%f + %f i", & x -> re, & x -> im); } break;
			case dcomplexwa: { dcomplex *x = (dcomplex *) my fieldData [i]. address;
				swscanf (text, L"%lf + %lf i", & x -> re, & x -> im); } break;
			case charwa: { * (char *) my fieldData [i]. address = wcstol (text, NULL, 10); } break;
			case enumwa: {
				if (wcslen (text) < 3) goto error;
				text [wcslen (text) - 1] = '\0';   /* Remove trailing ">". */
				int value = enum_search (my fieldData [i]. description -> tagType, text + 1);   /* Skip leading "<". */
				if (value < 0) goto error;
				* (signed char *) my fieldData [i]. address = value;
			} break;
			case lenumwa: {
				if (wcslen (text) < 3) goto error;
				text [wcslen (text) - 1] = '\0';   /* Remove trailing ">". */
				int value = enum_search (my fieldData [i]. description -> tagType, text + 1);   /* Skip leading "<". */
				if (value < 0) goto error;
				* (signed short *) my fieldData [i]. address = value;
			} break;
			case booleanwa: {
				int value = wcsnequ (text, L"<true>", 6) ? 1 : wcsnequ (text, L"<false>", 7) ? 0 : -1;
				if (value < 0) goto error;
				* (signed char *) my fieldData [i]. address = value;
			} break;
			case questionwa: {
				int value = wcsnequ (text, L"<yes>", 5) ? 1 : wcsnequ (text, L"<no>", 4) ? 0 : -1;
				if (value < 0) goto error;
				* (signed char *) my fieldData [i]. address = value;
			} break;
			case stringwa:
			case stringwwa:
			case lstringwa:
			case lstringwwa: {
				wchar_t *old = * (wchar_t **) my fieldData [i]. address;
				Melder_free (old);
				* (wchar_t **) my fieldData [i]. address = Melder_wcsdup (text);
			} break;
			default: break;
		}
		Melder_free (text);
	}
	/* Several collaborators have to be notified of this change:
	 * 1. The owner (creator) of our root DataEditor: so that she can notify other editors, if any.
	 * 2. All our sibling DataSubEditors.
	 */
	if (my root -> dataChangedCallback)
		my root -> dataChangedCallback (my root, my root -> dataChangedClosure, NULL);   /* Notify owner. */
	update (me);
	for (i = 1; i <= my root -> children -> size; i ++) {
		DataSubEditor subeditor = my root -> children -> item [i];
		if (subeditor != me) update (subeditor);
	}
	return;
error:
	Melder_error3 (L"Edit field \"", my fieldData [i]. description -> name, L"\" or click \"Cancel\".");
	Melder_flushError (NULL);
MOTIF_CALLBACK_END

MOTIF_CALLBACK (cb_cancel)
	iam (DataSubEditor);
	update (me);
MOTIF_CALLBACK_END

MOTIF_CALLBACK (cb_scroll)
	iam (DataSubEditor);
	int value, slider, incr, pincr;
	XmScrollBarGetValues (w, & value, & slider, & incr, & pincr);
	my topField = value + 1;
	update (me);
MOTIF_CALLBACK_END

MOTIF_CALLBACK (cb_open)
	iam (DataSubEditor);
	int ifield = 0;
	static MelderStringW name = { 0 };
	MelderStringW_empty (& name);
	DataSubEditor_FieldData fieldData;

	/* Identify the pressed button; it must be one of those created in the list. */

	for (int i = 1; i <= MAXNUM_ROWS; i ++) if (my fieldData [i]. button == w) { ifield = i; break; }
	Melder_assert (ifield != 0);

	/* Launch the appropriate subeditor. */

	fieldData = & my fieldData [ifield];
	if (! fieldData -> description) {
		Melder_casual ("Not yet implemented.");
		return;   /* Not yet implemented. */
	}

	if (fieldData -> description -> rank == 1 || fieldData -> description -> rank == 3 || fieldData -> description -> rank < 0) {
		MelderStringW_append8 (& name, fieldData -> history, L". ", fieldData -> description -> name,
			L" [", Melder_integer (fieldData -> minimum), L"..", Melder_integer (fieldData -> maximum), L"]");
		if (! VectorEditor_create (my root, name.string, fieldData -> address,
			fieldData -> description, fieldData -> minimum, fieldData -> maximum)) Melder_flushError (NULL);
	} else if (fieldData -> description -> rank == 2) {
		MelderStringW_append8 (& name, fieldData -> history, L". ", fieldData -> description -> name,
			L" [", Melder_integer (fieldData -> minimum), L"..", Melder_integer (fieldData -> maximum), L"]");
		MelderStringW_append5 (& name, L" [", Melder_integer (fieldData -> min2), L"..", Melder_integer (fieldData -> max2), L"]");
		if (! MatrixEditor_create (my root, name.string, fieldData -> address, fieldData -> description,
			fieldData -> minimum, fieldData -> maximum, fieldData -> min2, fieldData -> max2)) Melder_flushError (NULL);
	} else if (fieldData -> description -> type == structwa) {
		MelderStringW_append3 (& name, fieldData -> history, L". ", fieldData -> description -> name);
		if (! StructEditor_create (my root, name.string, fieldData -> address,
			(Data_Description) fieldData -> description -> tagType)) Melder_flushError (NULL);
	} else if (fieldData -> description -> type == objectwa || fieldData -> description -> type == collectionwa) {
		MelderStringW_append3 (& name, fieldData -> history, L". ", fieldData -> description -> name);
		if (! ClassEditor_create (my root, name.string, fieldData -> address,
			((Data_Table) fieldData -> description -> tagType) -> description)) Melder_flushError (NULL);
	} else /*if (fieldData -> description -> type == inheritwa)*/ {
		if (! ClassEditor_create (my root, fieldData -> history, fieldData -> address,
			fieldData -> description)) Melder_flushError (NULL);
/*	} else {
		Melder_casual ("Strange editor \"%s\" required (type %d, rank %d).",
			fieldData -> description -> name,
			fieldData -> description -> type,
			fieldData -> description -> rank);*/
	}
MOTIF_CALLBACK_END

static void classDataSubEditor_createChildren (I) {
	iam (DataSubEditor);
	Widget button, scrolledWindow, board;

	button = XtVaCreateManagedWidget
		("Change", xmPushButtonWidgetClass, my dialog,
		 XmNx, 30, XmNy, 5 + Machine_getMenuBarHeight (), XmNwidth, 120, NULL);
	XtAddCallback (button, XmNactivateCallback, cb_change, (XtPointer) me);

	button = XtVaCreateManagedWidget
		("Cancel", xmPushButtonWidgetClass, my dialog,
		 XmNx, 170, XmNy, 5 + Machine_getMenuBarHeight (), XmNwidth, 120, NULL);
	XtAddCallback (button, XmNactivateCallback, cb_cancel, (XtPointer) me);

	scrolledWindow = XmCreateScrolledWindow (my dialog, "list", NULL, 0);
	XtVaSetValues (scrolledWindow, 
		XmNrightAttachment, XmATTACH_FORM,
		XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, LIST_Y + Machine_getMenuBarHeight (),
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM, NULL);

	my scrollBar = XtVaCreateManagedWidget ("verticalScrollBar",
		xmScrollBarWidgetClass, scrolledWindow,
		XmNheight, SCROLL_BAR_WIDTH,
		XmNminimum, 0,
		XmNmaximum, my numberOfFields,
		XmNvalue, 0,
		XmNsliderSize, my numberOfFields < MAXNUM_ROWS ? my numberOfFields : MAXNUM_ROWS,
		XmNincrement, 1, XmNpageIncrement, MAXNUM_ROWS - 1,
		NULL);
	XtVaSetValues (scrolledWindow, XmNverticalScrollBar, my scrollBar, NULL);
	XtManageChild (scrolledWindow);
	XtAddCallback (my scrollBar, XmNvalueChangedCallback, cb_scroll, (XtPointer) me);

	board = XmCreateBulletinBoard (scrolledWindow, "list", NULL, 0);
	for (int i = 1; i <= MAXNUM_ROWS; i ++) {
		int y = LIST_TOP_MARGIN + (i - 1) * ROW_HEIGHT;
		my fieldData [i]. label = XtVaCreateWidget ("label", xmLabelWidgetClass, board,
			XmNy, y, NULL);   /* No fixed x value: sometimes indent. */
		my fieldData [i]. button = XtVaCreateWidget ("Open", xmPushButtonWidgetClass, board,
			XmNy, y, XmNx, BUTTON_X, NULL);
		my fieldData [i]. text = XtVaCreateWidget ("text", xmTextWidgetClass, board,
			XmNeditable, True, XmNy, y, XmNx, TEXT_X, NULL);
		XtAddCallback (my fieldData [i]. button, XmNactivateCallback, cb_open, (XtPointer) me);
	}
	XtManageChild (board);
}

DIRECT (DataSubEditor, cb_help) Melder_help (L"Inspect"); END

static void classDataSubEditor_createMenus (I) {
	iam (DataSubEditor);
	inherited (DataSubEditor) createMenus (me);
	Editor_addCommand (me, L"Help", L"DataEditor help", '?', cb_help);
}

static long classDataSubEditor_countFields (I) { iam (DataSubEditor); (void) me; return 0; }

static void classDataSubEditor_showMembers (I) { iam (DataSubEditor); (void) me; }

class_methods (DataSubEditor, Editor)
	class_method_local (DataSubEditor, destroy)
	class_method_local (DataSubEditor, createChildren)
	class_method_local (DataSubEditor, createMenus)
	class_method_local (DataSubEditor, countFields)
	class_method_local (DataSubEditor, showMembers)
	us -> scriptable = FALSE;
class_methods_end

static int DataSubEditor_init (I, DataEditor root, const wchar_t *title, void *address, Data_Description description) {
	iam (DataSubEditor);
	my root = root;
	if (me != (DataSubEditor) root) Collection_addItem (root -> children, me);
	my address = address;
	my description = description;
	my topField = 1;
	my numberOfFields = our countFields (me);
	if (! Editor_init (me, root -> parent, 0, 0, EDITOR_WIDTH, EDITOR_HEIGHT, title, NULL)) return 0;
	update (me);
	return 1;
}

/********** StructEditor **********/

static long classStructEditor_countFields (I) {
	iam (StructEditor);
	return Data_Description_countMembers (my description);
}

static wchar_t * singleTypeToText (void *address, int type, void *tagType, MelderStringW *buffer) {
	switch (type) {
		case bytewa: MelderStringW_append1 (buffer, Melder_integer (* (signed char *) address)); break;
		case shortwa: MelderStringW_append1 (buffer, Melder_integer (* (short *) address)); break;
		case intwa: MelderStringW_append1 (buffer, Melder_integer (* (int *) address)); break;
		case longwa: MelderStringW_append1 (buffer, Melder_integer (* (long *) address)); break;
		case ubytewa: MelderStringW_append1 (buffer, Melder_integer (* (unsigned char *) address)); break;
		case ushortwa: MelderStringW_append1 (buffer, Melder_integer (* (unsigned short *) address)); break;
		case uintwa: MelderStringW_append1 (buffer, Melder_integer (* (unsigned int *) address)); break;
		case ulongwa: MelderStringW_append1 (buffer, Melder_integer (* (unsigned long *) address)); break;
		case boolwa: MelderStringW_append1 (buffer, Melder_integer (* (bool *) address)); break;
		case floatwa: MelderStringW_append1 (buffer, Melder_single (* (float *) address)); break;
		case doublewa: MelderStringW_append1 (buffer, Melder_double (* (double *) address)); break;
		case fcomplexwa: { fcomplex value = * (fcomplex *) address;
			MelderStringW_append4 (buffer, Melder_single (value. re), L" + ", Melder_single (value. im), L" i"); } break;
		case dcomplexwa: { dcomplex value = * (dcomplex *) address;
			MelderStringW_append4 (buffer, Melder_double (value. re), L" + ", Melder_double (value. im), L" i"); } break;
		case charwa: MelderStringW_append1 (buffer, Melder_integer (* (wchar_t *) address)); break;
		case enumwa: MelderStringW_append3 (buffer, L"<", enum_string (tagType, * (signed char *) address), L">"); break;
		case lenumwa: MelderStringW_append3 (buffer, L"<", enum_string (tagType, * (signed short *) address), L">"); break;
		case booleanwa: MelderStringW_append1 (buffer, * (signed char *) address ? L"<true>" : L"<false>"); break;
		case questionwa: MelderStringW_append1 (buffer, * (signed char *) address ? L"<yes>" : L"<no>"); break;
		case stringwa:
		case stringwwa:
		case lstringwa:
		case lstringwwa: {
			wchar_t *string = * (wchar_t **) address;
			if (string == NULL) { MelderStringW_empty (buffer); return buffer -> string; }   // Convert NULL string to empty string.
			return string;   // May be much longer than the usual size of 'buffer'.
		} break;
		default: return L"(unknown)";
	}
	return buffer -> string;   // Mind the special return for strings above.
}

static void showStructMember (
	void *structAddress,   /* The address of (the first member of) the struct. */
	Data_Description structDescription,   /* The description of (the first member of) the struct. */
	Data_Description memberDescription,   /* The description of the current member. */
	DataSubEditor_FieldData fieldData,   /* The widgets in which to show the info about the current member. */
	wchar_t *history)
{
	int type = memberDescription -> type, rank = memberDescription -> rank, isSingleType = type <= maxsingletypewa && rank == 0;
	unsigned char *memberAddress = (unsigned char *) structAddress + memberDescription -> offset;
	static MelderStringW buffer = { 0 };
	MelderStringW_empty (& buffer);
	if (type == inheritwa) {
		MelderStringW_append3 (& buffer, L"Class part \"", memberDescription -> name, L"\":");
	} else {
		MelderStringW_append2 (& buffer, memberDescription -> name,
			rank == 0 ? L"" : rank == 1 || rank == 3 || rank < 0 ? L" [ ]" : L" [ ] [ ]");
	}
	XtVaSetValues (fieldData -> label, motif_argXmString (XmNlabelString, Melder_peekWcsToUtf8 (buffer.string)),
		XmNx, type == inheritwa ? 0 : NAME_X, NULL);
	XtManageChild (fieldData -> label);

	/* Show the value (for a single type) or a button (for a composite type). */

	if (isSingleType) {
		XtVaSetValues (fieldData -> text, XmNcolumns, stringLengths [type], NULL);
		MelderStringW_empty (& buffer);
		wchar_t *text = singleTypeToText (memberAddress, type, memberDescription -> tagType, & buffer);
		GuiText_setStringW (fieldData -> text, text);
		XtManageChild (fieldData -> text);
		fieldData -> address = memberAddress;
		fieldData -> description = memberDescription;
		fieldData -> rank = 0;
	} else if (rank == 1) {
		void *arrayAddress = * (void **) memberAddress;
		long minimum, maximum;
		if (arrayAddress == NULL) return;   /* No button for empty fields. */
		Data_Description_evaluateInteger (structAddress, structDescription,
			memberDescription -> min1,  & minimum);
		Data_Description_evaluateInteger (structAddress, structDescription,
			memberDescription -> max1, & maximum);
		if (maximum < minimum) return;   /* No button if no elements. */
		fieldData -> address = arrayAddress;   /* Indirect. */
		fieldData -> description = memberDescription;
		fieldData -> minimum = minimum;   /* Normally 1. */
		fieldData -> maximum = maximum;
		fieldData -> rank = 1;
		Melder_free (fieldData -> history); fieldData -> history = Melder_wcsdup (history);
		XtManageChild (fieldData -> button);
	} else if (rank < 0) {
		/*
		 * This represents an in-line array.
		 */
		long maximum;   /* But: capacity = - rank */
		Data_Description_evaluateInteger (structAddress, structDescription,
			memberDescription -> max1, & maximum);
		if (-- maximum < 0) return;   /* Subtract one for zero-based array; no button if no elements. */
		fieldData -> address = memberAddress;   /* Direct. */
		fieldData -> description = memberDescription;
		fieldData -> minimum = 0;   /* In-line arrays start with index 0. */
		fieldData -> maximum = maximum;   /* Probably between -1 and capacity - 1. */
		fieldData -> rank = rank;
		Melder_free (fieldData -> history); fieldData -> history = Melder_wcsdup (history);
		XtManageChild (fieldData -> button);
	} else if (rank == 3) {
		/*
		 * This represents an in-line set.
		 */
		fieldData -> address = memberAddress;   /* Direct. */
		fieldData -> description = memberDescription;
		fieldData -> minimum = wcsequ (enum_string (memberDescription -> max1, 0), L"_") ? 1 : 0;
		fieldData -> maximum = enum_length (memberDescription -> max1);
		fieldData -> rank = rank;
		Melder_free (fieldData -> history); fieldData -> history = Melder_wcsdup (history);
		XtManageChild (fieldData -> button);
	} else if (rank == 2) {
		void *arrayAddress = * (void **) memberAddress;
		long min1, max1, min2, max2;
		if (arrayAddress == NULL) return;   /* No button for empty fields. */
		Data_Description_evaluateInteger (structAddress, structDescription,
			memberDescription -> min1,  & min1);
		Data_Description_evaluateInteger (structAddress, structDescription,
			memberDescription -> max1, & max1);
		Data_Description_evaluateInteger (structAddress, structDescription,
			memberDescription -> min2,  & min2);
		Data_Description_evaluateInteger (structAddress, structDescription,
			memberDescription -> max2, & max2);
		if (max1 < min1 || max2 < min2) return;   /* No button if no elements. */
		fieldData -> address = arrayAddress;   /* Indirect. */
		fieldData -> description = memberDescription;
		fieldData -> minimum = min1;   /* Normally 1. */
		fieldData -> maximum = max1;
		fieldData -> min2 = min2;
		fieldData -> max2 = max2;
		fieldData -> rank = 2;
		Melder_free (fieldData -> history); fieldData -> history = Melder_wcsdup (history);
		XtManageChild (fieldData -> button);
	} else if (type == structwa) {   /* In-line struct. */
		fieldData -> address = memberAddress;   /* Direct. */
		fieldData -> description = memberDescription;
		fieldData -> rank = 0;
		Melder_free (fieldData -> history); fieldData -> history = Melder_wcsdup (history);
		XtManageChild (fieldData -> button);
	} else if (type == objectwa || type == collectionwa) {
		fieldData -> address = * (Data *) memberAddress;   /* Indirect. */
		if (! fieldData -> address) return;   /* No button if no object. */
		fieldData -> description = memberDescription;
		fieldData -> rank = 0;
		Melder_free (fieldData -> history); fieldData -> history = Melder_wcsdup (history);
		XtManageChild (fieldData -> button);
	}
}

static void showStructMembers (I, void *structAddress, Data_Description structDescription, int fromMember, wchar_t *history) {
	iam (DataSubEditor);
	int i;
	Data_Description memberDescription;
	for (i = 1, memberDescription = structDescription;
	     i < fromMember && memberDescription -> name != NULL;
	     i ++, memberDescription ++)
		(void) 0;
	for (; memberDescription -> name != NULL; memberDescription ++) {
		if (++ my irow > MAXNUM_ROWS) return;
		showStructMember (structAddress, structDescription, memberDescription, & my fieldData [my irow], history);
	}
}

static void classStructEditor_showMembers (I) {
	iam (StructEditor);
	showStructMembers (me, my address, my description, my topField, my nameW);
}

class_methods (StructEditor, DataSubEditor)
	class_method_local (StructEditor, countFields)
	class_method_local (StructEditor, showMembers)
class_methods_end

static int StructEditor_init (I, DataEditor root, const wchar_t *title, void *address, Data_Description description) {
	iam (StructEditor);
	if (! DataSubEditor_init (me, root, title, address, description))
		return 0;
	return 1;
}

static StructEditor StructEditor_create (DataEditor root, const wchar_t *title, void *address, Data_Description description) {
	StructEditor me = new (StructEditor);
	if (! me || ! StructEditor_init (me, root, title, address, description)) return NULL;
	return me;
}

/********** VectorEditor **********/

static long classVectorEditor_countFields (I) {
	iam (VectorEditor);
	long numberOfElements = my maximum - my minimum + 1;
	if (my description -> type == structwa)
		return numberOfElements * (Data_Description_countMembers (my description -> tagType) + 1);
	else
		return numberOfElements;
}

static void classVectorEditor_showMembers (I) {
	iam (VectorEditor);
	long firstElement, ielement;
	int type = my description -> type, isSingleType = type <= maxsingletypewa;
	int elementSize = type == structwa ?
		Data_Description_countMembers (my description -> tagType) + 1 : 1;
	firstElement = my minimum + (my topField - 1) / elementSize;

	for (ielement = firstElement; ielement <= my maximum; ielement ++) {
		unsigned char *elementAddress = (unsigned char *) my address + ielement * my description -> size;
		static MelderStringW buffer = { 0 };
		MelderStringW_empty (& buffer);
		DataSubEditor_FieldData fieldData;
		int skip = ielement == firstElement ? (my topField - 1) % elementSize : 0;

		if (++ my irow > MAXNUM_ROWS) return;
		fieldData = & my fieldData [my irow];

		if (isSingleType) {
			MelderStringW_append4 (& buffer, my description -> name, L" [",
				my description -> rank == 3 ? enum_string (my description -> max1, ielement) : Melder_integer (ielement), L"]");
			XtVaSetValues (fieldData -> label, motif_argXmString (XmNlabelString, Melder_peekWcsToUtf8 (buffer.string)), XmNx, 0, NULL);
			XtManageChild (fieldData -> label);

			MelderStringW_empty (& buffer);
			wchar_t *text = singleTypeToText (elementAddress, type, my description -> tagType, & buffer);
			XtVaSetValues (fieldData -> text, XmNcolumns, stringLengths [type], NULL);
			GuiText_setStringW (fieldData -> text, text);
			XtManageChild (fieldData -> text);
			fieldData -> address = elementAddress;
			fieldData -> description = my description;
		} else if (type == structwa) {
			static MelderStringW history = { 0 };
			MelderStringW_copyW (& history, my nameW);

			/* Replace things like [1..100] by things like [19]. */

			if (history.string [history.length - 1] == ']') {
				wchar_t *openingBracket = wcsrchr (history.string, '[');
				Melder_assert (openingBracket != NULL);
				* openingBracket = '\0';
				history.length = openingBracket - history.string;
			}
			MelderStringW_append3 (& history, L"[", Melder_integer (ielement), L"]");

			if (skip) {
				my irow --;
			} else {
				MelderStringW_append4 (& buffer, my description -> name, L" [", Melder_integer (ielement), L"]: ---------------------------");
				XtVaSetValues (fieldData -> label, motif_argXmString (XmNlabelString, Melder_peekWcsToUtf8 (buffer.string)), XmNx, 0, NULL);
				XtManageChild (fieldData -> label);
			}
			showStructMembers (me, elementAddress, my description -> tagType, skip, history.string);
		} else if (type == objectwa) {
			static MelderStringW history = { 0 };
			MelderStringW_copyW (& history, my nameW);
			if (history.string [history.length - 1] == ']') {
				wchar_t *openingBracket = wcsrchr (history.string, '[');
				Melder_assert (openingBracket != NULL);
				* openingBracket = '\0';
				history.length = openingBracket - history.string;
			}
			MelderStringW_append3 (& history, L"[", Melder_integer (ielement), L"]");

			MelderStringW_append4 (& buffer, my description -> name, L" [", Melder_integer (ielement), L"]");
			XtVaSetValues (fieldData -> label, motif_argXmString (XmNlabelString, Melder_peekWcsToUtf8 (buffer.string)), XmNx, 0, NULL);
			XtManageChild (fieldData -> label);

			Data object = * (Data *) elementAddress;
			if (object == NULL) return;   /* No button if no object. */
			if (! object -> methods -> description) return;   /* No button if no description for this class. */
			fieldData -> address = object;
			fieldData -> description = object -> methods -> description;
			fieldData -> rank = 0;
			if (fieldData -> history) Melder_free (fieldData -> history);
			fieldData -> history = Melder_wcsdup (history.string);
			XtManageChild (fieldData -> button);			
		}
	}
}

class_methods (VectorEditor, DataSubEditor)
	class_method_local (VectorEditor, countFields)
	class_method_local (VectorEditor, showMembers)
class_methods_end

static VectorEditor VectorEditor_create (DataEditor root, const wchar_t *title, void *address,
	Data_Description description, long minimum, long maximum)
{
	VectorEditor me = new (VectorEditor);
	my minimum = minimum;
	my maximum = maximum;
	if (! DataSubEditor_init (me, root, title, address, description))
		{ forget (me); return 0; }
	return me;
}

/********** MatrixEditor **********/

static long classMatrixEditor_countFields (I) {
	iam (MatrixEditor);
	long numberOfElements = (my maximum - my minimum + 1) * (my max2 - my min2 + 1);
	if (my description -> type == structwa)
		return numberOfElements * (Data_Description_countMembers (my description -> tagType) + 1);
	else
		return numberOfElements;
}

static void classMatrixEditor_showMembers (I) {
	iam (MatrixEditor);
	long firstRow, firstColumn;
	int type = my description -> type, isSingleType = type <= maxsingletypewa;
	int elementSize = type == structwa ?
		Data_Description_countMembers (my description -> tagType) + 1 : 1;
	int rowSize = elementSize * (my max2 - my min2 + 1);
	firstRow = my minimum + (my topField - 1) / rowSize;
	firstColumn = my min2 + (my topField - 1 - (firstRow - my minimum) * rowSize) / elementSize;

	for (long irow = firstRow; irow <= my maximum; irow ++)
	for (long icolumn = irow == firstRow ? firstColumn : my min2; icolumn <= my max2; icolumn ++) {
		unsigned char *elementAddress = * ((unsigned char **) my address + irow) + icolumn * my description -> size;
		DataSubEditor_FieldData fieldData;

		if (++ my irow > MAXNUM_ROWS) return;
		fieldData = & my fieldData [my irow];
		
		if (isSingleType) {
			static MelderStringW buffer = { 0 };
			MelderStringW_empty (& buffer);
			MelderStringW_append6 (& buffer, my description -> name, L" [", Melder_integer (irow), L"] [", Melder_integer (icolumn), L"]");
			XtVaSetValues (fieldData -> label, motif_argXmString (XmNlabelString, Melder_peekWcsToUtf8 (buffer.string)), XmNx, 0, NULL);
			XtManageChild (fieldData -> label);

			MelderStringW_empty (& buffer);
			wchar_t *text = singleTypeToText (elementAddress, type, my description -> tagType, & buffer);
			XtVaSetValues (fieldData -> text, XmNcolumns, stringLengths [type], NULL);
			GuiText_setStringW (fieldData -> text, text);
			XtManageChild (fieldData -> text);
			fieldData -> address = elementAddress;
			fieldData -> description = my description;
		}
	}
}

class_methods (MatrixEditor, DataSubEditor)
	class_method_local (MatrixEditor, countFields)
	class_method_local (MatrixEditor, showMembers)
class_methods_end

static MatrixEditor MatrixEditor_create (DataEditor root, const wchar_t *title, void *address,
	Data_Description description, long min1, long max1, long min2, long max2)
{
	MatrixEditor me = new (MatrixEditor);
	my minimum = min1;
	my maximum = max1;
	my min2 = min2;
	my max2 = max2;
	if (! DataSubEditor_init (me, root, title, address, description))
		{ forget (me); return 0; }
	return me;
}

/********** ClassEditor **********/

static void ClassEditor_showMembers_recursive (ClassEditor me, void *klas) {
	Data_Table parentClass = (Data_Table) ((Data_Table) klas) -> _parent;
	Data_Description description = ((Data_Table) klas) -> description;
	int classFieldsTraversed = 0;
	while (parentClass -> description == description)
		parentClass = (Data_Table) parentClass -> _parent;
	if (parentClass != classData) {
		ClassEditor_showMembers_recursive (me, parentClass);
		classFieldsTraversed = Data_Description_countMembers (parentClass -> description);
	}
	showStructMembers (me, my address, description, my irow ? 1 : my topField - classFieldsTraversed, my nameW);
}

static void classClassEditor_showMembers (I) {
	iam (ClassEditor);
	ClassEditor_showMembers_recursive (me, ((Data) my address) -> methods);
}

class_methods (ClassEditor, StructEditor)
	class_method_local (ClassEditor, showMembers)
class_methods_end

static int ClassEditor_init (I, DataEditor root, const wchar_t *title, void *address, Data_Description description) {
	iam (ClassEditor);
	if (! description) return Melder_error
		("(ClassEditor_init:) Class \"%s\" cannot be inspected.", Thing_className (address));
	if (! StructEditor_init (me, root, title, address, description))
		return 0;
	return 1;
}

static ClassEditor ClassEditor_create (DataEditor root, const wchar_t *title, void *address, Data_Description description) {
	ClassEditor me = new (ClassEditor);
	if (! me || ! ClassEditor_init (me, root, title, address, description)) { forget (me); return NULL; }
	return me;
}

/********** DataEditor **********/

static void classDataEditor_destroy (I) {
	iam (DataEditor);
	int i;

	/* Tell my children not to notify me when they die. */

	for (i = 1; i <= my children -> size; i ++) {
		DataSubEditor child = my children -> item [i];
		child -> root = NULL;
	}

	forget (my children);
	inherited (DataEditor) destroy (me);
}

static void classDataEditor_dataChanged (I) {
	iam (DataEditor);
	int i;
	/*
	 * Someone else changed our data.
	 * We know that the top-level data is still accessible.
	 */
	update ((DataSubEditor) me);
	/*
	 * But all structure may have changed,
	 * so that we do not know if any of the subeditors contain valid data.
	 */
	for (i = my children -> size; i >= 1; i --) {
		DataSubEditor subeditor = my children -> item [i];
		Collection_subtractItem (my children, i);
		forget (subeditor);
	}
}

class_methods (DataEditor, ClassEditor)
	class_method_local (DataEditor, destroy)
	class_method_local (DataEditor, dataChanged)
class_methods_end

DataEditor DataEditor_create (Widget parent, const wchar_t *title, Any data) {
	DataEditor me;
	Data_Table klas = ((Data) data) -> methods;
	if (! klas -> description) return Melder_errorp
		("(DataEditor_create:) Class \"%s\" cannot be inspected.", klas -> _className);
	me = new (DataEditor);
	my children = Collection_create (classDataSubEditor, 10);
	my parent = parent;
	if (! me || ! ClassEditor_init (me, me, title, data, klas -> description))
		{ forget (me); return NULL; }
	return me;
}

/* End of file DataEditor.c */
