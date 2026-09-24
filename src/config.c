/*
 * config.c — flat dotted-key TOML-subset reader shared by the package
 * manager, engine proxy, and task runner.
 *
 * Recognizes `# comments`, `[section]` and `[dotted.section]` headers,
 * and `key = value` pairs (string-quoted or bare). Entries are stored
 * flattened: `key = value` under `[tasks.pulp]` becomes the dotted key
 * "tasks.pulp.build".
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/compiler.h"

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
                       s[len - 1] == '\r' || s[len - 1] == '\n'))
        s[--len] = '\0';
    return s;
}

static void add_entry(PithConfig *cfg, const char *key, const char *val)
{
    if (cfg->count >= PITH_CONFIG_MAX_ENTRIES)
        return;
    PithConfigEntry *e = &cfg->entries[cfg->count++];
    snprintf(e->key, sizeof(e->key), "%s", key);
    snprintf(e->value, sizeof(e->value), "%s", val);
}

int pith_config_load(const char *path, PithConfig *out)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;

    memset(out, 0, sizeof(*out));

    char line[512];
    char section[PITH_CONFIG_KEY_MAX] = "";

    while (fgets(line, sizeof(line), fp)) {
        char *s = trim(line);
        if (!*s || *s == '#')
            continue;

        if (*s == '[') {
            char *close = strchr(s, ']');
            if (!close) {
                fclose(fp);
                return -2;   /* malformed section header */
            }
            *close = '\0';
            snprintf(section, sizeof(section), "%s", trim(s + 1));
            continue;
        }

        char *eq = strchr(s, '=');
        if (!eq) {
            fclose(fp);
            return -2;   /* malformed key/value line */
        }
        *eq = '\0';
        char *key = trim(s);
        char *val = trim(eq + 1);

        size_t vlen = strlen(val);
        if (vlen >= 2 && val[0] == '"' && val[vlen - 1] == '"') {
            val[vlen - 1] = '\0';
            val++;
        }

        char dotted[PITH_CONFIG_KEY_MAX];
        if (section[0])
            snprintf(dotted, sizeof(dotted), "%s.%s", section, key);
        else
            snprintf(dotted, sizeof(dotted), "%s", key);

        add_entry(out, dotted, val);
    }

    fclose(fp);
    return 0;
}

const char *pith_config_get(const PithConfig *cfg, const char *dotted_key)
{
    for (size_t i = 0; i < cfg->count; i++)
        if (strcmp(cfg->entries[i].key, dotted_key) == 0)
            return cfg->entries[i].value;
    return NULL;
}
