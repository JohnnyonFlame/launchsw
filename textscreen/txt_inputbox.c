// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// Copyright(C) 2006 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doomkeys.h"

#include "txt_inputbox.h"
#include "txt_label.h"
#include "txt_button.h"
#include "txt_gui.h"
#include "txt_io.h"
#include "txt_main.h"
#include "txt_utf8.h"
#include "txt_window.h"

#ifdef __NO_KEYBOARD
#include "txt_inputbox_vkb.h"

static txt_window_t *window;
static txt_inputbox_t *vkb_inputbox;
static txt_label_t *vkb_label;

static char **p_charset = &lowercase_charset;
static int charset_no = 0;
#endif

extern txt_widget_class_t txt_inputbox_class;
extern txt_widget_class_t txt_int_inputbox_class;

#ifdef __NO_KEYBOARD
static void TraverseCharsets(TXT_UNCAST_ARG(widget), void *user_data)
{
	TXT_CAST_ARG(txt_inputbox_t, widget);
	charset_no++;

	switch (charset_no)
	{
	default:
	case SET_LOWERCASE:
		p_charset = (char**)&lowercase_charset;
		if (charset_no != SET_LOWERCASE)
			charset_no = SET_LOWERCASE;
		break;
	case SET_UPPERCASE:
		p_charset  = (char**)&uppercase_charset;
		break;
	case SET_SPECIAL:
		p_charset  = (char**)&special_charset;
		break;
	}

	if (window)
		TXT_CloseWindow(window);

	BuildVKB(vkb_inputbox);
}

static void VKB_AddCharacter(TXT_UNCAST_ARG(widget), void *user_data)
{
	AddCharacter(vkb_inputbox, *(char*)user_data);
	TXT_SetLabel(vkb_label, vkb_inputbox->buffer);
}

static void VKB_Cancel(void)
{
	vkb_inputbox->editing = 0;
	TXT_CloseWindow(window);
}

static void VKB_Ok(void)
{
	TXT_CloseWindow(window);
	FinishEditing(vkb_inputbox);
}

static void VKB_Backspace(void)
{
	Backspace(vkb_inputbox);
	TXT_SetLabel(vkb_label, vkb_inputbox->buffer);
}

static void BuildVKB(TXT_UNCAST_ARG(inputbox))
{
	TXT_CAST_ARG(txt_inputbox_t, inputbox);

	txt_button_t *button, *space, *cancel, *ok;
	txt_table_t  *vkb, *cmd;
	txt_window_action_t *act;

	vkb_inputbox = inputbox;
	window = TXT_NewWindow("Virtual Keyboard");

	vkb    = TXT_NewTable(10);
	cmd    = TXT_NewTable(4);

	int i=0;
	char *cur_row, *cur_char;
	for (cur_row = p_charset [i++]; cur_row != NULL; cur_row = p_charset [i++])
	{
		for (cur_char = cur_row; *cur_char != '\0'; cur_char++)
		{
			//printf("%c", *cur_char);

			char c[3] = {(i%2) ? ' ': *cur_char,
					(i%2) ? *cur_char : ' ', '\0'};

			TXT_AddWidget(vkb, TXT_NewButton2((char*)&c, VKB_AddCharacter, cur_char));
		}
	}

	vkb_label = TXT_NewLabel(inputbox->buffer);

	TXT_AddWidget(cmd, cancel = TXT_NewButton2("cancel", VKB_Cancel, NULL));
	TXT_AddWidget(cmd, space = TXT_NewButton2("  space", VKB_AddCharacter, " "));
	TXT_AddWidget(cmd, ok = TXT_NewButton2("     ok", VKB_Ok, NULL));

	TXT_SignalConnect(act = TXT_NewWindowAction(KEY_BBUTTON, "Cancel"), "pressed", VKB_Cancel, NULL);
	TXT_SetWindowAction(window, TXT_HORIZ_LEFT, act);

	TXT_SignalConnect(act = TXT_NewWindowAction(KEY_SPACE, "Caps"), "pressed", TraverseCharsets, NULL);
	TXT_SetWindowAction(window, TXT_HORIZ_CENTER, act);

	TXT_SignalConnect(act = TXT_NewWindowAction(KEY_RSHIFT, "Backspace"), "pressed", VKB_Backspace, NULL);
	TXT_SetWindowAction(window, TXT_HORIZ_RIGHT, act);

	TXT_SetWidgetAlign(space, TXT_HORIZ_CENTER);
	TXT_SetWidgetAlign(cmd,   TXT_HORIZ_CENTER);
	TXT_SetWidgetAlign(vkb, TXT_HORIZ_CENTER);
	TXT_SetBGColor(vkb_label, TXT_COLOR_BLACK);

	TXT_AddWidget(window, vkb_label);
	TXT_AddWidget(window, TXT_NewSeparator(NULL));
	TXT_AddWidget(window, vkb);
	TXT_AddWidget(window, cmd);
}
#endif

