/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_M_CONFIG_H
#define MPLAYER_M_CONFIG_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "misc/bstr.h"

// m_config provides an API to manipulate the config variables in MPlayer.
// It makes use of the Options API to provide a context stack that
// allows saving and later restoring the state of all variables.

typedef struct m_profile m_profile_t;
struct m_option;
struct m_option_type;
struct m_sub_options;
struct m_obj_desc;
struct m_obj_settings;
struct mp_log;

// Config option
struct m_config_option {
    bool is_hidden : 1;             // Does not show up in help
    bool is_set_from_cmdline : 1;   // Set by user from command line
    bool is_set_locally : 1;        // Has a backup entry
    bool warning_was_printed : 1;
    int16_t shadow_offset;          // Offset into m_config_shadow.data
    int16_t group;                  // Index into m_config.groups
    const char *name;               // Full name (ie option-subopt)
    const struct m_option *opt;     // Option description
    void *data;                     // Raw value of the option
    const void *default_data;       // Raw default value
};

// Config object
/** \ingroup Config */
typedef struct m_config {
    struct mp_log *log;
    struct mpv_global *global; // can be NULL

    // Registered options.
    struct m_config_option *opts; // all options, even suboptions
    int num_opts;

    // Creation parameters
    size_t size;
    const void *defaults;
    const struct m_option *options;

    // List of defined profiles.
    struct m_profile *profiles;
    // Depth when recursively including profiles.
    int profile_depth;

    struct m_opt_backup *backup_opts;

    bool use_profiles;
    bool is_toplevel;
    int (*includefunc)(void *ctx, char *filename, int flags);
    void *includefunc_ctx;

    // For the command line parser
    int recursion_depth;

    bool subopt_deprecation_warning;

    void *optstruct; // struct mpopts or other

    int shadow_size;

    // List of m_sub_options instances.
    // Index 0 is the top-level and is always present.
    struct m_config_group *groups;
    int num_groups;

    // Thread-safe shadow memory; only set for the main m_config.
    struct m_config_shadow *shadow;
} m_config_t;

// Create a new config object.
//  talloc_ctx: talloc parent context for the m_config allocation
//  size: size of the optstruct (where option values are stored)
//        size==0 creates a config object with no option struct allocated
//  defaults: if not NULL, points to a struct of same type as optstruct, which
//            contains default values for all options
//  options: list of options. Each option defines a member of the optstruct
//           and a corresponding option switch or sub-option field.
//  suboptinit: if not NULL, initialize the suboption string (used for presets)
// Note that the m_config object will keep pointers to defaults and options.
struct m_config *m_config_new(void *talloc_ctx, struct mp_log *log,
                              size_t size, const void *defaults,
                              const struct m_option *options);

// Creates "backup" shadow memory for use with m_config_cache. Sets it on
// mpv_global. Expected to be called at early init on the main m_config.
void m_config_create_shadow(struct m_config *config);

// (Warning: new object references config->log and others.)
struct m_config *m_config_dup(void *talloc_ctx, struct m_config *config);

struct m_config *m_config_from_obj_desc(void *talloc_ctx, struct mp_log *log,
                                        struct m_obj_desc *desc);

struct m_config *m_config_from_obj_desc_noalloc(void *talloc_ctx,
                                                struct mp_log *log,
                                                struct m_obj_desc *desc);

struct m_config *m_config_from_obj_desc_and_args(void *ta_parent,
    struct mp_log *log, struct mpv_global *global, struct m_obj_desc *desc,
    const char *name, struct m_obj_settings *defaults, char **args);

// Make sure the option is backed up. If it's already backed up, do nothing.
// All backed up options can be restored with m_config_restore_backups().
void m_config_backup_opt(struct m_config *config, const char *opt);

// Call m_config_backup_opt() on all options.
void m_config_backup_all_opts(struct m_config *config);

// Restore all options backed up with m_config_backup_opt(), and delete the
// backups afterwards.
void m_config_restore_backups(struct m_config *config);

enum {
    M_SETOPT_PRE_PARSE_ONLY = 1,    // Silently ignore non-M_OPT_PRE_PARSE opt.
    M_SETOPT_CHECK_ONLY = 2,        // Don't set, just check name/value
    M_SETOPT_FROM_CONFIG_FILE = 4,  // Reject M_OPT_NOCFG opt. (print error)
    M_SETOPT_FROM_CMDLINE = 8,      // Mark as set by command line
    M_SETOPT_BACKUP = 16,           // Call m_config_backup_opt() before
    M_SETOPT_PRESERVE_CMDLINE = 32, // Don't set if already marked as FROM_CMDLINE
    M_SETOPT_NO_FIXED = 64,         // Reject M_OPT_FIXED options
    M_SETOPT_NO_PRE_PARSE = 128,    // Reject M_OPT_PREPARSE options
};

// Flags for safe option setting during runtime.
#define M_SETOPT_RUNTIME M_SETOPT_NO_FIXED

// Set the named option to the given string.
// flags: combination of M_SETOPT_* flags (0 for normal operation)
// Returns >= 0 on success, otherwise see OptionParserReturn.
int m_config_set_option_ext(struct m_config *config, struct bstr name,
                            struct bstr param, int flags);

/*  Set an option. (Like: m_config_set_option_ext(config, name, param, 0))
 *  \param config The config object.
 *  \param name The option's name.
 *  \param param The value of the option, can be NULL.
 *  \return See \ref OptionParserReturn.
 */
int m_config_set_option(struct m_config *config, struct bstr name,
                        struct bstr param);

static inline int m_config_set_option0(struct m_config *config,
                                       const char *name, const char *param)
{
    return m_config_set_option(config, bstr0(name), bstr0(param));
}

