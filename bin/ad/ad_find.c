/* 
   Copyright (c) 2010 Frank Lahm <franklahm@gmail.com>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

#include <atalk/adouble.h>
#include <atalk/cnid.h>
#include <atalk/cnid_dbd_private.h>
#include <atalk/volinfo.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/directory.h>
#include "ad.h"

static volatile sig_atomic_t alarmed;

/*
  SIGNAL handling:
  catch SIGINT and SIGTERM which cause clean exit. Ignore anything else.
*/

static void sig_handler(int signo)
{
    alarmed = 1;
    return;
}

static void set_signal(void)
{
    struct sigaction sv;

    sv.sa_handler = sig_handler;
    sv.sa_flags = SA_RESTART;
    sigemptyset(&sv.sa_mask);
    if (sigaction(SIGTERM, &sv, NULL) < 0)
        ERROR("error in sigaction(SIGTERM): %s", strerror(errno));

    if (sigaction(SIGINT, &sv, NULL) < 0)
        ERROR("error in sigaction(SIGINT): %s", strerror(errno));

    memset(&sv, 0, sizeof(struct sigaction));
    sv.sa_handler = SIG_IGN;
    sigemptyset(&sv.sa_mask);

    if (sigaction(SIGABRT, &sv, NULL) < 0)
        ERROR("error in sigaction(SIGABRT): %s", strerror(errno));

    if (sigaction(SIGHUP, &sv, NULL) < 0)
        ERROR("error in sigaction(SIGHUP): %s", strerror(errno));

    if (sigaction(SIGQUIT, &sv, NULL) < 0)
        ERROR("error in sigaction(SIGQUIT): %s", strerror(errno));
}

static void usage_find(void)
{
    printf(
        "Usage: ad find VOLUME_PATH NAME\n"
        );
}

int ad_find(int argc, char **argv)
{
    int c;
    afpvol_t vol;

    if (argc != 4) {
        usage_find();
        exit(1);
    }

    set_signal();
    cnid_init();

    if (openvol(argv[2], &vol) != 0)
        ERROR("Cant open volume \"%s\"", argv[2]);

    int count;
    char resbuf[DBD_MAX_SRCH_RSLTS * sizeof(cnid_t)];
    if ((count = cnid_find(vol.volume.v_cdb, argv[3], strlen(argv[3]), resbuf, sizeof(resbuf))) < 1) {
        SLOG("No results");
    } else {
        cnid_t cnid;
        char *bufp = resbuf;
        bstring sep = bfromcstr("/");
        while (count--) {
            memcpy(&cnid, bufp, sizeof(cnid_t));
            bufp += sizeof(cnid_t);

            bstring path = NULL;
            bstring volpath = bfromcstr(vol.volinfo.v_path);
            BSTRING_STRIP_SLASH(volpath);
            char buffer[12 + MAXPATHLEN + 1];
            int buflen = 12 + MAXPATHLEN + 1;
            char *name;
            cnid_t did = cnid;
            struct bstrList *pathlist = bstrListCreateMin(32);

            while (did != DIRDID_ROOT) {
                if ((name = cnid_resolve(vol.volume.v_cdb, &did, buffer, buflen)) == NULL)
                    ERROR("Can't resolve CNID: %u", ntohl(did));
                bstrListPush(pathlist, bfromcstr(name));
            }
            bstrListPush(pathlist, volpath);
            path = bjoinInv(pathlist, sep);
            bstrListDestroy(pathlist);
            
            printf("%s\n", cfrombstr(path));
            bdestroy(path);
        }
        bdestroy(sep);
    }

    closevol(&vol);

    return 0;
}