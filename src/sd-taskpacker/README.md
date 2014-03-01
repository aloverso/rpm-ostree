SD-TaskPacker
=============

The SD is short for systemd.  The goal here is something pretty simple
- we want a scheduler on top of systemd, that chooses what services to
spawn.  Systemd is good at managing services, we need something that
priortizes among competing demands for execution.

In other words, we have more work to do than resources - something
needs to choose.  If we just overcommit then everything would run
very, very slowly at best, if we didn't OOM.

Implementatation
----------------

Let's make a fundamental assumption that we can't preempt/cancel
running tasks.  Things could obviously be a lot more interesting if we
had the ability to do so.

Now, we have a fixed amount of memory allocated to us (specified
by the toplevel MemoryLimit= on our unit file), and a fixed number
of CPU cores.

Memory is divided into 'chunks' of 32MiB each.

There are two classes of tasks, kind of like what Google describes in
Omega - "workorder" (equivalent to their "service", except renamed to
avoid confusion with systemd's .service) and "batch".  Workorders we
*always* have to run.  Batch are best effort.

Task
----

A task has two core properties:
- name
- version

There is a set of "root" tasks that are started when sd-taskpacker
starts.  Each root task has a directory - all state will be stored
relative to that directory.

Any task can request execution of other tasks - then its final name is
a child of the parent.

Any task (including a root) can be "decomissioned", which means that
no further tasks in that root will be executed.

The raw implementation of a task is simply calling into systemd's
StartTransientUnit(), passing it the binary name and arguments/cmdline
data.

We'll allocate a working directory for it after its name and version.
A directory results/ will be pre-created, and preserved.  Inside that
directory, a file output.txt will hold stdout/stderr.  Any other files
will be garbage collected after the task terminates.

Tasks are defined like systemd units:

[Workorder]
Exec=/usr/bin/rpm-ostree-autobuilder task build

in /usr/share/sd-taskpacker/units/build.workorder

[Batch]
Exec=/usr/bin/rpm-ostree-autobuilder task generate-disk-graph

Now, tasks have arguments - these might be appended to the Exec= line,
or we may pass a parameters.json.

Workorder tasks
---------------

Let's keep this simple for now - we don't attempt any actual
scheduling of them for now, we just overcommit.  They're started as
soon as we get a request to do so.

Batch tasks
-----------

These tasks have a few additional properties:

- requested priority (uint32?)
- deadline (wall clock/monotonic?)
- size: tiny/small/medium/large - say approx 32MiB, 128MiB, 256MiB, 1GiB of RAM
- parameters
  This is just key/value pairs where the values fit into a well-known schema.
  For example, there might be a 'massif' task that takes a parameter 'target'
  which is one of ['/usr/bin/gnome-shell', '/usr/bin/systemd']

For scheduling batch tasks, we look at our current resources, inspect
the requested priority, target deadline, size, and attempt to run
whatever batch task best matches our criteria.

API:
  method BatchSubmit(a(s name, a{sv} args, a{sv} params))
    -> a(as started, as rejected)

  signal BatchComplete(a name, a{sv} parameters, au estimatedSlotsRemaining)

  signal BatchFailed(a name, a{sv} parameters, au estimatedSlotsRemaining)


A client requests a set of batch task instances, and we respond with a
subset that we started, plus an estimate of remaining capacity (if
any).


Distribution
============

The easiest way to distribute would be to add a message queue, and
have sd-taskpacker instances on other nodes discover state changes
through the queue.  Or some other form of shared state.

But I think we may want to just keep it simple.
