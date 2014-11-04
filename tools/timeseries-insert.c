/*
 * libtimeseries
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of libtimeseries.
 *
 * libtimeseries is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libtimeseries is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libtimeseries.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <wandio.h>
#include <wandio_utils.h>

#include <libtimeseries.h>

#include "config.h"

#define BUFFER_LEN 1024

timeseries_t *timeseries = NULL;
timeseries_backend_t *backends[TIMESERIES_BACKEND_ID_LAST];
int backends_cnt = 0;

static int insert(char *line)
{
  int i;
  char *end = NULL;
  char *key = NULL;
  char *value_str = NULL;
  char *time_str = NULL;
  uint64_t value = 0;
  uint32_t time = 0;

  /* line format is "<key> <value> <time>" */

  /* get the key string */
  if((key = strsep(&line, " ")) == NULL)
    {
      /* malformed line */
      fprintf(stderr, "ERROR: Malformed metric record (missing key): %s\n",
              key);
      return 0;
    }

  /* get the value string */
  if((value_str = strsep(&line, " ")) == NULL)
    {
      /* malformed line */
      fprintf(stderr, "ERROR: Malformed metric record (missing value): %s\n",
              key);
      return 0;
    }
  /* parse the value */
  value = strtoull(value_str, &end, 10);
  if(end == value_str || *end != '\0' || errno == ERANGE)
    {
      fprintf(stderr, "ERROR: Invalid metric value for '%s': '%s'\n",
              key, value_str);
      return 0;
    }

  /* get the time string */
  if((time_str = strsep(&line, " ")) == NULL)
    {
      /* malformed line */
      fprintf(stderr, "ERROR: Malformed metric record (missing time): '%s %s'\n",
              key, value_str);
      return 0;
    }
  /* parse the time */
  time = strtoul(time_str, &end, 10);
  if(end == time_str || *end != '\0' || errno == ERANGE)
    {
      fprintf(stderr, "ERROR: Invalid metric time for '%s %s': '%s'\n",
              key, value_str, time_str);
      return 0;
    }

  for(i=0; i<backends_cnt; i++)
    {
      if(timeseries_set_single(backends[i], key, value, time) != 0)
        {
          return -1;
        }
    }
  return 0;
}

static void backend_usage()
{
  assert(timeseries != NULL);
  timeseries_backend_t **avail_backends = NULL;
  int i;

  /* get the available backends from libtimeseries */
  avail_backends = timeseries_get_all_backends(timeseries);

  fprintf(stderr,
	  "                            available backends:\n");
  for(i = 0; i < TIMESERIES_BACKEND_ID_LAST; i++)
    {
      /* skip unavailable backends */
      if(avail_backends[i] == NULL)
	{
	  continue;
	}

      assert(timeseries_get_backend_name(avail_backends[i]));
      fprintf(stderr,
	      "                            - %s\n",
	      timeseries_get_backend_name(avail_backends[i]));
    }
}

static void usage(const char *name)
{
  fprintf(stderr,
	  "usage: %s -t <ts-backend> [-f input-file]\n"
          "       -f <input-file>    File to read time series data from (default: stdin)\n"
	  "       -t <ts-backend>    Timeseries backend to use for writing\n",
	  name);
  backend_usage();
}

static int init_timeseries(char *ts_backend)
{
  char *strcpy = NULL;
  char *args = NULL;

  if((strcpy = strdup(ts_backend)) == NULL)
    {
      goto err;
    }

  if((args = strchr(ts_backend, ' ')) != NULL)
    {
      /* set the space to a nul, which allows ts_backend to be used
	 for the backend name, and then increment args ptr to
	 point to the next character, which will be the start of the
	 arg string (or at worst case, the terminating \0 */
      *args = '\0';
      args++;
    }

  if((backends[backends_cnt] =
      timeseries_get_backend_by_name(timeseries, ts_backend)) == NULL)
    {
      fprintf(stderr, "ERROR: Invalid backend name (%s)\n",
	      ts_backend);
      goto err;
    }

  if(timeseries_enable_backend(timeseries, backends[backends_cnt], args) != 0)
    {
      fprintf(stderr, "ERROR: Failed to initialized backend (%s)\n",
	      ts_backend);
      goto err;
    }

  backends_cnt++;

  free(strcpy);

  return 0;

 err:
  if(strcpy != NULL)
    {
      free(strcpy);
    }
  return -1;
}

int main(int argc, char **argv)
{
  /* for option parsing */
  int opt;
  int prevoptind;

  /* to store command line argument values */
  char *ts_backend[TIMESERIES_BACKEND_ID_LAST];
  int ts_backend_cnt = 0;

  int i;

  char *input_file = "-";
  io_t *infile = NULL;
  char buffer[BUFFER_LEN];

  /* better just grab a pointer to lts before anybody goes crazy and starts
     dumping usage strings */
  if((timeseries = timeseries_init()) == NULL)
    {
      fprintf(stderr, "ERROR: Could not initialize libtimeseries\n");
      return -1;
    }

  while(prevoptind = optind,
	(opt = getopt(argc, argv, ":f:t:v?")) >= 0)
    {
      if (optind == prevoptind + 2 && (optarg == NULL || *optarg == '-') ) {
        opt = ':';
        -- optind;
      }
      switch(opt)
	{
	case ':':
	  fprintf(stderr, "ERROR: Missing option argument for -%c\n", optopt);
	  usage(argv[0]);
	  return -1;
	  break;

        case 'f':
          input_file = optarg;
          break;

	case 't':
          if(ts_backend_cnt >= TIMESERIES_BACKEND_ID_LAST-1)
            {
              fprintf(stderr, "ERROR: At most %d backends can be enabled\n",
                      TIMESERIES_BACKEND_ID_LAST);
              usage(argv[0]);
              return -1;
            }
	  ts_backend[ts_backend_cnt++] = optarg;
	  break;

	case '?':
	case 'v':
	  fprintf(stderr, "libtimeseries version %d.%d.%d\n",
		  LIBTIMESERIES_MAJOR_VERSION,
		  LIBTIMESERIES_MID_VERSION,
		  LIBTIMESERIES_MINOR_VERSION);
	  usage(argv[0]);
	  return 0;
	  break;

	default:
	  usage(argv[0]);
	  return -1;
	  break;
	}
    }

  /* NB: once getopt completes, optind points to the first non-option
     argument */

  if(ts_backend_cnt == 0)
    {
      fprintf(stderr,
	      "ERROR: Timeseries backend(s) must be specified\n");
      usage(argv[0]);
      return -1;
    }

  for(i=0; i<ts_backend_cnt; i++)
    {
      assert(ts_backend[i] != NULL);
      if(init_timeseries(ts_backend[i]) != 0)
        {
          usage(argv[0]);
          goto err;
        }
    }

  assert(timeseries != NULL && backends_cnt > 0);

  timeseries_log(__func__, "Reading metrics from %s", input_file);
  /* open the input file, (defaults to stdin) */
  if((infile = wandio_create(input_file)) == NULL)
    {
      fprintf(stderr, "ERROR: Could not open %s for reading\n", input_file);
      usage(argv[0]);
      goto err;
    }

  /* read from the input file (chomp off the newlines) */
  while(wandio_fgets(infile, &buffer, BUFFER_LEN, 1) > 0)
    {
      /* treat # as comment line, and ignore empty lines */
      if(buffer[0] == '#' || buffer[0] == '\0')
        {
          continue;
        }

      insert(buffer);
    }

  /* free timeseries, backends will be free'd */
  timeseries_free(timeseries);

  /* complete successfully */
  return 0;

 err:
  timeseries_free(timeseries);
  return -1;
}
