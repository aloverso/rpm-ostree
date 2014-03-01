// Copyright (C) 2014 Colin Walters <walters@verbum.org>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the
// Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Lang = imports.lang;

const GSystem = imports.gi.GSystem;
const JsUtil = imports.jsutil;

var SdTaskPackerIface = '<node> \
<interface name="org.verbum.SdTaskPacker"> \
<method name="StartTask"> \
    <arg name="name" type="s" direction="in" /> \
    <arg name="parent" type="s" direction="in" /> \
    <arg name="scheduletype" type="s" direction="in" /> \
    <arg name="exec" type="as" direction="in" /> \
    <arg name="rootcwd" type="s" direction="in" /> \
    <arg name="params" type="a{sv}" direction="in" /> \
    <arg name="version" type="s" direction="out" /> \
</method> \
<method name="GetState"> \
    <arg type="u" direction="out" /> \
    <arg type="u" direction="out" /> \
</method> \
</interface> \
</node>';

const SdTaskPackerClass = Gio.DBusProxy.makeProxyWrapper(SdTaskPackerIface);

var proxy = new SdTaskPackerClass(Gio.DBus.system,
				  'org.verbum.SdTaskPacker',
				  '/org/verbum/SdTaskPacker');

let loop = GLib.MainLoop.new(null, true);

proxy.StartTask('hello', '', 'workorder',
		['echo', 'hello', 'world'], '/tmp',
		[],
		'20140228.0')


