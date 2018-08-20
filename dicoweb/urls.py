#  This file is part of GNU Dico.
#  Copyright (C) 2008-2010, 2012, 2013, 2014 Wojciech Polak
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

from django.conf import settings
from django.conf.urls import url, include
# Uncomment this when running via runserver
#from django.conf.urls.static import static
from . import views as app_views

urlpatterns = [
    url(r'^$', app_views.index, name='index'),
    url(r'^opensearch\.xml$', app_views.opensearch, name='opensearch'),
]
# Add the following when running via runserver:
#  + static(settings.MEDIA_URL, document_root=settings.MEDIA_ROOT)