// Similar to m_config_set_option_ext(), but set as data in its native format.
// The type data points to is as in co->opt
int m_config_set_option_raw(struct m_config *config, struct m_config_option *co,
                            void *data, int flags);

// Similar to m_config_set_option_ext(), but set as data using mpv_node.
struct mpv_node;
int m_config_set_option_node(struct m_config *config, bstr name,
                             struct mpv_node *data, int flags);


int m_config_parse_suboptions(struct m_config *config, char *name,
                              char *subopts);

struct m_config_option *m_config_get_co(const struct m_config *config,
                                        struct bstr name);

int m_config_get_co_count(struct m_config *config);
struct m_config_option *m_config_get_co_index(struct m_config *config, int index);

// Return the n-th option by position. n==0 is the first option. If there are
// less than (n + 1) options, return NULL.
const char *m_config_get_positional_option(const struct m_config *config, int n);

// Return a hint to the option parser whether a parameter is/may be required.
// The option may still accept empty/non-empty parameters independent from
// this, and this function is useful only for handling ambiguous options like
// flags (e.g. "--a" is ok, "--a=yes" is also ok).
// Returns: error code (<0), or number of expected params (0, 1)
int m_config_option_requires_param(struct m_config *config, bstr name);

// Notify m_config_cache users that the option has (probably) changed its value.
void m_config_notify_change_co(struct m_config *config,
                               struct m_config_option *co);

bool m_config_is_in_group(struct m_config *config,
                          const struct m_sub_options *group,
                          struct m_config_option *co);

// Return all (visible) option names as NULL terminated string list.
char **m_config_list_options(void *ta_parent, const struct m_config *config);

/*  Print a list of all registered options.
 *  \param config The config object.
 */
void m_config_print_option_list(const struct m_config *config);


/*  Find the profile with the given name.
 *  \param config The config object.
 *  \param arg The profile's name.
 *  \return The profile object or NULL.
 */
struct m_profile *m_config_get_profile0(const struct m_config *config,
                                        char *name);
struct m_profile *m_config_get_profile(const struct m_config *config, bstr name);

// Apply and clear the default profile - it's the only profile that new config
// files do not simply append to (for configfile parser).
void m_config_finish_default_profile(struct m_config *config, int flags);

/*  Get the profile with the given name, creating it if necessary.
 *  \param config The config object.
 *  \param arg The profile's name.
 *  \return The profile object.
 */
struct m_profile *m_config_add_profile(struct m_config *config, char *name);

/*  Set the description of a profile.
 *  Used by the config file parser when defining a profile.
 *
 *  \param p The profile object.
 *  \param arg The profile's name.
 */
void m_profile_set_desc(struct m_profile *p, bstr desc);

/*  Add an option to a profile.
 *  Used by the config file parser when defining a profile.
 *
 *  \param config The config object.
 *  \param p The profile object.
 *  \param name The option's name.
 *  \param val The option's value.
 */
int m_config_set_profile_option(struct m_config *config, struct m_profile *p,
                                bstr name, bstr val);

/*  Enables profile usage
 *  Used by the config file parser when loading a profile.
 *
 *  \param config The config object.
 *  \param p The profile object.
 *  \param flags M_SETOPT_* bits
 * Returns error code (<0) or 0 on success
 */
int m_config_set_profile(struct m_config *config, char *name, int flags);

struct mpv_node m_config_get_profiles(struct m_config *config);

void *m_config_alloc_struct(void *talloc_ctx,
                            const struct m_sub_options *subopts);

// Create a copy of the struct ptr, described by opts.
// "opts" must live until the struct is free'd.
// Freeing the struct frees all members.
void *m_sub_options_copy(void *talloc_ctx, const struct m_sub_options *opts,
                         const void *ptr);

// This can be used to create and synchronize per-thread option structs,
// which then can be read without synchronization. No concurrent access to
// the cache itself is allowed.
struct m_config_cache {
    // The struct as indicated by m_config_cache_alloc's group parameter.
    void *opts;

    // Internal.
    struct m_config_shadow *shadow;
    struct m_config *shadow_config;
    long long ts;
    int group;
};

// Create a mirror copy from the global options.
//  ta_parent: parent for the returned allocation
//  global: option data source
//  group: the option group to return. This can be NULL for the global option
//         struct (MPOpts), or m_sub_options used in a certain OPT_SUBSTRUCT()
//         item.
struct m_config_cache *m_config_cache_alloc(void *ta_parent,
                                            struct mpv_global *global,
                                            const struct m_sub_options *group);

// Update the options in cache->opts to current global values. Return whether
// there was an update notification at all (which may or may not indicate that
// some options have changed).
// Keep in mind that while the cache->opts pointer does not change, the option
// data itself will (e.g. string options might be reallocated).
bool m_config_cache_update(struct m_config_cache *cache);

// Like m_config_cache_alloc(), but return the struct (m_config_cache->opts)
// directly, with no way to update the config.
// Warning: does currently not set the child as its own talloc root, which
//          means the only way to free the struct is by freeing ta_parent.
void *mp_get_config_group(void *ta_parent, struct mpv_global *global,
                          const struct m_sub_options *group);


// Read a single global option in a thread-safe way. For multiple options,
// use m_config_cache. The option must exist and match the provided type (the
// type is used as a sanity check only). Performs semi-expensive lookup.
void mp_read_option_raw(struct mpv_global *global, const char *name,
                        const struct m_option_type *type, void *dst);

struct m_config *mp_get_root_config(struct mpv_global *global);

#endif /* MPLAYER_M_CONFIG_H */
