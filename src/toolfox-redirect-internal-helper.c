/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <glib.h>
#include <stdio.h>
#include <unistd.h>

int main () {
  g_autoptr(GError) error = NULL;
  g_autofree char *payload = g_uri_unescape_string (g_getenv ("VTE_VERSION"), NULL);
  g_autoptr(GVariant) payload_v = g_variant_parse (G_VARIANT_TYPE ("(msas)"),
                                                   payload, NULL, NULL, &error);

  const char *orig_env = NULL;
  g_autofree const char **forwarded_argv = NULL;
  g_variant_get_child (payload_v, 0, "m&s", &orig_env);
  g_variant_get_child (payload_v, 1, "^a&s", &forwarded_argv);

  g_autofree char *argv0 = g_path_get_basename (forwarded_argv[0]);
  forwarded_argv[0] = argv0;

  execvp (forwarded_argv[0], (char * const *) forwarded_argv);
  // If we're still here, then we failed.
  perror ("execlp");
  return 1;
}
