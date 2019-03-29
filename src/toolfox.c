/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <glib.h>
#include <gio/gio.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define ENV_PAYLOAD "VTE_VERSION"
#define INTERNAL_HELPER  "toolfox-redirect-internal-helper"

gboolean startswith (const char *s1, const char *s2) {
  return strncmp (s1, s2, strlen (s2)) == 0;
}

GFile *get_exports_dir () {
  g_autofree char *path = g_build_filename (g_get_user_data_dir (), "toolfox", "exports", "bin",
                                            NULL);
  return g_file_new_for_path (path);
}

gboolean valid_name (const char *command, GError **error) {
  if (strchr (command, '/') != NULL) {
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Invalid command name: %s",
                 command);
    return FALSE;
  }

  return TRUE;
}

void print_shellcode (const char *shell) {
  gboolean zsh = strcmp (shell, "zsh") == 0;

  g_print ("command_not_found_handle%s () {", zsh ? "r" : "");
  g_print ("  toolfox=\"$HOME/.local/share/toolfox/bin/toolfox\"; ");
  if (zsh)
    g_print ("  echo \"command not found: $1\" >&2; ");
  else
    g_print ("  echo \"bash: $1: command not found\" >&2; ");
  g_print ("  [[ $- =~ i && -f \"$toolfox\" && -z ${COMP_CWORD-} && $HOSTNAME != toolbox ]] ");
  g_print ("    && \"$toolfox\" run which \"$1\" >/dev/null 2>&1 ");
  g_print ("    && echo \"Try running it inside the toolbox via 'toolfox run'.\" ||:;");
  if (zsh)
    g_print ("  return 127; ");
  else
    g_print ("  return 1; ");
  g_print ("}\n");
}

gboolean link_command (const char *command, gboolean force, GError **error) {
  g_autoptr(GFile) exports = get_exports_dir ();

  if (!valid_name (command, error))
    return FALSE;

  if (!g_file_make_directory_with_parents (exports, NULL, error)) {
    if (g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_EXISTS))
      g_clear_error (error);
    else
      return FALSE;
  }

  g_autoptr(GFile) target = g_file_get_child (exports, command);

  if (force) {
    if (!g_file_delete (target, NULL, error)) {
      if (g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        g_clear_error (error);
      else
        return FALSE;
    }
  }

  if (!g_file_make_symbolic_link (target, "../../bin/toolfox", NULL, error))
    return FALSE;

  return TRUE;
}

gboolean unlink_command (const char *command, gboolean force, GError **error) {
  g_autoptr(GFile) exports = get_exports_dir ();
  g_autoptr(GFile) target = g_file_get_child (exports, command);

  if (!valid_name (command, error))
    return FALSE;

  if (!g_file_delete (target, NULL, error)) {
    if (g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND) && force)
      g_clear_error (error);
    else
      return FALSE;
  }

  return TRUE;
}

gboolean setup_run_environment (int argc, char **argv) {
  g_autoptr(GError) error = NULL;

  g_autofree char *self_path = g_file_read_link ("/proc/self/exe", &error);
  if (self_path == NULL) {
    g_printerr ("Failed to find path to self: %s\n", error->message);
    return FALSE;
  }

  g_autofree char *self_dirname = g_path_get_dirname (self_path);
  g_autofree char *internal_path = g_build_filename (self_dirname, INTERNAL_HELPER, NULL);

  if (startswith (internal_path, "/var/home")) {
    char *new_path = g_strdup (internal_path + 4);
    g_free (g_steal_pointer (&internal_path));
    internal_path = g_steal_pointer (&new_path);
  }

  g_auto(GVariantBuilder) args_builder;
  g_variant_builder_init (&args_builder, G_VARIANT_TYPE ("as"));
  for (int i = 0; i < argc; i++)
    g_variant_builder_add (&args_builder, "s", argv[i]);

  g_autoptr(GVariant) payload_v = g_variant_new ("(msas)", g_getenv (ENV_PAYLOAD), &args_builder);
  g_autofree char *payload = g_variant_print (payload_v, FALSE);
  g_autofree char *escaped_payload = g_uri_escape_string (payload, NULL, FALSE);
  g_setenv (ENV_PAYLOAD, escaped_payload, TRUE);

  g_setenv ("SHELL", internal_path, TRUE);

  return TRUE;
}

gboolean run_command (int argc, char **argv, GError **error) {
  int local_errno;

  // Support working transparently when already inside the toolbox.
  if (strcmp (g_get_host_name (), "toolbox") == 0) {
    argv[0] = g_path_get_basename (argv[0]);

    execvp (argv[0], argv);
    // If we're still here, then execvp failed.
    local_errno = errno;
    g_set_error (error, G_IO_ERROR, g_io_error_from_errno (local_errno), "execvp: %s",
                 strerror (local_errno));
    return FALSE;
  }

  if (!setup_run_environment (argc, argv))
    return FALSE;

  execlp ("toolbox", "toolbox", "enter", NULL);
  // If we're still here, then we failed.
  local_errno = errno;
  g_set_error (error, G_IO_ERROR, g_io_error_from_errno (local_errno), "execlp: %s",
               strerror (local_errno));
  return FALSE;
}

int main (int argc, char **argv) {
  g_autoptr(GError) error = NULL;
  gboolean success = TRUE;

  g_autofree char *self_name = g_path_get_basename (argv[0]);
  if (strcmp (self_name, "toolfox") != 0) {
    success = run_command (argc, argv, &error);
    if (!success)
      g_printerr ("%s\n", error->message);
    return !success;
  }

  gboolean opt_force = FALSE;
  GOptionEntry entries[] = {
    { "force", 'f', 0, G_OPTION_ARG_NONE, &opt_force, "For link, overwrite. "
                                                      "For unlink, don't error if not found." },
    { NULL }
  };

  g_autoptr(GOptionContext) ctx = g_option_context_new (
    "- Command runner for toolbox\n"
    "\n"
    "Commands:\n"
    "\n"
    "  link    <NAME>        Create a link to the given command inside your path.\n"
    "  unlink  <NAME>        Undo the link operation.\n"
    "  run     <COMMAND...>  Run a command inside the toolbox."
  );

  g_option_context_add_main_entries (ctx, entries, NULL);
  g_option_context_set_strict_posix (ctx, TRUE);

  if (!g_option_context_parse (ctx, &argc, &argv, &error)) {
    g_printerr ("Error parsing options: %s\n", error->message);
    return 1;
  }

  if (argc < 2) {
    g_printerr ("A command is required.\n");
    return 1;
  }

  if (strcmp (argv[1], "shellcode") == 0) {
    g_return_val_if_fail (1, argc != 3);
    print_shellcode (argv[2]);
  } else if (strcmp (argv[1], "link") == 0) {
    if (argc != 3) {
      g_printerr ("Expected a name for link.\n");
      return 1;
    }

    success = link_command (argv[2], opt_force, &error);
  } else if (strcmp (argv[1], "unlink") == 0) {
    if (argc != 3) {
      g_printerr ("Expected a name for unlink.\n");
      return 1;
    }

    success = unlink_command (argv[2], opt_force, &error);
  } else if (strcmp (argv[1], "run") == 0) {
    if (argc < 3) {
      g_printerr ("Expected a command for run.\n");
      return 1;
    }

    success = run_command (argc - 2, argv + 2, &error);
  } else {
    g_printerr ("Invalid command: %s\n", argv[1]);
    return 1;
  }

  if (!success)
    g_printerr ("%s\n", error->message);

  return !success;
}
