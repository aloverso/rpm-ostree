#!/usr/bin/env python
#
# Copyright (C) 2012,2013 Colin Walters <walters@verbum.org>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

import os
import re
import sys
import optparse
import time
import shutil
import subprocess

from gi.repository import GLib
from gi.repository import Gio

os_release_data = {}
opts = None
args = None

def log(msg):
    sys.stdout.write(msg)
    sys.stdout.write('\n')
    sys.stdout.flush()

def ensuredir(path):
    if not os.path.isdir(path):
        os.makedirs(path)

def rmrf(path):
    shutil.rmtree(path, ignore_errors=True)

def feed_checksum(checksum, stream):
    b = stream.read(8192)
    while b != '':
        checksum.update(b)
        b = stream.read(8192)

def _find_current_origin_refspec():
    dpath = '/ostree/deploy/%s/deploy' % (os_release_data['ID'], )
    for name in os.listdir(dpath):
        if name.endswith('.origin'):
            for line in open(os.path.join(dpath, name)):
                if line.startswith('refspec='):
                    return line[len('refspec='):]
    return None

def replace_nsswitch(target_usretc):
    nsswitch_conf = os.path.join(target_usretc, 'nsswitch.conf')
    f = open(nsswitch_conf)
    newf = open(nsswitch_conf + '.tmp', 'w')
    passwd_re = re.compile(r'^passwd:\s+files(.*)$')
    group_re = re.compile(r'^group:\s+files(.*)$')
    for line in f:
        match = passwd_re.match(line)
        if match and line.find('altfiles') == -1:
            newf.write('passwd: files altfiles' + match.group(1) + '\n')
            continue
        match = group_re.match(line)
        if match and line.find('altfiles') == -1:
            newf.write('group: files altfiles' + match.group(1) + '\n')
            continue
        newf.write(line)
    f.close()
    newf.close()
    os.rename(nsswitch_conf + '.tmp', nsswitch_conf)
    
def runyum(argv, yumroot, stdin_str=None):
    yumargs = list(['yum', '-y', '--releasever=%s' % (opts.os_version, ), '--nogpg', '--setopt=keepcache=1', '--installroot=' + yumroot, '--disablerepo=*'])
    yumargs.extend(map(lambda x: '--enablerepo=' + x, opts.enablerepo))
    yumargs.extend(argv)
    log("Running: %s" % (subprocess.list2cmdline(yumargs), ))
    if stdin_str:
        log("%s" % (stdin_str, ))
    yum_env = dict(os.environ)
    yum_env['KERNEL_INSTALL_NOOP'] = 'yes'
    reposdir_path = os.path.join(yumroot, 'etc', 'yum.repos.d')
    # Hideous workaround for the fact that as soon as yum.repos.d
    # exists in the install root, yum will prefer it.
    tmp_reposdir_path = None
    if os.path.isdir(reposdir_path):
        tmp_reposdir_path = os.path.join(yumroot, 'etc', 'yum.repos.d.tmp')
        os.rename(reposdir_path, tmp_reposdir_path)
    stdin_arg=None
    if stdin_str is not None:
        stdin_arg = subprocess.PIPE
    proc = subprocess.Popen(yumargs, env=yum_env, stdin=stdin_arg)
    proc.communicate(input=stdin_str)
    if tmp_reposdir_path is not None:
        os.rename(tmp_reposdir_path, reposdir_path)
    if proc.returncode != 0:
        raise ValueError("Yum exited with code %d" % (proc.returncode, ))

def yuminstall(yumroot, packages):
    cmds = ['makecache fast']
    for package in packages:
        if package.startswith('@'):
            cmds.append('group install ' + package)
        else:
            cmds.append('install ' + package)
    cmds.append('run')
    stdin = '\n'.join(cmds) + '\n'
    runyum(['shell'], yumroot, stdin_str=stdin)

def proc_wait_check(proc):
    retcode = proc.wait()
    if retcode:
        raise subprocess.CalledProcessError(retcode, str(proc))

