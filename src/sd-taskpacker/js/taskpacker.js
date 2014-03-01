// Copyright (C) 2012,2013 Colin Walters <walters@verbum.org>
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

var TaskPackerIface = '<node> \
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

var SystemdIface = '<node> \
<interface name="org.freedesktop.Systemd"> \
<method name="StartTransientUnit"> \
  <arg type="s" direction="in" /> \
  <arg type="s" direction="in" /> \
  <arg type="a{sv}" direction="in" /> \
  <arg type="a(sa{sv})" direction="in" /> \
  <arg type="o" direction="out" /> \
</method> \
</interface> \
</node> \
';

const SystemdProxyClass = Gio.DBusProxy.makeProxyWrapper(SystemdIface);

const TaskPacker = new Lang.Class({
    Name: 'TaskPacker',

    _init: function(cancellable) {
	this._impl = Gio.DBusExportedObject.wrapJSObject(TaskPackerIface, this);
	this._impl.export(Gio.DBus.system, '/org/verbum/SdTaskPacker');

	this._sdProxy = new SystemdProxyClass(Gio.DBus.system,
					      'org.freedesktop.systemd1',
					      '/org/freedesktop/systemd1');
	this._paths = {};
    },

    StartTask: function(name, parent, scheduletype, exec, rootcwd, params, version) {
	if (!(scheduletype == 'workorder' ||
	      scheduletype == 'batch'))
	    throw new Error("Invalid schedule type: " + scheduletype);

	let fullname;
	if (parent && parent != '')
	    fullname = scheduletype + '-' + parent + '/name';
	else
	    fullname = scheduletype + '-' + name;
	
	let execStart = GLib.Variant.new("a(sasb)", [
	    [ exec[0], [], false ]]);
	let path = this._sdProxy.StartTransientUnit(fullname + '.service', "fail",
						    { 'Description': 'None',
						      'RemainAfterExit': false,
						      'ExecStart': execStart,
						    },
						    []);
	print("Started " + name + " at " + path);
	this._paths[path] = name;
    }
});



