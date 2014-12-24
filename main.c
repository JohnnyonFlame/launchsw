#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "textscreen.h"
#include "grp_utils.h"

#ifdef __WIN32
#define LAUNCH_TARGET "sw.exe"
#else
#define MAX_PATH PATH_MAX
#define LAUNCH_TARGET "./sw"
#endif
#define HOME_DIR ".jfsw"
#define MAP_DIR "maps"
#define ADDON_DIR "addons"
#define MAIN_GRP "sw.grp"
char homedir_s[MAX_PATH];
char mapdir_s[MAX_PATH];
char addondir_s[MAX_PATH];

#define ADDCMDLINE_COUNT 4
#define ADDCMDLINE_STRLEN 32

#ifdef __WIN32
char *realpath(const char *path, char *resolved_path);
#endif

/*
 * Main screen
 */
static txt_window_t        	*window;
static txt_dropdown_list_t 	*droplist_map;
static txt_table_t 			*grp_selector;

/*
 * User Map
 */
static char *userMap_dir_s = NULL;
static char *userMap_s = NULL;

/*
 * Skill Selector
 */
static int   skill_v = 2;
static char *skill_s[] =
{
	"Tiny grasshopper",
	"I Have No Fear",
	"Who Wants Wang",
	"No Pain, No Gain"
};

/*
 * No Monsters
 */
static int noMonsters_v = 0;

/*
 * No Meters:
 * Don't show air or boss meter bars in game
 */
static int noMeters_v = 0;

/*
 * Additional Command-lines
 */
static char **additionalCmdLines;

/*
 * Custom GRP Addon Selector
 */
static int    addonGrp_c = 0;
static int 	  addonGrp_v = -1;
static char **addonGrp_s = NULL;

/*
 * Final command to execute.
 */
static char *cmdLine = NULL;

static __inline__ char p_separator()
{
#ifdef __WIN32
	return '\\';
#else
	return '/';
#endif
}

void usermap_dialog();
void launch_finish(const char *map);

static void exit_callback(TXT_UNCAST_ARG(widget), void *user_data)
{
	TXT_ExitMainLoop();
}

static void error_screen(char *msg)
{
	txt_label_t *label = TXT_NewLabel(msg);
	txt_window_t *dialog = TXT_NewWindow("ERROR:\n");
	txt_window_action_t *act = TXT_NewWindowAction(KEY_BBUTTON, "Exit");
	if (!label || !dialog)
	{
		printf("error_screen: error displaying error!\n");
		exit(-1);
	}

	TXT_SetWidgetAlign(label, TXT_HORIZ_CENTER);
	TXT_AddWidget(dialog, label);

	TXT_SignalConnect(act, "pressed", exit_callback, NULL);
	TXT_SetWindowAction(dialog, TXT_HORIZ_RIGHT, NULL);
	TXT_SetWindowAction(dialog, TXT_HORIZ_CENTER, act);
	TXT_SetWindowAction(dialog, TXT_HORIZ_LEFT, NULL);
}

static inline rebuildMapList_bailOut()
{
	droplist_map->num_values = 1;
	droplist_map->values = calloc(1, sizeof(char**));
	droplist_map->values[0] = strdup("?");
}

