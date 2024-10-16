/*
   "$Id: conf_get.c 2901 2003-01-14 06:51:03Z spitzak $"

    Configuration file routines for the Fast Light Tool Kit (FLTK).

    Carl Thompson's config file routines version 0.5
    Copyright 1995-2000 Carl Everard Thompson (clip@home.net)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
    USA.
*/

#include "conf.h"
#include <config.h>

/* define this to give reading config files a serious speed boost
   but you will need to call conf_clear_cache() to see changes
*/
#define CONF_CACHED

#ifndef CONF_CACHED

extern int conf_is_path_rooted(const char *);
extern const char* conf_dirname(const char *);

/*
        int getconf(const char *configfile, const char *key, char *svalue,
                    int slen)

        description:
                gets the string associated with a key in a config file
        arguments:
                configfile: path of config file
                key: section/key to look for
                slen: length of passed string buffer
        return values:
                returns 0 for OK or error code defined in conf.h
                svalue: string associated with key
*/
int
getconf(const char *configfile, const char *k, char *svalue, int slen)
{
  FILE    *ifp;                                                                 /* file pointer for config file */
  char    current_section[CONF_MAX_SECT_LEN + 2] = "";                          /* current section */
  char    line[CONF_MAX_LINE_LEN];                                              /* line buffer */
  char    found[CONF_MAX_LINE_LEN] = "";                                        /* the last match found */
  char    *p, *p2;                                                              /* miscelaneous char pointers */
  char    key[CONF_MAX_LINE_LEN], section[CONF_MAX_LINE_LEN];                   /* key and section to look for */
  int     in_correct_section;                                                   /* found the section we want? */
  int     section_found = 0, key_found = 0;                                     /* did we ever find the right section or key? */
  static int depth = 0;                                                         /* don't go too deep */

  if (!configfile || !k || !svalue) return CONF_ERR_ARGUMENT;                   /* NULL pointer was passed */

  strncpy(section, k, sizeof(section));                                         /* copy key and section to buffer */
  if ((p = strrchr(section, conf_level_sep))) {                                 /* if a level separator found */
    strncpy(key,  p + 1, sizeof(key));                                          /* set key */
    *(p + 1) = (char)0;                                                         /* remove key from section */
  } else {                                                                      /* no level separator found */
    strcpy(key, section);                                                       /* set key */
    *section = (char)0;                                                         /* set toplevel section */
  }

  conf_trim(section); conf_trim(key);

  ifp = fopen(configfile, "r");
  if (!ifp) return CONF_ERR_FILE;                                               /* could not open config file */

  if (strlen(section) > (CONF_MAX_SECT_LEN - 1))                                /* if section name too long */
    return CONF_ERR_ARGUMENT;                                                   /* bail out */

  in_correct_section = (*section) ? 0 : 1;                                      /* found the right section if we want toplevel */
  while (fgets(line, sizeof(line), ifp)) {                                      /* while there are still lines in the section */
    /* hack so that comment separator can occur in value
       if not immediately followed by a whitespace       */
    for (p2 = line; (p = strchr(p2, conf_comment_sep)); p2 = p + 1) {
      if (strchr(CONF_WHITESPACE, *(p + 1)) ||
          line + strspn(line, CONF_WHITESPACE) == p)
      {
        *p = (char)0;
        break;
      }
    }

    conf_trim(line);                                                            /* remove unnecessary whitespace */
    if (!(*line)) continue;                                                     /* this is a blank line, go to next line */

    if (*line == '[') {                                                         /* in new section */
      strncpy(current_section, line + 1, sizeof(current_section));
      *(current_section + strlen(current_section) - 1) = conf_level_sep;
      if (!strcasecmp(current_section, section)) in_correct_section = 1;        /* this is the section we want */
      else in_correct_section = 0;                                              /* not the section we want */
      continue;                                                                 /* go to next line in file */
    }

    if (strncasecmp(current_section, section, strlen(current_section)))
      continue;                                                                 /* not wanted section or parent */

    if ( (p = strchr(line, conf_sep)) ) *p++ = '\0';                            /* if there is a separator character */
    else {                                                                      /* no separator? could be command */
      char* sv;                                                                 /* to save strtok_r state */
      p = strtok_r(line, CONF_WHITESPACE, &sv);                                 /* get the command */
      if (!strcasecmp(p, "include")) {                                          /* it is include command */
        char fn[CONF_MAXPATHLEN];                                               /* filename of include file */
        char sk[CONF_MAX_LINE_LEN];                                             /* what to look for in included file */
        int r;

        p = strtok_r(0, "", &sv);                                               /* get the name of file to be included */
        conf_trim(p);                                                           /* kill unecessary whitespace */
        if (conf_is_path_rooted(p)) strncpy(fn, p, sizeof(fn));                 /* fully qualified path */
        else snprintf(fn, sizeof(fn), "%s%s", conf_dirname(configfile), p);     /* figure out pathname */
        snprintf(sk, sizeof(sk), "%s%s", section + strlen(current_section),     /* section/key to look up in included file */
                 key);
        if (depth == CONF_MAX_INCLUDE) return CONF_ERR_DEPTH;                   /* went too deep */
        depth++;
        r = getconf(fn, sk, found, sizeof(found));                              /* look for what we want in included file */
        depth--;
        if (r == CONF_ERR_DEPTH) return CONF_ERR_DEPTH;                         /* went too deep */
        if (!r) section_found = key_found = 1;                                  /* we found what we wanted in included file */
      }
      continue;                                                                 /* go to next line */
    }

    /* p now points to value, line is now just key */
    if (!in_correct_section) continue;                                          /* section not found yet, keep looking for it */
    section_found = 1;                                                          /* must be in right section */

    conf_trim(line); conf_trim(p);                                              /* kill unnecessary whitespace */

    if (strcasecmp(line, key)) continue;                                        /* this isn't the entry we want- next line */
    key_found = 1;                                                              /* must be the right entry */

    strncpy(found, p, sizeof(found));                                           /* copy value to found */
  } /* while (fgets(line, sizeof(line), ifp)) */

  fclose(ifp);                                                                  /* close the config file */

  if (!section_found) return CONF_ERR_SECTION;                                  /* section was never found */
  if (!key_found) return CONF_ERR_KEY;                                          /* key was never found */

  strncpy(svalue, found, slen);                                                 /* copy found value to user variable */
  svalue[slen - 1] = (char)0;                                                   /* make sure string is null terminated */

  return CONF_SUCCESS;                                                          /* it worked? */
} /* getconf() */