static void SetBufferFromValue(txt_inputbox_t *inputbox)
{
    if (inputbox->widget.widget_class == &txt_inputbox_class)
    {
        char **value = (char **) inputbox->value;

        if (*value != NULL)
        {
            strncpy(inputbox->buffer, *value, inputbox->size);
            inputbox->buffer[inputbox->size] = '\0';
        }
        else
        {
            strcpy(inputbox->buffer, "");
        }
    }
    else if (inputbox->widget.widget_class == &txt_int_inputbox_class)
    {
        int *value = (int *) inputbox->value;
        sprintf(inputbox->buffer, "%i", *value);
    }
}

static void StartEditing(txt_inputbox_t *inputbox)
{
    // Integer input boxes start from an empty buffer:

    if (inputbox->widget.widget_class == &txt_int_inputbox_class)
    {
        strcpy(inputbox->buffer, "");
    }
    else
    {
        SetBufferFromValue(inputbox);
    }

    inputbox->editing = 1;
}

static void FinishEditing(txt_inputbox_t *inputbox)
{
    if (!inputbox->editing)
    {
        return;
    }

    // Save the new value back to the variable.

    if (inputbox->widget.widget_class == &txt_inputbox_class)
    {
        free(*((char **)inputbox->value));
        *((char **) inputbox->value) = strdup(inputbox->buffer);
    }
    else if (inputbox->widget.widget_class == &txt_int_inputbox_class)
    {
        *((int *) inputbox->value) = atoi(inputbox->buffer);
    }

    TXT_EmitSignal(&inputbox->widget, "changed");

    inputbox->editing = 0;
}

static void TXT_InputBoxSizeCalc(TXT_UNCAST_ARG(inputbox))
{
    TXT_CAST_ARG(txt_inputbox_t, inputbox);

    // Enough space for the box + cursor

    inputbox->widget.w = inputbox->size + 1;
    inputbox->widget.h = 1;
}

static void TXT_InputBoxDrawer(TXT_UNCAST_ARG(inputbox))
{
    TXT_CAST_ARG(txt_inputbox_t, inputbox);
    int focused;
    int i;
    int chars;
    int w;

    focused = inputbox->widget.focused;
    w = inputbox->widget.w;

    // Select the background color based on whether we are currently
    // editing, and if not, whether the widget is focused.

    if (inputbox->editing && focused)
    {
        TXT_BGColor(TXT_COLOR_BLACK, 0);
    }
    else
    {
        TXT_SetWidgetBG(inputbox);
    }

    if (!inputbox->editing)
    {
        // If not editing, use the current value from inputbox->value.

        SetBufferFromValue(inputbox);
    }

    TXT_DrawUTF8String(inputbox->buffer);

    chars = TXT_UTF8_Strlen(inputbox->buffer);

    if (chars < w && inputbox->editing && focused)
    {
        TXT_BGColor(TXT_COLOR_BLACK, 1);
        TXT_DrawString("_");
        ++chars;
    }

    for (i=chars; i < w; ++i)
    {
        TXT_DrawString(" ");
    }
}

static void TXT_InputBoxDestructor(TXT_UNCAST_ARG(inputbox))
{
    TXT_CAST_ARG(txt_inputbox_t, inputbox);

    free(inputbox->buffer);
}

static void Backspace(txt_inputbox_t *inputbox)
{
    unsigned int len;
    char *p;

    len = TXT_UTF8_Strlen(inputbox->buffer);

    if (len > 0)
    {
        p = TXT_UTF8_SkipChars(inputbox->buffer, len - 1);
        *p = '\0';
    }
}

