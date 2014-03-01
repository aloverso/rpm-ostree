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

const TaskPacker = imports.taskpacker;

let loop = GLib.MainLoop.new(null, true);
let cancellable = null;

let ecode = 1;
let bus = Gio.DBus.system.own_name('org.verbum.SdTaskPacker',
				   Gio.BusNameOwnerFlags.NONE,
				   function(name) { print("Acquired bus name") },
				   function(name) { loop.quit(); });

try {
    let taskpacker = new TaskPacker.TaskPacker(cancellable);
    print("Awaiting requests");
    loop.run();
    ecode = 0;
} finally {
    loop.quit();
}
loop.run();
ecode;

    
    
