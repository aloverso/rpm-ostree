/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
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

#pragma once

#include <ostree.h>

G_BEGIN_DECLS

#define RPMOSTREE_TYPE_PKGROOT (rpmostree_pkgroot_get_type ())
#define RPMOSTREE_PKGROOT(inst) (G_TYPE_CHECK_INSTANCE_CAST ((inst), RPMOSTREE_TYPE_PKGROOT, RpmOstreePkgRoot))
#define RPMOSTREE_IS_PKGROOT(inst) (G_TYPE_CHECK_INSTANCE_TYPE ((inst), RPMOSTREE_TYPE_PKGROOT))

typedef struct _RpmOstreePkgRoot RpmOstreePkgRoot;

GType rpmostree_pkgroot_get_type (void) G_GNUC_CONST;

RpmOstreePkgRoot * rpmostree_pkgroot_new (GFile   *prefix);

G_END_DECLS

