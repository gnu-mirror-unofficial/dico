#  This file is part of GNU Dico.
#  Copyright (C) 2008-2010, 2012-2015 Wojciech Polak
#
#  GNU Dico is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3, or (at your option)
#  any later version.
#
#  GNU Dico is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with GNU Dico.  If not, see <http://www.gnu.org/licenses/>.

import re
import hashlib
import socket
from django.conf import settings
try:
    from django.core.urlresolvers import reverse
except ImportError:
    from django.urls import reverse
from django.core.cache import cache
from django.shortcuts import render_to_response
from django.utils.encoding import force_bytes
from django.utils.translation import ugettext as _

from .dicoclient import dicoclient
try:
    from wikitrans import wiki2html
except ImportError:
    wiki2html = None
    print('WARNING: wikitrans is not installed.')

def onerror(err, dfl=None):
    try:
        act = settings.ONERROR[err]
    except:
        act = dfl
    return act

def index(request):
    page = {
        'robots': 'index',
    }
    selects = {}
    mtc = {}
    result = {}
    markup_style = 'default'
    port = 2628

    sid = request.COOKIES.get('dicoweb_sid', '')
    request.session.set_expiry(0)

    accept_lang = request.META.get('HTTP_ACCEPT_LANGUAGE', '').split(',')
    for i, lang in enumerate(accept_lang):
        accept_lang[i] = lang.split(';')[0]

    if 'server' in request.session:
        server = request.session['server']
    else:
        server = settings.DICT_SERVERS[0]
    server = request.GET.get('server', server)
    if not server in settings.DICT_SERVERS:
        server = settings.DICT_SERVERS[0]
    request.session['server'] = server

    if len(settings.DICT_SERVERS) > 1:
        selects['sv'] = HtmlOptions(settings.DICT_SERVERS, server)

    key = hashlib.md5(force_bytes(
        '%s/%s' % (sid, server.encode('ascii', 'backslashreplace'))))
    sid = key.hexdigest()

    type = 'search'
    if 'define' in request.GET:
        type = 'define'
    else:
        type = 'search'

    database = request.GET.get('db', '*')
    strategy = request.GET.get('strategy', '.')

    key_databases = str('dicoweb/databases/' + server)
    key_strategies = str('dicoweb/strategies/' + server)

    databases = cache.get(key_databases)
    strategies = cache.get(key_strategies)

    if server.find(':') != -1:
        s = server.split(':', 1)
        server = s[0]
        port = int(s[1])

    if not databases or not strategies:
        dc = dicoclient.DicoClient()
        try:
            dc.open(server, port)
            dc.timeout = settings.DICT_TIMEOUT
            databases = dc.show_databases()['databases']
            strategies = dc.show_strategies()['strategies']
            dc.close()
        except (socket.timeout, socket.error, dicoclient.DicoNotConnectedError):
            return render_to_response('index.html', {'selects': selects})

        cache.set(key_databases, databases, timeout=86400)
        cache.set(key_strategies, strategies, timeout=86400)

    for s in strategies:
        s[1] = _(s[1])
    databases.insert(0, ['!', _('First match')])
    databases.insert(0, ['*', _('All')])
    strategies.insert(0, ['.', _('Default')])

    selects['db'] = HtmlOptions(databases, database)
    selects['st'] = HtmlOptions(strategies, strategy)

    q = request.GET.get('q', '')

    if 'q' in request.GET and q != '':
        langkey = '*'
        if database == '*':
            langkey = ','.join(accept_lang)

        key = hashlib.md5(force_bytes(
            '%s:%d/%s/%s/%s/%s/%s' % (server, port, langkey, type,
                                      database, strategy,
                                      q.encode('ascii', 'backslashreplace'))))
        key = key.hexdigest()
        result = cache.get('dicoweb/' + key)

        if not result:
            try:
                dc = dicoclient.DicoClient()
                dc.timeout = settings.DICT_TIMEOUT
                dc.open(server, port)
                dc.option('MIME')

                if database == '*' and 'lang' in dc.server_capas:
                    dc.option('LANG', ': ' + ' '.join(accept_lang))
                if 'markup-wiki' in dc.server_capas:
                    if dc.option('MARKUP', 'wiki'):
                        markup_style = 'wiki'

                if database == 'dbinfo':
                    result = dc.show_info(q)
                elif type == 'define':
                    result = dc.define(database, q)
                else:
                    result = dc.match(database, strategy, q)
                dc.close()

                result['markup_style'] = markup_style
                cache.set('dicoweb/' + key, result, timeout=3600)

            except (socket.timeout, socket.error,
                    dicoclient.DicoNotConnectedError):
                return render_to_response('index.html',
                                          {'selects': selects})

        # get last match results
        if sid and type == 'search':
            cache.set('dicoweb/%s/last_match' % sid, key, timeout=3600)
        else:
            key = cache.get('dicoweb/%s/last_match' % sid)

        if key != None:
            mtc = cache.get('dicoweb/' + key)
        if not mtc:
            mtc = result

        mtc['dbnames'] = {}
        if 'matches' in mtc:
            for m in mtc['matches']:
                for d in databases:
                    if d[0] == m:
                        mtc['dbnames'][m] = d[1]
                        break

        if database == 'dbinfo':
            q = ''
        if q != '':
            page['title'] = q + ' - '
            page['robots'] = 'noindex,nofollow'

    if 'definitions' in result:
        rx1 = re.compile('{+(.*?)}+', re.DOTALL)
        for i, df in enumerate(result['definitions']):
            if 'content-type' in df:
                if (df['content-type'].startswith('text/x-wiki')
                    and wiki2html):
                    lang = df['x-wiki-language'] \
                          if 'x-wiki-language' in df else 'en'
                    wikiparser = wiki2html.HtmlWiktionaryMarkup(
                                          text=df['desc'],
                                          html_base='?q=',
                                          lang=lang)
                    wikiparser.parse()
                    df['desc'] = str(wikiparser)
                    df['format_html'] = True
                elif df['content-type'].startswith('text/html'):
                    df['format_html'] = True
                elif df['content-type'].startswith('text/'):
                    df['format_html'] = False
                else:
                    act = onerror('UNSUPPORTED_CONTENT_TYPE',
                                  { 'action': 'replace',
                                    'message': 'Article cannot be displayed due to unsupported content type' })
                    if act['action'] == 'delete':
                        del(result['definitions'][i])
                        result['count'] -= 1
                    elif act['action'] == 'replace':
                        df['desc'] = act['message']
                        df['format_html'] = (
                            act['format_html'] if 'format_html' in act
                            else False)
                        result['definitions'][i] = df
                    elif act['action'] == 'display':
                        df['format_html'] = (
                            act['format_html'] if 'format_html' in act
                            else False)
                        result['definitions'][i] = df
                    elif settings.DEBUG:
                        raise AssertionError("""
Article cannot be displayed due to unsupported content type (%s).
Additionally, ONERROR['UNSUPPORTED_CONTENT_TYPE'] has unsupported value (%s).
""" % (df['content-type'], act))
                    else:
                        del(result['definitions'][i])
                        result['count'] -= 1
            else:
                df['desc'] = re.sub('_(.*?)_', '<b>\\1</b>', df['desc'])
                df['desc'] = re.sub(rx1, __subs1, df['desc'])
        if result['count'] == 0:
            result = { 'error': 552, 'msg': 'No match' }

    return render_to_response('index.html', {'page': page,
                                             'q': q,
                                             'mtc': mtc,
                                             'result': result,
                                             'selects': selects, })


