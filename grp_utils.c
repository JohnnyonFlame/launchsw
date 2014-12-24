#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "grp_utils.h"
#define KENSILVERMAN "KenSilverman"

void GRP_Free(GRP *grp)
{
	if (!grp)
	{
		printf("GRP_Free: Warning: grp is a NULL pointer!\n");
		return;
	}

	if (grp->files)
		free(grp->files);

	free(grp);
}

GRP *GRP_FromFile(const char *path)
{
	GRP *grp;
	FILE *file = fopen(path, "rb");
	if (!file)
	{
		printf("GRP_FromFile: Unable to open %s!\n", path);
		return NULL;
	}

	grp = GRP_FromFilePointer(file);
	fclose(file);

	return grp;
}

GRP *GRP_FromFilePointer(FILE *file)
{
	GRP *grp = NULL;
	char magicBits[12];

	if (!file)
	{
		printf("GRP_BuildTable: NULL file passed!\n");
		return NULL;
	}

	if (fread(magicBits, 12, 1, file) < 1)
	{
		printf("GRP_BuildTable: Unexpected file end (no KenSilverman string)!\n");
		return NULL;
	}

	if (strncmp(magicBits, KENSILVERMAN, 12))
	{
		printf("GRP_BuildTable: File validation failed!\n");
		return NULL;
	}

	grp = calloc(1, sizeof(GRP));
	if (!grp)
	{
		printf("GRP_BuildTable: Failed to allocate GRP file!\n");
		return NULL;
	}

	if (fread(&grp->filecount, 4, 1, file) < 1)
	{
		free(grp);
		printf("GRP_BuildTable: Unexpected file end (no file count)!\n");
		return NULL;
	}

	grp->files = calloc(grp->filecount, sizeof(GRP_FILE_INDEX));
	if (!grp->files)
	{
		free(grp);
		printf("GRP_BuildTable: Unable to allocate enough indices for all files!\n");
		return NULL;
	}

	int i;
	for (i=0; i<grp->filecount; i++)
	{
		if ((fread(grp->files[i].name, 12, 1, file) < 1) ||
				(fread(&grp->files[i].size, 4, 1, file) < 1))
		{
			free(grp->files);
			free(grp);
			printf("GRP_BuildTable: Unexpected file end (file count too high)!\n");
			return NULL;
		}
	}

	uint32_t ptr = ftell(file);
	for (i=0; i<grp->filecount; i++)
	{
		grp->files[i].start = ptr;
		ptr += grp->files[i].size;
	}

	return grp;
}

int GRP_CountFilesByEXT(GRP *grp, char *ext)
{
	int i, ret = 0;
	for (i=0; i<grp->filecount; i++)
	{
		char *file_ext = strrchr(grp->files[i].name, '.');
		if (!file_ext)
			continue; /* no extension */

		if (!strcmp(ext, file_ext+1))
			ret++;
	}

	return ret;
}

GRP *GRP_FilterByEXT(GRP *grp, char *ext, int auto_free)
{
	GRP *ret = calloc(1, sizeof(GRP));

	if (!ret)
	{
		printf("GRP_FilterByEXT: Unable to allocate return value.\n");
		return NULL;
	}

	if (!(ret->filecount = GRP_CountFilesByEXT(grp, ext)))
	{
		return ret;
	}

	ret->files = calloc(ret->filecount, sizeof(GRP_FILE_INDEX));
	if (!ret->files)
	{
		free(ret);
		return NULL;
	}

	int i, j = 0;
	for (i=0; i<grp->filecount; i++)
	{
		char *file_ext = strrchr(grp->files[i].name, '.');
		if (!file_ext)
			continue; /* no extension */

		if (!strcmp(ext, file_ext+1))
		{
			if (j >= ret->filecount)
			{
				printf("GRP_FilterByEXT: Unexpected filecount.\n");
				break;
			}

			memcpy(&ret->files[j++], &grp->files[i], sizeof(GRP_FILE_INDEX));
		}
	}

	if (auto_free)
		GRP_Free(grp);

	return ret;
}

int GRP_Validate(const char *path)
{
	int ret = 0;
	char magicBits[12];
	FILE *file = fopen(path, "rb");

	if (!file)
		return 0;

	if (fread(magicBits, 12, 1, file) &&
	     !strncmp(magicBits, KENSILVERMAN, 12))
		ret = 1;

	fclose(file);
	return ret;
}
