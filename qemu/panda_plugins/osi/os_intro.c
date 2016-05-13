/* PANDABEGINCOMMENT
 * 
 * Authors:
 *  Tim Leek               tleek@ll.mit.edu
 *  Ryan Whelan            rwhelan@ll.mit.edu
 *  Joshua Hodosh          josh.hodosh@ll.mit.edu
 *  Michael Zhivich        mzhivich@ll.mit.edu
 *  Brendan Dolan-Gavitt   brendandg@gatech.edu
 * 
 * This work is licensed under the terms of the GNU GPL, version 2. 
 * See the COPYING file in the top-level directory. 
 * 
PANDAENDCOMMENT */
// This needs to be defined before anything is included in order to get
// the PRIx64 macro
#define __STDC_FORMAT_MACROS

#include <libgen.h>

#include "config.h"
#include "qemu-common.h"

#include "panda_plugin.h"
#include "panda_plugin_plugin.h"

#include "osi_types.h"
#include "osi_int_fns.h"
#include "os_intro.h"
#ifdef OSI_PROC_EVENTS
#include <glib.h>
#include "osi_proc_events.h"
#endif

bool init_plugin(void *);
void uninit_plugin(void *);
#ifdef OSI_PROC_EVENTS
int vmi_pgd_changed(CPUState *, target_ulong, target_ulong);
#endif

PPP_PROT_REG_CB(on_get_processes)
PPP_PROT_REG_CB(on_get_current_process)
PPP_PROT_REG_CB(on_get_modules)
PPP_PROT_REG_CB(on_get_libraries)
PPP_PROT_REG_CB(on_free_osiproc)
PPP_PROT_REG_CB(on_free_osiprocs)
PPP_PROT_REG_CB(on_free_osimodules)
#ifdef OSI_PROC_EVENTS
PPP_PROT_REG_CB(on_new_process)
PPP_PROT_REG_CB(on_finished_process)
#endif

PPP_CB_BOILERPLATE(on_get_processes)
PPP_CB_BOILERPLATE(on_get_current_process)
PPP_CB_BOILERPLATE(on_get_modules)
PPP_CB_BOILERPLATE(on_get_libraries)
PPP_CB_BOILERPLATE(on_free_osiproc)
PPP_CB_BOILERPLATE(on_free_osiprocs)
PPP_CB_BOILERPLATE(on_free_osimodules)
#ifdef OSI_PROC_EVENTS
PPP_CB_BOILERPLATE(on_new_process)
PPP_CB_BOILERPLATE(on_finished_process)
#endif

// The copious use of pointers to pointers in this file is due to
// the fact that PPP doesn't support return values (since it assumes
// that you will be running multiple callbacks at one site)

OsiProcs *get_processes(CPUState *env) {
    OsiProcs *p = NULL;
    PPP_RUN_CB(on_get_processes, env, &p);
    return p;
}

OsiProc *get_current_process(CPUState *env) {
    OsiProc *p = NULL;
    PPP_RUN_CB(on_get_current_process, env, &p);
    return p;
}

OsiModules *get_modules(CPUState *env) {
    OsiModules *m = NULL;
    PPP_RUN_CB(on_get_modules, env, &m);
    return m;
}

OsiModules *get_libraries(CPUState *env, OsiProc *p) {
    OsiModules *m = NULL;
    PPP_RUN_CB(on_get_libraries, env, p, &m);
    return m;
}

void free_osiproc(OsiProc *p) {
    PPP_RUN_CB(on_free_osiproc, p);
}

void free_osiprocs(OsiProcs *ps) {
    PPP_RUN_CB(on_free_osiprocs, ps);
}

void free_osimodules(OsiModules *ms) {
    PPP_RUN_CB(on_free_osimodules, ms);
}


#ifdef OSI_PROC_EVENTS
int vmi_pgd_changed(CPUState *env, target_ulong oldval, target_ulong newval) {
    uint32_t i;
    OsiProcs *ps, *in, *out;
    ps = in = out = NULL;

    /* update process state */
    ps = get_processes(env);
    procstate_update(ps, &in, &out);

    /* invoke callbacks for finished processes */
    if (out != NULL) {
        for (i=0; i<out->num; i++) {
            PPP_RUN_CB(on_finished_process, env, &out->proc[i]);
        }
        free_osiprocs(out);
    }

    /* invoke callbacks for new processes */
    if (in != NULL) {
        for (i=0; i<in->num; i++) {
            PPP_RUN_CB(on_new_process, env, &in->proc[i]);
        }
        free_osiprocs(in);
    }

    return 0;
}
#endif

// ugh
extern char **gargv;

bool init_plugin(void *self) {
#ifdef OSI_PROC_EVENTS
    panda_cb pcb;
    pcb.after_PGD_write = vmi_pgd_changed;
    panda_register_callback(self, PANDA_CB_VMI_PGD_CHANGED, pcb);
#endif
    // figure out what kind of os introspection is needed and grab it? 
    assert (!(panda_os_type == OST_UNKNOWN));
    if (panda_os_type == OST_LINUX) {
        // sadly, all of this is to find kernelinfo.conf file
        char *progname = gargv[0];
        if (progname[0] == '/') {
            // absolute path, yay!
        }
        else {
            // relative path
            char *cwd = get_current_dir_name();
            char *rel_progname = strdup(progname);
            progname = (char *) malloc(256);
            sprintf (progname, "%s/%s", cwd, rel_progname);
        }            
        char *progdir = strdup(progname);
        progdir = dirname(progdir);
        char kconfgroup[512];
        sprintf (kconfgroup, "%s-%d", panda_os_details, panda_os_bits);     
        char kconfile[256];
        snprintf (kconfile, 256, "%s/../panda_plugins/osi_linux/kernelinfo.conf", progdir);
        char *kconfile2 = realpath( kconfile, NULL);
        printf ("kconfile [%s]\n", kconfile2);
        char osi_linux_arg[512];
        sprintf (osi_linux_arg, "osi_linux:kconf_file=%s", kconfile2);
        panda_add_arg(osi_linux_arg, strlen(osi_linux_arg));
        sprintf (osi_linux_arg, "osi_linux:kconf_group=%s", panda_os_details);
        panda_add_arg(osi_linux_arg, strlen(osi_linux_arg));        
        printf ("osi grabbing linux introspection backend. osi_linux arg [%s]\n", osi_linux_arg);
        panda_require("osi_linux");
    }
    if (panda_os_type == OST_WINDOWS) {
        printf("osi grabbing windows introspection backend\n");
        panda_require("win7x86intro");
        panda_require("wintrospection");
    }
    return true;
}

void uninit_plugin(void *self) { }
