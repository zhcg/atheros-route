/* geninittramfs -- generate a cpio-format file useful for initializing
*       a RAM file system.
*   The linux kernel includes such a tool, and "cpio" is such a tool,
*   but this tool has the advantage of handling the 
*   standard device table format as well as other input formats.
*   
*       See help message below.
*
*/

const char * const HelpLines[] = {
"geninitramfs -- generate cpio-format file useful for initializing",
"    a RAM file system. ",
"",
"SYNOPSIS",
"    geninitramfs [<option>]...  >outfile.cpio ",
"",
"OPTIONS",
"    -h | -help | --help  -- print this message and exit",
"    -copy <from> <to>   -- real file or directory is archive as <to> ",
"       If <from> is a directory, it is recursed. ",
"    -device_table <path>  -- create device node entries per ",
"       intructions in device table file.",
"    -directory <to>  -- archive a directory node ",
"",
"DESCRIPTION",
"    geninitramfs generates a cpio format archive acceptable to the ",
"linux kernel initramfs building, or to the inittrampoline program ",
"when compressed with lzma. ",
"",
"The -copy option ignores the actual Uid and Gid of the file being ",
"copied and puts 0, 0 in the archive. ",
"",
"Handles only a SUBSET of the (loosely defined) device table file ",
"specification, specifically it ignores anything on a line past ",
"the first 7 fields. ",
"The supported format of a device table file is: ",
"-- Empty lines are ignored. ",
"-- Lines beginning with '#' are ignored. ",
"-- Other lines have format of 7 whitespace-separated words  ",
"    (additional words ignored): ",
"    -- Device file path, e.g. /dev/tty ",
"    -- Device file type, 'c' or 'b' (without the quotes) ",
"    -- Octal numeral giving r/w permissions, usuallly 666 ",
"    -- Uid, usually  0 ",
"    -- Gid, usually 0 ",
"    -- Major device number ",
"    -- Minor device number ",
"This subset is also supported by build tools such as genext2fs, so ",
"you can use the same device table file with them. ",
"Caution: you should make sure that the directory needed for a device ",
"file path has been already established. ",
(const char *)0     /* terminator */
};

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

/* PrintHelp -- print help message for this program 
*/
void PrintHelp(void)
{
    int ILine;
    for (ILine = 0; HelpLines[ILine]; ILine++)
        printf("%s\n", HelpLines[ILine]);
    return;
}

struct cpioHeader {
    unsigned short magic;
        #define CPIO_MAGIC 0x71c7
    unsigned short dev;
    unsigned short ino;
    unsigned short mode;
    unsigned short uid;
    unsigned short gid;
    unsigned short nlink;
    unsigned short rdev;
    unsigned short mtime[2];    /* big endian w.r.t. shortwords */
    unsigned short namesize;
    unsigned short filesize[2]; /* big endian w.r.t. shortwords */
};

static char CopyBuf[0x10000];

/* DoCopy -- create archive segment(s) for file or directory tree
*/
void DoCopy(
        char *From, 
        char *To
        )
{
    struct stat Stat;
    struct cpioHeader Header = {};
    int PathLen = strlen(To)+1/*incl. null!*/;
    int FileSize = 0;
    int NRead;

    Header.magic = CPIO_MAGIC;
    if (lstat(From, &Stat)) {
        fprintf(stderr, "lstat errno %d on file %s\n", errno, From);
        exit(1);
    }
    Header.dev = Stat.st_rdev;
    // don't care: unsigned short ino;
    Header.mode = Stat.st_mode;
    // force to be root: unsigned short uid;
    // force to be root: unsigned short gid;
    Header.nlink = 1;
    Header.rdev = Stat.st_rdev;
    // don't care: unsigned short mtime[2];    /* big endian w.r.t. shortwords */
    Header.namesize = PathLen/*incl. null*/;
    if (S_ISLNK(Stat.st_mode) || S_ISREG(Stat.st_mode)) {
        FileSize = Stat.st_size;
        Header.filesize[0] = FileSize >> 16;
        Header.filesize[1] = FileSize & 0xffff;
    }
    fwrite(&Header, sizeof(Header), 1, stdout);
    fwrite(To, PathLen/*incl. null*/, 1, stdout);
    if ((PathLen&1) != 0)
        fwrite("", 1, 1, stdout);       /* align */
    if (S_ISLNK(Stat.st_mode)) {        /* symlink */
        char LinkValue[256];
        memset(LinkValue, 0, sizeof(LinkValue));
        NRead = readlink(From, LinkValue, sizeof(LinkValue)-1);
        if (NRead != FileSize) {
            fprintf(stderr, "readlink of %s returned %d instead of %d\n",
                From, NRead, FileSize);
            exit(1);
        }
        fwrite(LinkValue, NRead, 1, stdout);
        if ((NRead&1) != 0)
            fwrite("", 1, 1, stdout);
    } 
    else if (S_ISREG(Stat.st_mode)) {   /* regular file */
        int NTotal = 0;
        int Fd = open(From, O_RDONLY);
        if (Fd < 0) {
            fprintf(stderr, "open of %s failed errno %d\n", From, errno);
            exit(1);
        }
        while (NTotal < FileSize) {
            int NToRead = FileSize - NTotal;
            if (NToRead > sizeof(CopyBuf)) NToRead = sizeof(CopyBuf);
            NRead = read(Fd, CopyBuf, NToRead);
            if (NRead < 0) {
                fprintf(stderr, "read of %s failed errno %d\n", From, errno);
                exit(1);
            }
            if (NRead == 0) {
                fprintf(stderr, "read of %s premature EOF\n", From);
                exit(1);
            }
            fwrite(CopyBuf, NRead, 1, stdout);
            NTotal += NRead;
        }
        close(Fd);
        if ((FileSize&1) != 0)
            fwrite("", 1, 1, stdout);       /* align */
    } 
    else if (S_ISDIR(Stat.st_mode)) {   /* directory */
        DIR *D = opendir(From);
        struct dirent *d;
        if (!D) {
            fprintf(stderr, "opendir of %s failed errno %d\n", From, errno);
            exit(1);
        }
        while ((d = readdir(D)) != NULL) {
            char FromBuf[256];
            char ToBuf[256];
            char *Name = d->d_name;     /* null terminated under Linux */
            if (!strcmp(Name, "..") || !strcmp(Name, "."))
                continue;
            if (snprintf(FromBuf, sizeof(FromBuf), "%s/%s", From, Name) 
                >= sizeof(FromBuf) ||
                snprintf(ToBuf  , sizeof(ToBuf  ), "%s/%s", To  , Name)
                >= sizeof(ToBuf)) {
                fprintf(stderr, "Path too long at %s <-> %s + %s\n",
                    From, To, Name);
                exit(1);
            }
            DoCopy(FromBuf, ToBuf);
        }
        closedir(D);
    }
}


