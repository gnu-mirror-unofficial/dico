/* This file is part of GNU Dico. 
   Copyright (C) 1998-2000, 2008 Sergey Poznyakoff

   GNU Dico is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GNU Dico is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Dico.  If not, see <http://www.gnu.org/licenses/>. */

#include "dico-priv.h"

#ifdef WITH_GSASL
#include <gsaslstr.h>

static dico_list_t
get_implemented_mechs(Gsasl *ctx)
{
    char *listmech;
    dico_list_t supp = NULL;
    int rc;
    int mechc;
    char **mechv;
    
    rc =  gsasl_server_mechlist(ctx, &listmech);
    if (rc != GSASL_OK) {
	dico_log(L_ERR, 0, _("cannot get list of available SASL mechanisms: "
			     "%s"),
		 gsasl_strerror (rc));
	return NULL;
    }

    if (dico_argcv_get(listmech, "", NULL, &mechc, &mechv) == 0) {
	int i;
	supp = xdico_list_create();
	for (i = 0; i < mechc; i++) 
	    xdico_list_append(supp, mechv[i]);
	free(mechv);
    }
    free(listmech);
    return supp;
}

static int
_free_el(void *item, void *data DICO_ARG_UNUSED)
{
    free(item);
    return 0;
}

static char *
mech_intersect_first(dico_iterator_t itr, struct dict_connection *conn)
{
    char *p;
    
    for (p = dico_iterator_first(itr); p; p = dico_iterator_next(itr)) {
	int i;
	for (i = 0; i < conn->capac; i++)
	    if (xdico_sasl_capa_match_p(p, conn->capav[i]))
		return xstrdup(p);
    }
    return NULL;
}

static int
str_str_cmp(const void *item, const void *data)
{
    return c_strcasecmp(item, data);
}

static void
upcase(char *str)
{
    for (; *str; str++)
	*str = toupper(*str);
}

static char *
selectmech(struct dict_connection *conn, Gsasl *ctx, struct auth_cred *cred)
{
    dico_list_t impl;
    dico_iterator_t itr;
    char *mech;

    impl = get_implemented_mechs(ctx);
    if (!impl)
	return NULL;
    if (cred->mech) {
	dico_list_t supp = dico_list_intersect(cred->mech, impl, str_str_cmp);
	dico_list_destroy(&impl, _free_el, NULL);
	impl = supp;
    }
    itr = xdico_iterator_create(impl);
    mech = mech_intersect_first(itr, conn);
    dico_iterator_destroy(&itr);
    dico_list_destroy(&impl, NULL, NULL);
    if (mech)
	upcase(mech);
    return mech;
}

#define CRED_HOSTNAME(c) ((c)->hostname ? (c)->hostname :  \
			  ((c)->hostname = xdico_local_hostname()))

static int
callback(Gsasl *ctx, Gsasl_session *sctx, Gsasl_property prop)
{
    int rc = GSASL_NO_CALLBACK;
    struct auth_cred *cred = gsasl_callback_hook_get(ctx);

    switch (prop) {
    case GSASL_PASSWORD:
	gsasl_property_set(sctx, prop, cred->pass);
	rc = GSASL_OK;
	break;

    case GSASL_AUTHID:
    case GSASL_AUTHZID:
	gsasl_property_set(sctx, prop, cred->user);
	rc = GSASL_OK;
	break;

    case GSASL_SERVICE:
	gsasl_property_set(sctx, prop,
			   cred->service ? cred->service : "dico");
	rc = GSASL_OK;
	break;

    case GSASL_REALM:
	gsasl_property_set(sctx, prop,
			   cred->realm ? cred->realm : CRED_HOSTNAME(cred));
	rc = GSASL_OK;
	break;

    case GSASL_HOSTNAME:
	gsasl_property_set(sctx, prop, CRED_HOSTNAME(cred));
	rc = GSASL_OK;
	break;
	
    default:
	dico_log(L_NOTICE, 0, _("Unknown callback property %d"), prop);
	break;
    }

    return rc;
}

static int
sasl_read_response(struct dict_connection *conn, char **data)
{
    if (dict_read_reply(conn)) {
	dico_log(L_ERR, 0, _("GSASL handshake aborted"));
	return 1;
    }
    if (dict_status_p(conn, "130")) {
	if (dict_multiline_reply(conn)
	    || dict_read_reply(conn)) {
	    dico_log(L_ERR, 0, _("GSASL handshake aborted"));
	    return 1;
	}
	*data = obstack_finish(&conn->stk);
	dico_trim_nl(*data);
    } else
	*data = NULL;
    return 0;
}

