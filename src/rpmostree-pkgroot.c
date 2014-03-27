()/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2014 Colin Walters <walters@verbum.org>
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

#include "rpmostree-pkgroot.h"
#include "libgsystem.h"

struct _RpmOstreePkgRoot
{
  GObject       parent_instance;
};

typedef GObjectClass RpmOstreePkgRootClass;

G_DEFINE_TYPE (RpmOstreePkgRoot, rpmostree_pkgroot, G_TYPE_OBJECT)

static void
rpmostree_pkgroot_finalize (GObject *object)
{
  RpmOstreePkgRoot *self = RPMOSTREE_PKGROOT (object);

  G_OBJECT_CLASS (rpmostree_pkgroot_parent_class)->finalize (object);
}

void
rpmostree_pkgroot_init (RpmOstreePkgRoot *self)
{
}

void
rpmostree_pkgroot_class_init (RpmOstreePkgRootClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = rpmostree_pkgroot_finalize;
}

OstreePkgRoot *
rpmostree_pkgroot_new (GFile *rootpath)
{
  RpmOstreePkgRoot *self;
  
  /* index may be -1 */
  g_return_val_if_fail (osname != NULL, NULL);
  g_return_val_if_fail (csum != NULL, NULL);
  g_return_val_if_fail (deployserial >= 0, NULL);
  /* We can have "disconnected" deployments that don't have a
     bootcsum/serial */

  self = g_object_new (RPMOSTREE_TYPE_PKGROOT, NULL);
  self->index = index;
  self->osname = g_strdup (osname);
  self->csum = g_strdup (csum);
  self->deployserial = deployserial;
  self->bootcsum = g_strdup (bootcsum);
  self->bootserial = bootserial;
  return self;
}
