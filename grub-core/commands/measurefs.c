/* measurefs.c - command to calculate FS hash and extend a PCR  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2005,2007,2008  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/device.h>
#include <grub/fs.h>
#include <grub/env.h>
#include <grub/partition.h>
#include <grub/i18n.h>
#include <grub/extcmd.h>
#include <grub/mm.h>

#include <grub/crypto.h>
#include <grub/tpm.h>

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options[] =
  {
    {"pcr", 'p', 0, N_("Select PCR register index to measure into. Default 9"), 0, ARG_TYPE_INT},
    {"hash", 'h', 0, N_("Hash type. Default sha256"), 0, ARG_TYPE_STRING},
    {0, 0, 0, 0, 0, 0}
  };

static grub_err_t
grub_cmd_measurefs (grub_extcmd_context_t ctxt, int argc, char **args)
{
    struct grub_arg_list *state = ctxt->state;
    grub_device_t dev;
    grub_fs_t fs;
    grub_err_t err = GRUB_ERR_NONE;

    char* name = NULL;
    int pcr = GRUB_BINARY_PCR;
    char* hashalg = "sha256";
    GRUB_PROPERLY_ALIGNED_ARRAY (result, GRUB_CRYPTO_MAX_MDLEN);
    char* result_str = NULL;
    int result_len;
    int j;

    if (argc > 0) {
        name = args[0];
    } else {
        err = grub_error (GRUB_ERR_BAD_ARGUMENT, N_("device name expected"));
        goto error_no_close;
    }

    // PCR index
    if (state[0].set) {
        pcr = grub_strtoul (state[0].arg, 0, 10);
    }

    // name of hash algorithm
    if (state[1].set) {
        hashalg = state[1].arg;
    }

    grub_printf("measurefs: Measuring %s into PCR-%d\n", name, pcr);

    dev = grub_device_open(name);

    if (!dev) {
        err = grub_errno;
        goto error_no_close;
    }

    if (dev->disk == NULL && dev->net != NULL) {
        err = grub_error (GRUB_ERR_BAD_DEVICE,  N_("Network devices [`%s'] are not supported"), name);
        goto error;
    }

    fs = grub_fs_probe (dev);

    if (!fs) {
        err = grub_error (GRUB_ERR_BAD_FS,  N_("cannot find a filesystem on `%s'"), name);
        goto error;
    }

    grub_dprintf("measurefs", "FS: %s\n", fs->name);

    if (fs->digest) {
        err = fs->digest(dev, hashalg, &result, &result_len);

        if (err == GRUB_ERR_NONE) {
            // each byte is 2 chars + zero terminator
            result_str = grub_malloc(result_len * 2 + 1);
            if (result_str == NULL) {
                err = GRUB_ERR_OUT_OF_MEMORY;
                goto error;
            }

            for (j = 0; j < result_len; j++)
                grub_snprintf(result_str + j * 2, 3, "%02x", ((grub_uint8_t *) result)[j]);

            err = grub_tpm_measure(result, result_len, pcr, fs->name, result_str);

            grub_free(result_str);
        }
    } else {
        grub_printf("measurefs: FS %s doesn't support digest()\n", fs->name);
    }

error:
    grub_device_close(dev);
error_no_close:
    return err;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(cat)
{
  cmd = grub_register_extcmd ("measurefs", grub_cmd_measurefs, 0,
      N_("DEVICE"), N_("Calculates partition digest and extends specified PCR"),
      options);
}

GRUB_MOD_FINI(cat)
{
  grub_unregister_extcmd (cmd);
}