static void rebuild_maplist()
{
	char path[MAX_PATH];
	GRP *maps, *grp;
	uint32_t size = 0, size_total = 0;
	int i, j;

	/*
	 * Free current map list
	 */
	if (droplist_map->values)
	{
		for (i=0; i<droplist_map->num_values; i++)
		{
			if (droplist_map->values[i])
				free(droplist_map->values[i]);
		}

		free(droplist_map->values);
	}

	/*
	 * Find current main GRP
	 */

	grp = GRP_FromFile(MAIN_GRP);
	if (!grp)
	{
		snprintf(path, MAX_PATH, "%s%c%s", homedir_s, p_separator(), MAIN_GRP);
		grp = GRP_FromFile(path);
	}

	if (!grp || grp->filecount == 0)
	{
		if (grp)
		{
			error_screen("Invalid or corrupt sw.grp file!\nPlease copy a valid sw.grp from a shadow warrior install into '$home/.jfsw/'.");
			GRP_Free(grp);
		}
		else
			error_screen("Unable to open sw.grp!\nPlease copy your 'sw.grp' into the '$home/.jfsw/' folder\nand make sure the file name is lowercase.");

		rebuildMapList_bailOut();
		return;
	}

	/*
	 * Get a list with only the MAPs
	 */

	maps = GRP_FilterByEXT(grp, "MAP", 1);
	if (!maps || !maps->filecount)
	{
		if (maps)
			GRP_Free(maps);

		error_screen("Invalid sw.grp file!\nPlease copy a valid sw.grp from a shadow warrior install into '$home/.jfsw/'.");

		rebuildMapList_bailOut();
		return;
	}

	droplist_map->num_values = maps->filecount + 2;
	droplist_map->values = calloc(droplist_map->num_values, sizeof(char**));

	if (!droplist_map->values)
	{
		printf("rebuild_maplist: Unable to allocate map list!\n");
		exit(-1);
	}

	/*
	 * Non-main GRP maps or No Warp
	 */

	droplist_map->values[0] = strdup("No Warp");
	droplist_map->values[1] = strdup("User Map");

	for (i=0; i < maps->filecount; i++)
	{
		droplist_map->values[i+2] = strdup(maps->files[i].name);
		if (!droplist_map->values[i+2])
		{
			printf("rebuild_maplist: Unable to allocate map list index!\n");
			exit(-1);
		}
	}

	GRP_Free(maps);

	if (addonGrp_v != -1)
	{
		GRP *grp = GRP_FromFile(addonGrp_s[addonGrp_v]);

		if (!grp)
		{
			printf("rebuild_maplist: Warning, invalid addon grp!\n");
			return;
		}

		maps = GRP_FilterByEXT(grp, "MAP", 1);

		if (!maps || (maps->filecount == 0))
		{
			printf("rebuild_maplist: Warning, no maps in addon grp!\n");
			return;
		}

		for (i=0; i < maps->filecount; i++)
		{
			/*
			 * If map was already listed by the main GRP we don't need to list it twice.
			 * File priority and all that jazz :)
			 */

			int was_eq = 0;
			for (j=0; j<droplist_map->num_values; j++)
			{
				if (!strcasecmp(droplist_map->values[j], maps->files[i].name))
					was_eq = 1;
			}

			if (was_eq)
				continue;

			/*
			 * Since the file wasn't on the main GRP, we'd better add it to the list.
			 */

			droplist_map->values = realloc(droplist_map->values,
					sizeof(char**) * (droplist_map->num_values+1));

			if (!droplist_map->values)
			{
				printf("rebuild_maplist: Unable to allocate map list!\n");
				exit(-1);
			}

			droplist_map->values[droplist_map->num_values] =
					strdup(maps->files[i].name);

			if (!droplist_map->values[droplist_map->num_values])
			{
				printf("rebuild_maplist: Unable to allocate map list index!\n");
				exit(-1);
			}

			droplist_map->num_values++;
		}
	}

	/*
	 * Check if the var is outside of the bounds, and if so, reset.
	 */

	if (*droplist_map->variable >= droplist_map->num_values)
		*droplist_map->variable = 0;
}

void update_addonGrp(TXT_UNCAST_ARG(widget), void *user_data)
{
	rebuild_maplist();
}

#define ADDONGRP_ADDBUTTON(v, str) \
		radio = TXT_NewRadioButton((str), &addonGrp_v, (v)); \
		TXT_AddWidget(grp_selector, radio); \
		TXT_SignalConnect(radio, "selected", update_addonGrp, NULL);

static void build_addonGrpList()
{
    txt_radiobutton_t *radio;
	DIR *dir = opendir(addondir_s);
	struct dirent *ent;
	struct stat st;

	grp_selector = TXT_NewTable(1);
	ADDONGRP_ADDBUTTON(-1, "No addon");

	while (dir && (ent = readdir(dir)))
	{
		char file_buf[2048];
		snprintf(file_buf, 2048, "%s%c%s", addondir_s, p_separator(), ent->d_name);

		stat(file_buf, &st);
		if (S_ISREG(st.st_mode))
		{
			if (GRP_Validate(file_buf))
			{
				char name_buf[32];
				if (strlen(ent->d_name) >= 32)
					snprintf(name_buf, 32, "%.28s...", ent->d_name);
				else
					snprintf(name_buf, 32, "%s", ent->d_name);

				addonGrp_s = realloc(addonGrp_s, sizeof(char**) * (addonGrp_c+1));
				addonGrp_s[addonGrp_c] = strdup(file_buf);
				ADDONGRP_ADDBUTTON(addonGrp_c, name_buf);
				addonGrp_c++;
			}
		}
	}

	TXT_AddWidget(window, TXT_NewScrollPane(0, (addonGrp_c+1 > 5) ? 5 : addonGrp_c+1, grp_selector));
}

void usermap_open_callback(TXT_UNCAST_ARG(widget), TXT_UNCAST_ARG(user_data))
{
	char path[MAX_PATH];
	struct stat st;
	TXT_CAST_ARG(txt_window_t, user_data);
	TXT_CAST_ARG(txt_button_t, widget);
	
	snprintf(path, MAX_PATH, "%s%c%s", userMap_dir_s, p_separator(), widget->label);
	TXT_CloseWindow(user_data);
	
	if (path[strlen(path) - 1] == p_separator())
		path[strlen(path) - 1] = '\0';
	
	stat(path, &st);
	if (S_ISDIR(st.st_mode))
	{
		if (userMap_dir_s)
			free(userMap_dir_s);

		userMap_dir_s = strdup(path);
		usermap_dialog();
	}
	else if (S_ISREG(st.st_mode))
	{
		if (userMap_s)
			free(userMap_s);

		userMap_s = strdup(path);
	}
}

