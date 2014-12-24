#ifndef GRP_UTILS_H_
#define GRP_UTILS_H_

typedef struct GRP_FILE_INDEX {
	uint32_t start;
	uint32_t size;
	char name[13]; /* actually 12 bytes, but we'll throw a null terminator there */
} GRP_FILE_INDEX;

typedef struct GRP {
	uint32_t filecount;
	GRP_FILE_INDEX *files;
} GRP;

void GRP_Free(GRP *grp);

GRP *GRP_FromFile(const char *path);
GRP *GRP_FromFilePointer(FILE *file);
GRP *GRP_FilterByEXT(GRP *grp, char *ext, int auto_free);

int GRP_CountFilesByEXT(GRP *grp, char *ext);
int GRP_Validate(const char *path);

#endif /* GRP_UTILS_H_ */