def opensearch(request):
    url_query = request.build_absolute_uri(reverse('index'))
    url_media = request.build_absolute_uri(settings.MEDIA_URL)
    return render_to_response('opensearch.xml', {'url_query': url_query,
                                                 'url_media': url_media},
                              content_type='application/xml')


def __subs1(match):
    s = re.sub(r' +', ' ', match.group(1))
    return '<a href="?q=%s" title="Search for %s">%s</a>' \
        % (s.replace('\n', ''), s.replace('\n', ''), s)


class HtmlOptions:
    def __init__(self, lst=[], value=''):
        self.lst = lst
        self.value = value

    def html(self):
        buf = []
        for opt in self.lst:
            if len(opt) == 2:
                if not opt[1]:
                    opt[1] = opt[0]
                try:
                    opt[1] = opt[1].decode('utf-8')
                except:
                    pass
                if opt[0] == self.value:
                    buf.append('<option value="%s" selected="selected">%s</option>' % (
                        opt[0], opt[1]))
                else:
                    buf.append('<option value="%s">%s</option>' % (opt[0],
                                                                   opt[1]))
            else:
                if opt == self.value:
                    buf.append(
                        '<option value="%s" selected="selected">%s</option>' % (opt, opt))
                else:
                    buf.append('<option value="%s">%s</option>' % (opt, opt))
        return '\n'.join(buf)