void usermap_close_callback(TXT_UNCAST_ARG(widget), TXT_UNCAST_ARG(user_data))
{
	TXT_CAST_ARG(txt_window_t, user_data);

	/*
	 * Canceled. Fall back to "no warp"
	 */
	*droplist_map->variable = 0;
	
	TXT_CloseWindow(user_data);
}

void usermap_dialog()
{
	txt_window_action_t *act_close;
	txt_window_t *dialog;
	txt_table_t *table;
	txt_scrollpane_t *pane;
	DIR *dir;
	struct dirent *ent;
	struct stat st;
	
	/*
	 * First time on the map selector? Allocate path.
	 */

	if (!userMap_dir_s)
		userMap_dir_s = strdup(mapdir_s);

	if (!userMap_dir_s)
	{
		printf("usermap_dialog: Unable to allocate string to hold directory name!\n");
		exit(-1);
	}

	/*
	 * Let's get the real searchpath
	 */

	char *tmp = realpath(userMap_dir_s, NULL);
	if (!tmp)
	{
		printf("usermap_dialog: Unable to allocate string to hold directory name!\n");
		exit(-1);
	}

	free(userMap_dir_s);
	userMap_dir_s = tmp;

	dialog = TXT_NewWindow("Select User Map"); /* 15 */
	table  = TXT_NewTable(1);
	if (!dialog || !table)
	{
		printf("usermap_dialog: Unable to create dialog resources!\n");
		exit(-1);
	}
	
	/*
	 * Either add it fully, or truncate if path too big.
	 */
	if (strlen(userMap_dir_s) > 45)
	{
		char buf[45], *start;

		//Find null-terminator and walk backwards,
		start = strrchr(userMap_dir_s, '\0');
		start -= 41;

		//Print format it
		snprintf(buf, 45, "...%s", start);
		TXT_AddWidget(dialog, TXT_NewSeparator(buf));
	}
	else
		TXT_AddWidget(dialog, TXT_NewSeparator(userMap_dir_s));

	/*
	 * Let's populate our file browser, we'll do it folder-first
	 */
		
	dir = opendir(userMap_dir_s);
	while (dir && (ent = readdir(dir)))
	{
		char file_buf[MAX_PATH];
		if (!strcmp(ent->d_name, "."))
			continue;
			
		snprintf(file_buf, MAX_PATH, "%s%c%s", userMap_dir_s, p_separator(), ent->d_name);
		stat(file_buf, &st);
			
		if (!S_ISDIR(st.st_mode))
			continue;
		
		if (!strcmp("..", ent->d_name))
			snprintf(file_buf, MAX_PATH, "%s", ent->d_name);
		else
			snprintf(file_buf, MAX_PATH, "%s%c", ent->d_name, p_separator());
			
		TXT_AddWidget(table,
			TXT_NewButton2(file_buf, usermap_open_callback, dialog));
	}
	
	/*
	 * Now the files!
	 */
	
	rewinddir(dir);
	while (dir && (ent = readdir(dir)))
	{
		char file_buf[MAX_PATH];
		if (!strcmp(ent->d_name, "."))
			continue;
			
		snprintf(file_buf, MAX_PATH, "%s%c%s", userMap_dir_s, p_separator(), ent->d_name);
		stat(file_buf, &st);
			
		/* is it a regular file? */
		if (!S_ISREG(st.st_mode))
			continue;

		/* we are going to filter the files, so open it */
		FILE *f = fopen(file_buf, "rb");
		int magicBytes = 0;

		/* can we open the file? */
		if (!f)
			continue;

		/* can we read four bytes from it? */
		if (!fread(&magicBytes, 4, 1, f))
		{
			fclose(f);
			continue;
		}

		/* do they make a 7? */
		if (magicBytes != 7)
		{
			fclose(f);
			continue;
		}
			
		TXT_AddWidget(table, 
			TXT_NewButton2(ent->d_name, usermap_open_callback, dialog));
	}
	
	TXT_AddWidget(dialog, TXT_NewScrollPane(46, 10, table));

	act_close = TXT_NewWindowAction(KEY_BBUTTON, "Cancel");
	TXT_SignalConnect(act_close, "pressed", usermap_close_callback, dialog);

	TXT_SetWindowAction(dialog, TXT_HORIZ_LEFT, act_close);
}


static void warp_callback(TXT_UNCAST_ARG(widget), void *user_data)
{
	if (*droplist_map->variable == 1)
		usermap_dialog();
}