def main():
    parser = optparse.OptionParser('%prog ACTION PACKAGE1 [PACKAGE2...]')
    parser.add_option('', "--workdir",
                      action='store', dest='workdir',
                      default=os.getcwd(),
                      help="Path to working directory (default: cwd)")
    parser.add_option('', "--deploy",
                      action='store_true',
                      default=False,
                      help="Do a deploy if true")
    parser.add_option('', "--breakpoint",
                      action='store',
                      default=None,
                      help="Stop at given phase")
    parser.add_option('', "--name",
                      action='store',
                      default=None,
                      help="Use NAME as ref name")
    parser.add_option('', "--os",
                      action='store', dest='os',
                      default=None,
                      help="OS Name (default from /etc/os-release)")
    parser.add_option('', "--os-version",
                      action='store', dest='os_version',
                      default=None,
                      help="OS version (default from /etc/os-release)")
    parser.add_option('', "--enablerepo",
                      action='append', dest='enablerepo',
                      default=[],
                      help="Enable this yum repo")
    parser.add_option('', "--local-ostree-package",
                      action='store', dest='local_ostree_package',
                      default='ostree',
                      help="Path to local OSTree RPM")

    global opts
    global args
    (opts, args) = parser.parse_args(sys.argv[1:])

    os.chdir(opts.workdir)

    if (opts.deploy and
        (opts.os is None or
        opts.os_version is None)):
        f = open('/etc/os-release')
        for line in f.readlines():
            if line == '': continue
            (k,v) = line.split('=', 1)
            os_release_data[k.strip()] = v.strip()
        f.close()

        if opts.os is None:
            opts.os = os_release_data['ID']
        if opts.os_version is None:
            opts.os_version = os_release_data['VERSION_ID']

    log("Targeting os=%s version=%s" % (opts.os, opts.os_version))

    action = args[0]
    if action == 'create':
        ref = args[1]
        packages = args[2:]
        commit_message = 'Commit of %d packages' % (len(packages), )
    else:
        print >>sys.stderr, "Unknown action %s" % (action, )
        sys.exit(1)

    cachedir = os.path.join(opts.workdir, 'cache')
    ensuredir(cachedir)

    yumroot = os.path.join(cachedir, 'yum')
    targetroot = os.path.join(cachedir, 'rootfs')
    yumcachedir = os.path.join(yumroot, 'var/cache/yum')
    yumcache_lookaside = os.path.join(cachedir, 'yum-cache')
    logs_lookaside = os.path.join(cachedir, 'logs')

    rmrf(logs_lookaside)
    ensuredir(logs_lookaside)

    shutil.rmtree(yumroot, ignore_errors=True)
    yumroot_varcache = os.path.join(yumroot, 'var/cache')
    if os.path.isdir(yumcache_lookaside):
        log("Reusing cache: " + yumroot_varcache)
        ensuredir(yumroot_varcache)
        subprocess.check_call(['cp', '-a', yumcache_lookaside, yumcachedir])
    else:
        log("No cache found at: " + yumroot_varcache)

    # Ensure we have enough to modify NSS
    yuminstall(yumroot, ['filesystem', 'glibc', 'nss-altfiles', 'shadow-utils'])

    # Prepare NSS configuration; this needs to be done
    # before any invocations of "useradd" in %post
    for n in ['passwd', 'group']:
        open(os.path.join(yumroot, 'usr/lib', n), 'w').close()
    replace_nsswitch(os.path.join(yumroot, 'etc'))

    if opts.breakpoint == 'post-yum-phase1':
        return
    
    yuminstall(yumroot, packages)

    ref_unix = ref.replace('/', '_')

    # Attempt to cache stuff between runs
    rmrf(yumcache_lookaside)
    log("Saving yum cache " + yumcache_lookaside)
    os.rename(yumcachedir, yumcache_lookaside)

    yumroot_rpmlibdir = os.path.join(yumroot, 'var/lib/rpm')
    rpmtextlist = os.path.join(cachedir, 'packageset-' + ref_unix + '.txt')
    rpmtextlist_new = rpmtextlist + '.new'
    rpmqa_proc = subprocess.Popen(['rpm', '-qa', '--dbpath=' + yumroot_rpmlibdir],
                                  stdout=subprocess.PIPE)
    sort_proc = subprocess.Popen(['sort'], stdin=rpmqa_proc.stdout,
                                 stdout=open(rpmtextlist_new, 'w'))
    proc_wait_check(rpmqa_proc)
    proc_wait_check(sort_proc)

    differs = True
    if os.path.exists(rpmtextlist):
        log("Comparing diff of previous tree")
        rcode = subprocess.call(['diff', '-u', rpmtextlist, rpmtextlist_new])
        if rcode == 0:
            differs = False
        elif rcode == 1:
            differs = True
        else:
            raise subprocess.CalledProcessError(rcode, "diff")

    if not differs:
        log("No changes in package set")
        return

    os.rename(rpmtextlist_new, rpmtextlist)

    if opts.breakpoint == 'post-yum-phase2':
        return

    argv = ['rpm-ostree-postprocess-and-commit',
            '--repo=' + os.path.join(opts.workdir, 'repo'),
            '-m', commit_message,
            yumroot,
            ref]
    log("Running: %s" % (subprocess.list2cmdline(argv), ))
    subprocess.check_call(argv)

    log("Complete")
