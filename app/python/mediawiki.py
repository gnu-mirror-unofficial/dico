# -*- coding: utf-8 -*-
# Mediawiki module for Python 
# This file is part of GNU Dico.
# Copyright (C) 2008-2010, 2012 Wojciech Polak
# Copyright (C) 2018-2019 Sergey Poznyakoff
#
# GNU Dico is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Dico is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Dico.  If not, see <http://www.gnu.org/licenses/>.

from __future__ import print_function
import sys
import re
import socket
from xml.dom import minidom
from wikitrans.wiki2text import TextWiktionaryMarkup

if sys.version_info[0] > 2:
    from urllib.request import urlopen, Request
    from urllib.error import URLError
    from urllib.parse import quote as url_quote
    from html.entities import name2codepoint
    class unicode(str):
        pass
    def unichr(c):
        return chr(c)
else:
    from urllib2 import urlopen, Request, quote as url_quote, URLError
    from htmlentitydefs import name2codepoint
    # Set utf-8 as the default encoding. 
    # Trying to do so using encode('utf_8')/unicode, which is 
    # supposed to be the right way, does not work.
    # Simply calling sys.setdefaultencoding is not possible,
    # because, for some obscure reason, Python chooses to delete 
    # this symbol from the namespace after setting its default 
    # encoding in site.py. That's why reload is needed. 
    reload(sys)
    sys.setdefaultencoding('utf-8')

try:
    import json
except ImportError:
    import simplejson as json

import dico

__version__ = '1.03'

class DicoModule:
    user_agent = 'Mozilla/1.0'
    endpoint_match  = '/w/api.php?action=opensearch&format=json&search='
    endpoint_define = '/wiki/Special:Export/'

    def __init__ (self, *argv):
        self.wikihost = argv[0]
        socket.setdefaulttimeout (4)
        dico.register_markup ('wiki')

    def open (self, dbname):
        self.dbname = dbname
        return True

    def close (self):
        return True

    def descr (self):
        return self.wikihost

    def info (self):
        return False

    def define_word (self, word):
        url = 'http://%s%s%s' % (self.wikihost, self.endpoint_define,
                                 url_quote (word))
        req = Request (url)
        req.add_header ('User-Agent', self.user_agent)
        try:
            xml = urlopen (req).read ()
        except URLError:
            return False
        dom = minidom.parseString (xml)
        el = dom.getElementsByTagName ('text')
        if len (el):
            data = el[0].firstChild.data
            if dico.current_markup () != 'wiki':
                data = self.__htmlentitydecode (data)
                if sys.version_info[0] == 2:
                    data = data.encode ('utf-8')
                wikiparser = TextWiktionaryMarkup (text=data)
                wikiparser.parse ()
                data = str (wikiparser)
            return ['define', data]
        else:
            return False

    def match_word (self, strat, key):
        url = 'http://%s%s%s' % (self.wikihost, self.endpoint_match,
                                 url_quote (key.word))
        req = Request (url)
        req.add_header ('User-Agent', self.user_agent)
        try:
            result = json.load (urlopen (req))
            if result:
                if strat.has_selector:
                    fltres = []
                    for k in result[1]:
                        if strat.select (k, key):
                            fltres.append (k)
                    if len(fltres) > 0:
                        return ['match', sorted(fltres, key=unicode.lower)]
                else:
                    result[1].sort ()
                    return ['match', sorted(result[1], key=unicode.lower)]
            return False
        except URLError:
            return False

    def output (self, rh, n):
        if rh[0] == 'define':
            print(rh[1], end='')
        else:
            list = rh[1]
            sys.stdout.softspace = 0
            print(list[n], end='')
        return True

    def result_count (self, rh):
        if rh[0] == 'define':
            return 1
        else:
            return len (rh[1])

    def compare_count (self, rh):
        return 1

    def result_headers (self, rh, hdr):
        if dico.current_markup () != 'wiki':
            hdr['Content-Type'] = 'text/plain';
        elif '.wikipedia.org' in self.wikihost:
            hdr['Content-Type'] = 'text/x-wiki-wikipedia';
        elif '.wiktionary.org' in self.wikihost:
            hdr['Content-Type'] = 'text/x-wiki-wiktionary';
        else:
            hdr['Content-Type'] = 'text/x-wiki';
        return hdr

    def free_result (self, rh):
        pass

    def __htmlentitydecode (self, s):
        return re.sub ('&(%s);' % '|'.join (name2codepoint),
                       lambda m: unichr (name2codepoint[m.group (1)]), s)