void launch_callback(TXT_UNCAST_ARG(widget), void *user_data)
{
	char args[4096] = "";

	snprintf(&args[strlen(args)], 4096-strlen(args), "%s", LAUNCH_TARGET);

	if (addonGrp_v >= 0) /* did you select an addon grp? */
		snprintf(&args[strlen(args)], 4096-strlen(args), " -g%s", addonGrp_s[addonGrp_v]);

	if (*droplist_map->variable) /* are we warping? */
	{
		snprintf(&args[strlen(args)], 4096-strlen(args), " -s%i", skill_v+1);
		if (userMap_s) /* is it an usermap? */
		{
			snprintf(&args[strlen(args)], 4096-strlen(args), " -map \"%s\"", userMap_s);
		}
		else
		{
			snprintf(&args[strlen(args)], 4096-strlen(args), " -map %s",
					droplist_map->values[*droplist_map->variable]);
		}
	}

	if (noMonsters_v)
		snprintf(&args[strlen(args)], 4096-strlen(args), " -monst");

	if (noMeters_v)
		snprintf(&args[strlen(args)], 4096-strlen(args), " -nometers");

	snprintf(&args[strlen(args)], 4096-strlen(args), " ");

	int i;
	for (i=0; i<ADDCMDLINE_COUNT; i++)
	{
		snprintf(&args[strlen(args)], 4096-strlen(args), "%s", additionalCmdLines[i]);
	}

	cmdLine = strdup(args);
	TXT_ExitMainLoop();
}

int main(int argc, char *argv[])
{
	if (!TXT_Init())
	{
		fprintf(stderr, "Failed to initialise GUI\n");
		return -1;
	}
	
	SDL_ShowCursor(0);
	window = TXT_NewWindow("Shadow Launcher");

	txt_table_t *table = TXT_NewTable(2);
	txt_window_action_t *launch_act, *map_act;

	/*
	 * Create dirs if they're missing
	 */
	snprintf(homedir_s, MAX_PATH, "%s%c%s", getenv("HOME"), p_separator(), HOME_DIR);
	snprintf(mapdir_s, MAX_PATH, "%s%c%s", homedir_s, p_separator(), MAP_DIR);
	snprintf(addondir_s, MAX_PATH, "%s%c%s", homedir_s, p_separator(), ADDON_DIR);

	mkdir(homedir_s, 0755);
	mkdir(mapdir_s, 0755);
	mkdir(addondir_s, 0755);

	/*
	 * Warp Settings
	 */
	droplist_map = TXT_NewDropdownList(calloc(1, sizeof(int)), NULL, 0);
	rebuild_maplist();

	TXT_AddWidget(window, table);
	TXT_AddWidgets(table, TXT_NewLabel("Warp: "), droplist_map, NULL);
	TXT_SignalConnect(droplist_map, "changed", warp_callback, NULL);

	/*
	 * Skill Settings
	 */
	TXT_AddWidgets(table, TXT_NewLabel("Skill: "), TXT_NewDropdownList(&skill_v, skill_s, 4), NULL);

	/*
	 * Misc Settings
	 */
	TXT_AddWidgets(window, TXT_NewSeparator("Misc"), NULL);
	TXT_AddWidgets(window, TXT_NewCheckBox("No Monsters", &noMonsters_v), NULL);
	TXT_AddWidgets(window, TXT_NewCheckBox("No Meters", &noMeters_v), NULL);

	/*
	 * Additional Command-lines
	 */
	additionalCmdLines = (char**)calloc(ADDCMDLINE_COUNT, sizeof(char*));
	TXT_AddWidgets(window, TXT_NewSeparator("Additional Command-lines"), NULL);
	int i;
	for (i=0; i<ADDCMDLINE_COUNT; i++)
	{
		additionalCmdLines[i] = calloc(ADDCMDLINE_STRLEN, sizeof(char));
		TXT_AddWidgets(window, TXT_NewInputBox(&additionalCmdLines[i], ADDCMDLINE_STRLEN), NULL);
	}

	/*
	 * Addon GRP Selector
	 */
	TXT_AddWidget(window, TXT_NewSeparator("Active Addon GRP"));
	build_addonGrpList();

	/*
	 * Add the 'launch' action to the main window
	 */
	launch_act = TXT_NewWindowAction(KEY_YBUTTON, "Launch");
	TXT_SignalConnect(launch_act, "pressed", launch_callback, NULL);
	TXT_SetWindowAction(window, TXT_HORIZ_CENTER, launch_act);

	/*
	 * We're ready, start the GUI
	 */
	TXT_GUIMainLoop();

	/*
	 * All done, destroy the GUI and SDL, and if we asked, launch game.
	 */
	TXT_Shutdown();
	if (cmdLine)
	{
		system(cmdLine);
		printf("Running: %s\n", cmdLine);
	}

	return 0;
}