static void
sasl_free_data(struct dict_connection *conn, char **pdata)
{
    if (*pdata) {
	obstack_free(&conn->stk, *pdata);
	*pdata = NULL;
    }
}
    
int
do_gsasl_auth(Gsasl *ctx, struct dict_connection *conn, char *mech)
{
    Gsasl_session *sess;
    int rc;
    char *output = NULL;
    char *input = NULL;
    
    rc = gsasl_client_start(ctx, mech, &sess);
    if (rc != GSASL_OK) {
	dico_log(L_ERR, 0, "SASL gsasl_client_start: %s",
		 gsasl_strerror(rc));
	return 1;
    }
    stream_printf(conn->str, "SASLAUTH %s\r\n", mech);

    if (sasl_read_response(conn, &input))
	return 1;

    do {
	if (!dict_status_p(conn, "330")) {
	    dico_log(L_ERR, 0,
		     _("GSASL handshake aborted: unexpected reply: %s"),
		     conn->buf);
	    return 1;
	}
	rc = gsasl_step64(sess, input, &output);
	sasl_free_data(conn, &input);
	if (rc != GSASL_NEEDS_MORE && rc != GSASL_OK)
	    break;
	stream_printf(conn->str, "SASLRESP %s\r\n",
		      output[0] ? output : "\"\"");
	free(output);
	output = NULL;
	if (sasl_read_response(conn, &input))
	    return 1;
    } while (rc == GSASL_NEEDS_MORE);

    free(output);
    if (rc != GSASL_OK) {
	dico_log(L_ERR, 0, _("GSASL error: %s"), gsasl_strerror(rc));
	return 1;
    }

    if (dict_status_p(conn, "330")) {
	/* Additional data are ignored. */
	sasl_free_data(conn, &input);
	if (sasl_read_response(conn, &input))
	    return 1;
    }

    if (dict_status_p(conn, "230")) {
	/* Authentication successful */
	insert_gsasl_stream(sess, &conn->str);
	return 0;
    }
    
    print_reply(conn);
    
    return 1;
}

int
saslauth0(struct dict_connection *conn, struct auth_cred *cred)
{
    Gsasl *ctx;
    int rc;
    char *mech;

    XDICO_DEBUG(1, _("Trying SASL\n"));
    rc = gsasl_init(&ctx);
    if (rc != GSASL_OK)	{
	dico_log(L_ERR, 0, _("Cannot initialize libgsasl: %s"),
		 gsasl_strerror(rc));
	free(mech);
	return AUTH_FAIL;
    }
    gsasl_callback_hook_set(ctx, cred);
    gsasl_callback_set(ctx, callback);

    mech = selectmech(conn, ctx, cred);
    if (!mech) {
	dico_log(L_ERR, 0, _("No suitable SASL mechanism found"));
	return AUTH_CONT;
    }
    dico_log(L_DEBUG, 0, _("Selected authentication mechanism %s"),
	     mech);
    
    rc = do_gsasl_auth(ctx, conn, mech);

    XDICO_DEBUG_F1(1, "%s\n",
		   rc == 0 ?
		   _("SASL authentication succeeded") :
		   _("SASL authentication failed"));

    /* FIXME */
    return rc == 0 ? AUTH_OK : AUTH_FAIL;
}

int
saslauth(struct dict_connection *conn, dico_url_t url)
{
    int rc = AUTH_FAIL;
    struct auth_cred cred;
    
    switch (auth_cred_get(url->host, &cred)) {
    case GETCRED_OK:
	if (cred.sasl) {
	    rc = saslauth0(conn, &cred);
	    auth_cred_free(&cred);
	} else
	    rc = AUTH_CONT;
	break;

    case GETCRED_FAIL:
	dico_log(L_WARN, 0,
		 _("Not enough credentials for authentication"));
	rc = AUTH_CONT;
	break;

    case GETCRED_NOAUTH:
	XDICO_DEBUG(1, _("Skipping authenitcation\n"));
	rc = AUTH_OK;
    }
    return rc;
}

static int sasl_enable_state = 1;

void
sasl_enable(int val)
{
    sasl_enable_state = 1;
}

int
sasl_enabled_p()
{
    return sasl_enable_state;
}

#else
int
saslauth(struct dict_connection *conn, dico_url_t url)
{
    return AUTH_CONT;
}

void
sasl_enable(int val)
{
    dico_log(L_WARN, 0, _("Dico compiled without SASL support"));
}

int
sasl_enabled_p()
{
    return 0;
}
#endif
