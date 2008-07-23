/* This file is part of Dico.
   Copyright (C) 1998-2000, 2008 Sergey Poznyakoff

   Dico is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   Dico is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Dico.  If not, see <http://www.gnu.org/licenses/>. */

#include <dicod.h>

dico_list_t sasl_disabled_mech;

#ifdef WITH_GSASL
#include <gsasl.h>

static Gsasl *ctx;   

static int
cmp_names(const void *item, const void *data)
{
    return c_strcasecmp(*(char**)item, data);
}

static int
disabled_mechanism_p(char *name)
{
    return !!dico_list_locate(sasl_disabled_mech, name, cmp_names);
}

static void
send_challenge(dico_stream_t str, char *data)
{
    stream_printf(str, "130 challenge follows\r\n");
    /* FIXME: use dicod_ostream_create */
    stream_writez(str, data);
    stream_writez(str, ".\r\n");
    stream_printf(str, "330 send response\r\n");
}    

#define SASLRESP "SASLRESP"

static int
get_sasl_response(dico_stream_t str, char **pret, char **pbuf, size_t *psize)
{
    char *p;
    size_t rdbytes;
    
    if (get_input_line(str, pbuf, psize, &rdbytes))
	return 1;
    p = *pbuf;
    rdbytes = dico_trim_nl(p);
    if (rdbytes > sizeof(SASLRESP)
	&& memcmp(p, SASLRESP, sizeof(SASLRESP) - 1)
	&& isspace(p[sizeof(SASLRESP)])) {
	for (p += sizeof(SASLRESP); isspace(*p); p++);
	*pret = p;
	return 0;
    }
    dico_log(L_ERR, 0, _("Unexpected input instead of SASLRESP command: %s"),
	     *pbuf);
    return 1;
}

#define RC_SUCCESS 0
#define RC_FAIL    1
#define RC_NOMECH  2

static int
sasl_auth(dico_stream_t str, char *mechanism, char *initresp)
{
    int rc;
    Gsasl_session *sess_ctx;
    char *input;
    char *output;
    char *inbuf;
    size_t insize;
    char *username = NULL;

    if (disabled_mechanism_p(mechanism)) 
	return RC_NOMECH;
    rc = gsasl_server_start (ctx, mechanism, &sess_ctx);
    if (rc != GSASL_OK) {
	dico_log(L_ERR, 0, _("SASL gsasl_server_start: %s"),
		 gsasl_strerror(rc));
	return rc == GSASL_UNKNOWN_MECHANISM ? RC_NOMECH : RC_FAIL;
    }

    gsasl_callback_hook_set(ctx, &username);
    output = NULL;
    inbuf = xstrdup(initresp);
    insize = strlen(initresp) + 1;
    input = inbuf;
    while ((rc = gsasl_step64(sess_ctx, input, &output)) == GSASL_NEEDS_MORE) {
	send_challenge(str, output);
	
	free(output);
	if (get_sasl_response(str, &input, &inbuf, &insize)) 
	    return RC_FAIL;
    }

    if (rc != GSASL_OK) {
	dico_log(L_ERR, 0, _("GSASL error: %s"), gsasl_strerror(rc));
	free(output);
	free(inbuf);
	return RC_FAIL;
    }

    /* Some SASL mechanisms output data when GSASL_OK is returned */
    if (output[0]) 
	send_challenge(str, output);

    free(output);
    free(inbuf);
    
    if (username == NULL) {
	dico_log(L_ERR, 0, _("GSASL %s: cannot get username"), mechanism);
	return RC_FAIL;
    }

    user_name = xstrdup(username);
    udb_get_groups(user_db, username, &user_groups);
    check_db_visibility();

    /* FIXME: Install a Gsasl stream */
    
    return RC_SUCCESS;
}

static void
dicod_saslauth(dico_stream_t str, int argc, char **argv)
{
    int rc;
    char *resp;
    
    if (udb_open(user_db)) {
	dico_log(L_ERR, 0, _("failed to open user database"));
	stream_writez(str,
		      "531 Access denied, "
		      "use \"SHOW INFO\" for server information\r\n");
	return;
    }
    rc = sasl_auth(str, argv[1], argv[2]);
    udb_close(user_db);
    switch (rc) {
    case RC_SUCCESS:
	resp = "230 Authentication successful";
	break;

    case RC_FAIL:
	resp = "531 Access denied, "
	       "use \"SHOW INFO\" for server information";
	break;

    case RC_NOMECH:
	resp = "532 Access denied, unknown mechanism";
	break;
    }
    stream_printf(str, "%s\r\n", resp);
}


