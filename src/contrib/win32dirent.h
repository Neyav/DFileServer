// It is assumed that this file is public domain:
// Found here: http://mail-archives.apache.org/mod_mbox/httpd-dev/199706.mbox/%3CPine.LNX.3.95.970615134837.16974A-100000@aardvark.ukweb.com%3E

/*
 * Structures and types used to implement opendir/readdir/closedir
 * on Windows 95/NT.
 */

#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/types.h>

/* struct dirent - same as Unix */
struct dirent {
    long d_ino;                    /* inode (always 1 in WIN32) */
    off_t d_off;                /* offset to this dirent */
    unsigned short d_reclen;    /* length of d_name */
    char d_name[_MAX_FNAME+1];    /* filename (null terminated) */
};

/* typedef DIR - not the same as Unix */
typedef struct {
    long handle;                /* _findfirst/_findnext handle */
    short offset;                /* offset into directory */
    short finished;             /* 1 if there are not more files */
    struct _finddata_t fileinfo;  /* from _findfirst/_findnext */
    char *dir;                  /* the dir we are reading */
    struct dirent dent;         /* the dirent to return */
} DIR;

/* Function prototypes */
DIR *opendir(char *);
struct dirent *readdir(DIR *);
int closedir(DIR *);

/**********************************************************************
 * Implement dirent-style opendir/readdir/closedir on Window 95/NT
 *
 * Functions defined are opendir(), readdir() and closedir() with the
 * same prototypes as the normal dirent.h implementation.
 *
 * Does not implement telldir(), seekdir(), rewinddir() or scandir().
 * The dirent struct is compatible with Unix, except that d_ino is
 * always 1 and d_off is made up as we go along.
 *
 * The DIR typedef is not compatible with Unix.
 **********************************************************************/

DIR *opendir(char *dir)
{
    DIR *dp;
    char *filespec;
    long handle;
    int index;

    filespec = (char *) malloc(strlen(dir) + 2 + 1);
    strcpy(filespec, dir);
    index = strlen(filespec) - 1;
    if (index >= 0 && (filespec[index] == '/' || filespec[index] == '\\'))
        filespec[index] = '\0';
    strcat(filespec, "/*");

    dp = (DIR *)malloc(sizeof(DIR));
    dp->offset = 0;
    dp->finished = 0;
    dp->dir = _strdup(dir);

    if ((handle = _findfirst(filespec, &(dp->fileinfo))) < 0) {
//        if (errno == ENOENT)
//            dp->finished = 1;
//        else
        return NULL;
    }

    dp->handle = handle;
    free(filespec);

    return dp;
}

struct dirent *readdir(DIR *dp)
{
    if (!dp || dp->finished) return NULL;

    if (dp->offset != 0) {
        if (_findnext(dp->handle, &(dp->fileinfo)) < 0) {
            dp->finished = 1;
            return NULL;
        }
    }
    dp->offset++;

    strncpy(dp->dent.d_name, dp->fileinfo.name, _MAX_FNAME);
    dp->dent.d_ino = 1;
    dp->dent.d_reclen = strlen(dp->dent.d_name);
    dp->dent.d_off = dp->offset;

    return &(dp->dent);
}

int closedir(DIR *dp)
{
    if (!dp) return 0;
    _findclose(dp->handle);
    if (dp->dir) free(dp->dir);
    if (dp) free(dp);

    return 0;
}
