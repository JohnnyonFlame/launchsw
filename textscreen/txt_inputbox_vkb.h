/*
 * vkb_def.h
 *
 *  Created on: Jul 6, 2013
 *      Author: johnny
 */

#ifndef VKB_DEF_H_
#define VKB_DEF_H_

typedef enum SET_ENUM
{
	SET_LOWERCASE= 0,
	SET_UPPERCASE= 1,
	SET_SPECIAL  = 2,
} SET_ENUM;

static char *lowercase_charset[] =
{
		"qwertyuiop",
		"asdfghjkl:",
		"\\zxcvbnm@!",

		NULL //TERMINATOR
};

static char *uppercase_charset[] =
{
		"QWERTYUIOP",
		"ASDFGHJKL;",
		"/ZXCVBNM<>",

		NULL //TERMINATOR
};

static char *special_charset[] =
{
	"1234567890",
	",.#$%_&*()",
	"[]{}^~-+=\"",

	NULL //TERMINATOR
};

#define MAX_ROWS 3

static void Backspace(txt_inputbox_t *inputbox);
static void FinishEditing(txt_inputbox_t *inputbox);
static void AddCharacter(txt_inputbox_t *inputbox, int key);
static void BuildVKB(TXT_UNCAST_ARG(inputbox));

#endif /* VKB_DEF_H_ */
