/*
 * tsmq
 *
 * Alistair King, CAIDA, UC San Diego
 * corsaro-info@caida.org
 *
 * Copyright (C) 2012 The Regents of the University of California.
 *
 * This file is part of tsmq.
 *
 * tsmq is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * tsmq is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tsmq.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tsmq_client.h"

#define KEY_LOOKUP_CNT 1

static void usage(const char *name)
{
  fprintf(stderr,
	  "usage: %s [<options>]\n"
	  "       -b <broker-uri>    0MQ-style URI to connect to broker on\n"
	  "                          (default: %s)\n"
          "       -n <key-cnt>       Number of keys to lookup and insert fake data for (default: %d)\n"
	  "       -r <retries>       Number of times to resend a request\n"
	  "                          (default: %d)\n"
	  "       -a <ack-timeout>     Time to wait for request ack\n"
          "                            (default: %d)\n"
	  "       -l <lookup-timeout>  Time to wait for key lookups\n"
          "                            (default: %d)\n"
          "       -s <set-timeout>     Time to wait for key set\n"
          "                            (default: %d)\n",
	  name,
	  TSMQ_CLIENT_BROKER_URI_DEFAULT,
          KEY_LOOKUP_CNT,
	  TSMQ_CLIENT_REQUEST_RETRIES,
	  TSMQ_CLIENT_REQUEST_ACK_TIMEOUT,
          TSMQ_CLIENT_KEY_LOOKUP_TIMEOUT,
          TSMQ_CLIENT_KEY_SET_TIMEOUT);
}

int main(int argc, char **argv)
{
  /* for option parsing */
  int opt;
  int prevoptind;

  /* to store command line argument values */
  const char *broker_uri = NULL;

  int retries = TSMQ_CLIENT_REQUEST_RETRIES;
  int ack_timeout = TSMQ_CLIENT_REQUEST_ACK_TIMEOUT;
  int lookup_timeout = TSMQ_CLIENT_KEY_LOOKUP_TIMEOUT;
  int set_timeout = TSMQ_CLIENT_KEY_SET_TIMEOUT;

  int key_cnt = KEY_LOOKUP_CNT;

  tsmq_client_t *client;

  int i;

  while(prevoptind = optind,
	(opt = getopt(argc, argv, ":b:n:r:a:l:s:v?")) >= 0)
    {
      if (optind == prevoptind + 2 && *optarg == '-' ) {
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

	case 'b':
	  broker_uri = optarg;
	  break;

        case 'n':
          key_cnt = atoi(optarg);
          break;

	case 'r':
	  retries = atoi(optarg);
	  break;

	case 'a':
	  ack_timeout = atoi(optarg);
	  break;

        case 'l':
	  lookup_timeout = atoi(optarg);
	  break;

        case 's':
	  set_timeout = atoi(optarg);
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

  if((client = tsmq_client_init()) == NULL)
    {
      fprintf(stderr, "ERROR: could not initialize tsmq metadata client\n");
      goto err;
    }

  if(broker_uri != NULL)
    {
      tsmq_client_set_broker_uri(client, broker_uri);
    }

  tsmq_client_set_request_ack_timeout(client, ack_timeout);
  tsmq_client_set_key_lookup_timeout(client, lookup_timeout);
  tsmq_client_set_key_set_timeout(client, set_timeout);

  tsmq_client_set_request_retries(client, retries);

  if(tsmq_client_start(client) != 0)
    {
      tsmq_client_perr(client);
      return -1;
    }

  /* debug !! */
  char *key = "a.test.key";
  tsmq_client_key_t *response;
  uint64_t value = 123456;
  uint32_t time = 1404174060;

  fprintf(stderr, "Looking up backend ID for %s... ", key);
  if((response =
      tsmq_client_key_lookup(client, key)) == NULL)
    {
      tsmq_client_perr(client);
      goto err;
    }
  fprintf(stderr, "done\n");

  fprintf(stderr, "Running set on %d keys (%s)... ", key_cnt, key);
  for(i=0; i<key_cnt; i++)
    {
      if(tsmq_client_key_set_single(client, response, value, time) != 0)
        {
          tsmq_client_perr(client);
          goto err;
        }
    }
  fprintf(stderr, "done \n");

  /* cleanup */
  tsmq_client_key_free(&response);
  tsmq_client_free(&client);

  /* complete successfully */
  return 0;

 err:
  tsmq_client_key_free(&response);
  tsmq_client_free(&client);
  return -1;
}