static int
callback(Gsasl *ctx, Gsasl_session *sctx, Gsasl_property prop)
{
    int rc = GSASL_NO_CALLBACK;
    char *username = gsasl_callback_hook_get(ctx);
    char *string;
    
    switch (prop) {
    case GSASL_PASSWORD:
	if (udb_get_password(user_db, username, &string)) {
	    dico_log(L_ERR, 0,
		     _("failed to get password for `%s' from the database"),
		     username);
	    return GSASL_NO_PASSWORD;
	} 
	gsasl_property_set(sctx, prop, string);
	rc = GSASL_OK;
	break;

    case GSASL_AUTHID:
	gsasl_property_set(sctx, prop, username);
	rc = GSASL_OK;
	break;

    case GSASL_AUTHZID:
	if (!username) {
	    dico_log(L_ERR, 0, _("user name not supplied"));
	    return GSASL_NO_AUTHZID;
	}
	gsasl_property_set(sctx, prop, username);
	rc = GSASL_OK;
	break;

    case GSASL_SERVICE:
	gsasl_property_set(sctx, prop, "dico");
	rc = GSASL_OK;
	break;

    case GSASL_REALM:
	/* FIXME: Use a separate realm? */
	gsasl_property_set(sctx, prop, hostname);
	rc = GSASL_OK;
	break;

    case GSASL_HOSTNAME:
	gsasl_property_set(sctx, prop, hostname);
	rc = GSASL_OK;
	break;

    default:
	dico_log(L_NOTICE, 0, _("Unknown callback property %d"), prop);
	break;
    }

    return rc;
}

static int
init_sasl_0()
{
    int rc = gsasl_init (&ctx);
    if (rc != GSASL_OK) {
	dico_log(L_ERR, 0, _("cannot initialize libgsasl: %s"),
		 gsasl_strerror(rc));
	return 1;
    }
    gsasl_callback_set(ctx, callback);
    return 0;
}

static int
init_sasl_1()
{
    static struct dicod_command cmd =
	{ "SASLAUTH", 3, "mechanism initial-response",
	  "Start SASL authentication",
	  dicod_saslauth };
    static int sasl_initialized;

    if (!sasl_initialized) {
	sasl_initialized = 1;
	dicod_add_command(&cmd);
    }
    return 0;
}

static char *mech_to_capa_table[][2] = {
    { "EXTERNAL", "external" },
    { "SKEY", "skey" },
    { "GSSAPI", "gssapi" },
    { "KERBEROS_V4", "kerberos_v4" }
};

static char *
xlate_mech_to_capa(char *mech)
{
    int i;
    size_t len;
    char *rets, *p;
    
    for (i = 0; i < DICO_ARRAY_SIZE(mech_to_capa_table); i++)
	if (strcmp(mech_to_capa_table[i][0], mech) == 0)
	    return mech_to_capa_table[i][1];

    len = strlen(mech) + 1;
    rets = p = xmalloc(len + 1);
    *p++ = 'x';
    for (; *mech; mech++)
	*p++ = tolower(*mech);
    *p = 0;
    return rets;
}
	

void
register_sasl()
{
  int rc;
  char *listmech;
  int mechc;
  char **mechv;

  if (init_sasl_0())
      return;
  rc =  gsasl_server_mechlist(ctx, &listmech);
  if (rc != GSASL_OK) {
      dico_log(L_ERR, 0, _("cannot get list of available SASL mechanisms: "
			   "%s"),
	       gsasl_strerror (rc));
      return;
  }
  
  if (dico_argcv_get(listmech, "", NULL, &mechc, &mechv) == 0) {
      int i;
      for (i = 0; i < mechc; i++) {
	  if (!disabled_mechanism_p(mechv[i])) {
	      char *name;
	      name = xlate_mech_to_capa(mechv[i]);
	      dicod_capa_register(name, NULL, init_sasl_1, NULL);
	      dicod_capa_add(name);
	  }
      }
      dico_argcv_free(mechc, mechv);
  }
  free(listmech);
}

#else
void
register_sasl()
{
    /* nothing */
}
#endif