/*
        void conf_clear_cache()

        description:
                clears all cached config file data so it will be reread
                from disk
*/

void conf_clear_cache(void) {}

#else /* CONF_CACHED */

#define NUM_CACHED 5

#include <time.h>

static struct {
  const char    *file;
  const char    *section;
  time_t        last_used;
  conf_list     list;
} cached[NUM_CACHED];

/*
        int getconf(const char *configfile, const char *key, char *svalue,
                    int slen)

        description:
                gets the string associated with a key in a config file
        arguments:
                configfile: path of config file
                key: section/key to look for
                slen: length of passed string buffer
        return values:
                returns 0 for OK or error code defined in conf.h
                svalue: string associated with key
*/

int
getconf(const char *configfile, const char *k, char *svalue, int slen)
{
  char    key[CONF_MAX_LINE_LEN], section[CONF_MAX_LINE_LEN];                   /* key and section to look for */
  int     use_this;                                                             /* which cached section */
  int     found;                                                                /* found section already cached */
  char    *p;                                                                   /* misc char pointer */
  int     i;                                                                    /* counter */

  if (!configfile || !k || !svalue) return CONF_ERR_ARGUMENT;                   /* NULL pointer was passed */

  strncpy(section, k, sizeof(section));                                         /* copy key and section to buffer */
  if ((p = strrchr(section, conf_level_sep))) {                                 /* if a level separator found */
    strncpy(key,  p + 1, sizeof(key));                                          /* set key */
    *(p) = (char)0;                                                             /* remove key from section */
  } else {                                                                      /* no level separator found */
    strcpy(key, section);                                                       /* set key */
    *section = (char)0;                                                         /* set toplevel section */
  }
  conf_trim(section); conf_trim(key);

  use_this = 0; found = 0;
  for (i = 0; i < NUM_CACHED; i++) {                                            /* loop through all cached sections */
    if (!cached[i].file) { use_this = i; break; }                               /* if not used yet */
    if (!strcasecmp(configfile, cached[i].file) &&                              /* if section already cached */
        !strcasecmp(section, cached[i].section))
    {
      found = 1;
      use_this = i;
      break;
    }
    if (cached[use_this].last_used > cached[i].last_used) use_this = i;
  }

  if (!found) {
    int r;
    if (cached[use_this].file) {
      free((void *)cached[use_this].file); cached[use_this].file = 0;
      free((void *)cached[use_this].section);
      conf_list_free(&cached[use_this].list);
    }
    r = getconf_keys(configfile, section, &cached[use_this].list);
    if (r) return r;
    cached[use_this].file = strdup(configfile);
    cached[use_this].section = strdup(section);
    if (!cached[use_this].file || !cached[use_this].section) {
      conf_list_free(&cached[use_this].list);
      return CONF_ERR_MEMORY;
    }
  }

  cached[use_this].last_used = time(0);
  return getconf_list(cached[use_this].list, key, svalue, slen);
}

/*
        void conf_clear_cache()

        description:
                clears all cached config file data so it will be reread
                from disk
*/

void
conf_clear_cache() {
  int i;                                                                        /* counter */

  for (i = 0; i < NUM_CACHED; i++) {                                            /* loop through all cached sections */
    if (cached[i].file) {
      free((void *)cached[i].file); cached[i].file = 0;
      free((void *)cached[i].section);
      conf_list_free(&cached[i].list);
    }
  }
}
#endif

/*
    End of "$Id: conf_get.c 2901 2003-01-14 06:51:03Z spitzak $".
*/
