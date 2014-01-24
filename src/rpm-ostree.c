/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2013,2014 Colin Walters <walters@verbum.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "string.h"

#include <ostree.h>
#include "libgsystem.h"

static char **opt_enable_repos;

static GOptionEntry option_entries[] = {
  { "enablerepo", 0, 0, G_OPTION_ARG_STRING_ARRAY, &opt_enable_repos, "Repositories to enable", "REPO" },
  { NULL }
};

static gboolean
replace_nsswitch (GFile         *target_usretc,
                  GCancellable  *cancellable,
                  GError       **error)
{
  gboolean ret = FALSE;
  gs_unref_object GFile *nsswitch_conf =
    g_file_get_child (target_usretc, "nsswitch.conf");
  gs_free char *nsswitch_contents = NULL;
  gs_free char *new_nsswitch_contents = NULL;

  static gsize regex_initialized;
  static GRegex *passwd_regex;

  if (g_once_init_enter (&regex_initialized))
    {
      passwd_regex = g_regex_new ("^(passwd|group):\\s+files(.*)$",
                                  G_REGEX_MULTILINE, 0, NULL);
      g_assert (passwd_regex);
      g_once_init_leave (&regex_initialized, 1);
    }

  nsswitch_contents = gs_file_load_contents_utf8 (nsswitch_conf, cancellable, error);
  if (!nsswitch_contents)
    goto out;

  new_nsswitch_contents = g_regex_replace (passwd_regex,
                                           nsswitch_contents, -1, 0,
                                           "\\1: files altfiles\\2",
                                           0, error);
  if (!new_nsswitch_contents)
    goto out;

  if (!g_file_replace_contents (nsswitch_conf, new_nsswitch_contents,
                                strlen (new_nsswitch_contents),
                                NULL, FALSE, 0, NULL,
                                cancellable, error))
    goto out;

  ret = TRUE;
 out:
  return ret;
}

static char *
strconcat_push_malloced (GPtrArray *malloced,
                         ...) G_GNUC_NULL_TERMINATED;

static char *
strconcat_push_malloced (GPtrArray *malloced,
                         ...)
{
  va_list args;
  GString *buf = g_string_new ("");
  const char *p;
  char *ret;

  va_start (args, malloced);

  while ((p = va_arg (args, const char *)) != NULL)
    g_string_append (buf, p);

  va_end (args);

  ret = g_string_free (buf, FALSE);
  g_ptr_array_add (malloced, buf);
  return buf;
}
    
static gboolean
run_yum (GFile          *yumroot,
         char          **argv,
         const char     *stdin_str,
         GCancellable   *cancellable,
         GError        **error)
{
  GPtrArray *malloced_argv = g_ptr_array_new_with_free_func (g_free);
  GPtrArray *yum_argv = g_ptr_array_new ();
  char **strviter;
  gs_unref_object GSSubprocessContext *context = NULL;
  gs_unref_object GSSubprocess *yum_process = NULL;
  gs_unref_object GFile *reposdir_path = NULL;
  gs_unref_object GFile *tmp_reposdir_path = NULL;
  GOutputStream *stdin_pipe = NULL;
  GMemoryInputStream *stdin_data = NULL;

  g_ptr_array_add (yum_argv, "yum");
  g_ptr_array_add (yum_argv, "-y");
  g_ptr_array_add (yum_argv, "--setopt=keepcache=1");
  g_ptr_array_add (yum_argv, strconcat_push_malloced (malloced_argv,
                                                      "--installroot=",
                                                      gs_file_get_path_cached (yumroot),
                                                      NULL));
  g_ptr_array_add (yum_argv, "--disablerepo=*");
  
  for (strviter = opt_enable_repos; strviter && *strviter; strviter++)
    {
      g_ptr_array_add (yum_argv, strconcat_push_malloced (malloced_argv,
                                                          "--enablerepo=",
                                                          *strviter,
                                                          NULL));
    }
  
  for (strviter = argv; strviter && *strviter; strviter++)
    g_ptr_array_add (yum_argv, *strviter);

  g_ptr_array_add (yum_argv, NULL);

  context = gs_subprocess_context_new ((char**)yum_argv->pdata);
  {
    g_strfreev char **duped_environ = g_get_environ ();

    g_environ_setenv (duped_environ, "OSTREE_KERNEL_INSTALL_NOOP", "1");

    gs_subprocess_context_set_environment (context, duped_environ);
  }

  reposdir_path = g_file_resolve_relative_path (yumroot, "etc/yum.repos.d");
  /* Hideous workaround for the fact that as soon as yum.repos.d
     exists in the install root, yum will prefer it. */
  if (g_file_query_exists (reposdir_path, NULL))
    {
      tmp_reposdir_path = g_file_resolve_relative_path (yumroot, "etc/yum.repos.d.tmp");
      if (!gs_file_rename (reposdir_path, tmp_reposdir_path,
                           cancellable, error))
        goto out;
    }

  if (stdin_str)
    gs_subprocess_context_set_stdin_disposition (context, GS_SUBPROCESS_STREAM_DISPOSITION_PIPE);

  yum_process = gs_subprocess_new (context, cancellable, error);
  if (!yum_process)
    goto out;

  if (stdin_str)
    {
      stdin_pipe = gs_subprocess_get_stdin_pipe (yum_process);
      g_assert (stdin_pipe);
      
      stdin_data = g_memory_input_stream_new_from_data (stdin_str,
                                                        strlen (stdin_str),
                                                        NULL);
      
      if (0 > g_output_stream_splice (stdin_pipe, (GInputStream*)stdin_data,
                                      G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                                      G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                      cancellable, error))
        goto out;
    }

  if (!gs_subprocess_wait_sync_check (yum_process, cancellable, error))
    goto out;

  if (tmp_reposdir_path)
    {
      if (!gs_file_rename (tmp_reposdir_path, reposdir_path,
                           cancellable, error))
        goto out;
    }

  ret = TRUE;
 out:
  return ret;
}

static gboolean
yuminstall (GFile   *yumroot,
            char   **packages,
            GCancellable *cancellable,
            GError      **error)
{
  gboolean ret = FALSE;
  GString *buf = g_string_new ("");
  char **strviter;
  char *yumargs[] = { "shell", NULL };

  g_string_append (buf, "makecache fast\n");

  for (strviter = packages; strviter && *strviter; strviter++)
    {
      const char *package = *strviter;
      if (g_str_has_prefix (package, "@"))
        g_string_append_printf (buf, "group install %s\n", package);
      else
        g_string_append_printf (buf, "install %s\n", package);
    }
  g_string_append (buf, "run\n");

  if (!run_yum (yumroot, yumargs, buf->str,
                cancellable, error))
    goto out;

  ret = TRUE;
 out:
  return ret;
}

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