/* DoDeviceTable -- create archive of device nodes specified by
*       device table file.
*/
void DoDeviceTable(
        char *TablePath)
{
    FILE *F = fopen(TablePath, "r");
    char Buf[256];
    if (!F) {
        fprintf(stderr, "Could not open device table file `%s'\n", TablePath);
        exit(1);
    }
    while (fgets(Buf, sizeof(Buf), F) != NULL) {
        char Path[256];
        char Type;
        int Mode;
        int Gid;
        int Uid;
        int Major;
        int Minor;
        int NMatch;
        if (Buf[0] == '#')
            continue;
        if (!isgraph(Buf[0]))
            continue;
        NMatch = sscanf(Buf, "%s %c %o %d %d %d %d",
            Path, &Type, &Mode, &Uid, &Gid, &Major, &Minor);
        if (NMatch < 7) {
        } else {
            /* NOTE: various web sources document 3 extra fields:
            *   start, incr, count
            *   ... but the Atheros dev.txt files have two extra
            *   fields of unknown use... hmmm...
            */
            struct cpioHeader Header = {};
            int PathLen = strlen(Path)+1/*for null*/;
            switch(Type) {
                case 'c':
                    Mode |= S_IFCHR;
                break;
                case 'b':
                    Mode |= S_IFBLK;
                break;
                default:
                    fprintf(stderr, "Unknown device type %c\n", Type);
                    exit(1);
                break;
            }
            Header.magic = CPIO_MAGIC;
            // ignore: unsigned short dev;
            // ignore: unsigned short ino;
            Header.mode = Mode;
            Header.uid = Uid;
            Header.gid = Gid;
            Header.nlink = 1;
            Header.rdev = (Major << 8) | Minor;
            // ignore: unsigned short mtime[2];
            Header.namesize = PathLen;
            // ignore: unsigned short filesize[2];
            fwrite(&Header, sizeof(Header), 1, stdout);
            fwrite(Path, PathLen, 1, stdout);
            if ((PathLen&1) != 0)
                fwrite("", 1, 1, stdout);       /* align */
                } 
    }
    fclose(F);
}


/* DoDirectory -- create a directory
*/
void DoDirectory(char *Path)
{
    int PathLen = strlen(Path)+1/*for null*/;
    struct cpioHeader Header = {};
    Header.magic = CPIO_MAGIC;
    Header.nlink = 2;
    Header.mode = S_IFDIR|0700;
    Header.namesize = PathLen;
    fwrite(&Header, sizeof(Header), 1, stdout);
    fwrite(Path, PathLen, 1, stdout);
    if ((PathLen&1) != 0)
        fwrite("", 1, 1, stdout);       /* align */
}

/* Finalize -- create ending cpio record.
*/
void Finalize(void)
{
    struct cpioHeader Header = {};
    char *Path = "TRAILER!!!";
    int PathLen = strlen(Path)+1;
    Header.magic = CPIO_MAGIC;
    Header.namesize = PathLen;
    fwrite(&Header, sizeof(Header), 1, stdout);
    fwrite(Path, PathLen, 1, stdout);
    if ((PathLen&1) != 0)
        fwrite("", 1, 1, stdout);       /* align */
}

int main(int argc, char **argv)
{
    char *arg;
    argv++;     /* skip program name */
    while ((arg = *argv++) != NULL) {
        if (!strcmp(arg, "-h") || !strcmp(arg, "-help") ||
                    !strcmp(arg, "--help")) {
            PrintHelp();
            exit(0);
        }
        if (!strcmp(arg, "-copy")) {
            char *From;
            char *To;
            if ((From = *argv++) == NULL || (To = *argv++) == NULL) {
                fprintf(stderr, "Missing arg to -copy\n");
                exit(1);
            }
            DoCopy(From, To);
            continue;
        }
        if (!strcmp(arg, "-device_table")) {
            char *table = *argv++;
            if (!table) {
                fprintf(stderr, "Missing arg to -device_table\n");
                exit(1);
            }
            DoDeviceTable(table);
            continue;
        }
        if (!strcmp(arg, "-directory")) {
            char *dirpath = *argv++;
            if (!dirpath) {
                fprintf(stderr, "Missing arg to -directory\n");
                exit(1);
            }
            DoDirectory(dirpath);
            continue;
        }
        fprintf(stderr, "Unknown arg: %s\n", arg);
        exit(1);
    }
    Finalize();
    if (ferror(stdout)) {
        fprintf(stderr, "Error writing output!\n");
        exit(1);
    }
    return 0;
}
