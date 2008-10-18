/* This file is part of Dico.
   Copyright (C) 2008 Sergey Poznyakoff

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

dico_list_t dicod_lang_preferences;

static int
cmp_string_ci(const void *a, const void *b)
{
    return c_strcasecmp(a, b);
}

int
dicod_lang_check(dico_list_t list)
{
    return list == NULL || dicod_lang_preferences == NULL
	   || dico_list_intersect_p(dicod_lang_preferences, list,
				    cmp_string_ci);
}

void
dicod_lang(dico_stream_t str, int argc, char **argv)
{
    dico_list_destroy(&dicod_lang_preferences, dicod_free_item, NULL);
    if (argc > 2) {
	int i;
    
	dicod_lang_preferences = xdico_list_create();
	for (i = 2; i < argc; i++)
	    xdico_list_append(dicod_lang_preferences, xstrdup(argv[i]));
    }
    check_db_visibility();
    stream_writez(str, "250 ok - set language preferences\r\n");
}

static int
_display_pref(void *item, void *data)
{
    dico_stream_t str = data;
    stream_writez(str, " ");
    stream_writez(str, item);
    return 0;
}

void
dicod_show_lang_pref(dico_stream_t str, int argc, char **argv)
{
    stream_printf(str, "280 %lu",
		  (unsigned long) dico_list_count(dicod_lang_preferences));
    dico_list_iterate(dicod_lang_preferences, _display_pref, str);
    dico_stream_write(str, "\r\n", 2);
}

void
dicod_show_lang_info(dico_stream_t str, int argc, char **argv)
{
    dico_list_t langlist;
    dicod_database_t *db = find_database(argv[3]);
    if (!db) {
	stream_writez(str,
		      "550 invalid database, use SHOW DB for a list\r\n");
	return;
    } else 
	langlist = dicod_get_database_languages(db);
   
    stream_printf(str, "280 %lu", (unsigned long) dico_list_count(langlist));
    dico_list_iterate(langlist, _display_pref, str);
    dico_stream_write(str, "\r\n", 2);
}

static int
_show_database_lang(void *item, void *data)
{
    dicod_database_t *db = item;
    dico_stream_t str = data;
    dico_list_t langlist = dicod_get_database_languages(db);
    stream_printf(str, "%s %lu", db->name,
		  (unsigned long) dico_list_count(langlist));
    dico_list_iterate(langlist, _display_pref, str);
    dico_stream_write(str, "\r\n", 2);
    return 0;
}

void
dicod_show_lang_db(dico_stream_t str, int argc, char **argv)
{
    size_t count = database_count();
    if (count == 0) 
	stream_printf(str, "554 No databases present\r\n");
    else {
	dico_stream_t ostr;
	
	stream_printf(str, "110 %lu databases present\r\n",
		      (unsigned long) count);
	ostr = dicod_ostream_create(str, NULL, NULL);
	database_iterate(_show_database_lang, ostr);
	dico_stream_close(ostr);
	dico_stream_destroy(&ostr);
	stream_writez(str, ".\r\n");
	stream_writez(str, "250 ok\r\n");
    }
}

void
register_lang()
{
    static struct dicod_command cmd[] = {
	{ "OPTION LANG", 2, DICOD_MAXPARAM_INF, "[list]",
	  "define language preferences",
	  dicod_lang },
	{ "SHOW LANG DB", 3, 3, NULL,
	  "show databases with their language preferences",
	  dicod_show_lang_db },
	{ "SHOW LANG DATABASES", 3, 3, NULL,
	  "show databases with their language preferences",
	  dicod_show_lang_db },
	{ "SHOW LANG INFO", 4, 4, "database",
	  "show language preferences of a database",
	  dicod_show_lang_info },
	{ "SHOW LANG PREF", 3, 3, NULL,
	  "show server language preferences",
	  dicod_show_lang_pref },
	{ NULL }
    };
    dicod_capa_register("lang", cmd, NULL, NULL);
}