static void AddCharacter(txt_inputbox_t *inputbox, int key)
{
    char *end, *p;

    if (TXT_UTF8_Strlen(inputbox->buffer) < inputbox->size)
    {
        // Add character to the buffer

        end = inputbox->buffer + strlen(inputbox->buffer);
        p = TXT_EncodeUTF8(end, key);
        *p = '\0';
    }
}

static int TXT_InputBoxKeyPress(TXT_UNCAST_ARG(inputbox), int key)
{
    TXT_CAST_ARG(txt_inputbox_t, inputbox);
    unsigned int c;

    if (!inputbox->editing)
    {
        if (key == KEY_YBUTTON)
        {
        	TXT_EmitSignal(inputbox, "descript");
        	return 1;
        }
        if (key == KEY_ABUTTON)
        {
            StartEditing(inputbox);
#ifdef __NO_KEYBOARD
            BuildVKB(inputbox);
#endif
            return 1;
        }

        return 0;
    }

#ifndef __NO_KEYBOARD

    if (key == KEY_ENTER)
    {
        FinishEditing(inputbox);
    }

    if (key == KEY_ESCAPE)
    {
        inputbox->editing = 0;
    }

    if (key == KEY_BACKSPACE)
    {
        Backspace(inputbox);
    }

    c = TXT_KEY_TO_UNICODE(key);

    if (c >= 128 || isprint(c))
    {
        // Add character to the buffer

        AddCharacter(inputbox, c);
    }
#else
    if (key == KEY_BBUTTON)
    {
        inputbox->editing = 0;
    }
#endif
    return 1;
}

static void TXT_InputBoxMousePress(TXT_UNCAST_ARG(inputbox),
                                   int x, int y, int b)
{
    TXT_CAST_ARG(txt_inputbox_t, inputbox);

    if (b == TXT_MOUSE_LEFT)
    {
        // Make mouse clicks start editing the box

        if (!inputbox->editing)
        {
            // Send a simulated keypress to start editing

            TXT_WidgetKeyPress(inputbox, KEY_ENTER);
        }
    }
}

static void TXT_InputBoxFocused(TXT_UNCAST_ARG(inputbox), int focused)
{
    TXT_CAST_ARG(txt_inputbox_t, inputbox);

    // Stop editing when we lose focus.

#ifndef __NO_KEYBOARD
    if (inputbox->editing && !focused)
    {
        FinishEditing(inputbox);
    }
#endif
}

txt_widget_class_t txt_inputbox_class =
{
    TXT_AlwaysSelectable,
    TXT_InputBoxSizeCalc,
    TXT_InputBoxDrawer,
    TXT_InputBoxKeyPress,
    TXT_InputBoxDestructor,
    TXT_InputBoxMousePress,
    NULL,
    TXT_InputBoxFocused,
};

txt_widget_class_t txt_int_inputbox_class =
{
    TXT_AlwaysSelectable,
    TXT_InputBoxSizeCalc,
    TXT_InputBoxDrawer,
    TXT_InputBoxKeyPress,
    TXT_InputBoxDestructor,
    TXT_InputBoxMousePress,
    NULL,
    TXT_InputBoxFocused,
};

static txt_inputbox_t *NewInputBox(txt_widget_class_t *widget_class,
                                   void *value, int size)
{
    txt_inputbox_t *inputbox;

    inputbox = malloc(sizeof(txt_inputbox_t));

    TXT_InitWidget(inputbox, widget_class);
    inputbox->value = value;
    inputbox->size = size;
    // 'size' is the maximum number of characters that can be entered,
    // but for a UTF-8 string, each character can take up to four
    // characters.
    inputbox->buffer = malloc(size * 4 + 1);
    inputbox->editing = 0;

    return inputbox;
}

txt_inputbox_t *TXT_NewInputBox(char **value, int size)
{
    return NewInputBox(&txt_inputbox_class, value, size);
}

txt_inputbox_t *TXT_NewIntInputBox(int *value, int size)
{
    return NewInputBox(&txt_int_inputbox_class, value, size);
}

