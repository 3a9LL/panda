Plugin: stpi
===========

Summary
-------

The `stpi` module forms the core of PANDA's OS-specific introspection support. The `stpi` plugin itself acts as a glue layer, offering a uniform API that can be called by plugins without that plugin needing to know anything about the underlying symbol table. An `stpi` povider plugin then implements the necessary functionality for each debugging information file format and executable file format.

Expressed graphically, this arrangement looks like:

    +-------------------+  +-------------------+
    |    Your Plugin    |  |    Your Plugin    |
    +-------------------+  +-------------------+
    |        stpi       |  |        stpi       |
    +-------------------+  +-------------------+
    |       dwarfp      |  |pdb (in the future)|
    +-------------------+  +-------------------+

The key is that you can swap out the bottom layer to support a different debugging file format without needing to modify your plugin.
Arguments
---------

None.

Dependencies
------------
Depends on a debugging file format specific plugin.  
Without this, stpi will return no useful information becuase it is just a shell for provider plugins. 

APIs and Callbacks
------------------
To implement debugging information process introspection support, an stpi provider should register the following callbacks:

Name: **on_get_pc_source_info**

Signature: `typedef void (*on_get_pc_source_info_t)(CPUState *a, target_ulong pc, PC_Info *info, int *rc)`

Description: get dwarf info for a pc at current execution return -1 if not in dwarf-land

Name: **on_all_livevar_iter**

Signature: `typedef void (*on_all_livevar_iter_t)(CPUState *a, target_ulong pc, liveVarCB f)`

Description: iterate through all live vars and apply function f on them.

Name: **on_global_livevar_iter**

Signature: `typedef void (*on_global_livevar_iter_t)(CPUState *a, target_ulong pc, liveVarCB f)`

Description: iterate through global live vars and apply function f on them.

Name: **on_funct_livevar_iter**

Signature: `typedef void (*on_funct_livevar_iter_t)(CPUState *a, target_ulong pc, liveVarCB f)`

Description: iterate through live vars local to the current function and apply function f on them.

---------------

In addition, there are two callbacks intended to be used by `stpi` *users*, rather than by introspection providers:

Name: **on_before_line_change**

Signature: `typedef void (*on_before_line_change_t)(CPUState *env, target_ulong pc, const char *file_name, const char *funct_name, unsigned long long lno)`

Description: Called before execution starts a line in source code.

Name: **on_after_line_change**

Signature: `typedef void (*on_after_line_change_t)(CPUState *env, target_ulong pc, const char *file_name, const char *funct_name, unsigned long long lno)`

Description: Called after execution finishes a line in source code.

Name: **on_fn_start**

Signature: `typedef void (*on_fn_start_t)(CPUState *env, target_ulong pc, const char *file_name, const char *funct_name, unsigned long long lno)`

Description: Called when execution hits the start of a function after the function's prologue.

---------------

There are three API functions provided to clients that allow them to iterate through live variables at the current state of execution.

    // get dwarf info for a pc at current execution return -1 if not in dwarf-land
    int stpi_get_pc_source_info (CPUState *env, target_ulong pc, PC_Info *info);
    
    // iterate through the live vars at the current state of execution
    void stpi_all_livevar_iter (CPUState *env, target_ulong pc, void (*f)(const char *var_ty, const char *var_nm, LocType loc_t,     target_ulong loc));

    // iterate through the function vars at the current state of execution
    void stpi_funct_livevar_iter (CPUState *env, target_ulong pc, void (*f)(const char *var_ty, const char *var_nm, LocType loc_t, target_ulong loc));
    
    // iterate through the global vars at the current state of execution
    void stpi_global_livevar_iter (CPUState *env, target_ulong pc, void (*f)(const char *var_ty, const char *var_nm, LocType loc_t, target_ulong loc));
    
There are three API functions provided to stpi providers that allow them to run callbacks that will be available to clients through the `stpi` interface.
    // run a line change callback
    void stpi_runcb_on_before_line_change(CPUState *env, target_ulong pc, const char *file_name, const char *funct_name, unsigned long long lno);
    void stpi_runcb_on_after_line_change(CPUState *env, target_ulong pc, const char *file_name, const char *funct_name, unsigned long long lno);
    // run a callback signaling the beginning of a function AFTER the function prologue
    void stpi_runcb_on_fn_start(CPUState *env, target_ulong pc, const char *file_name, const char *funct_name, unsigned long long lno);

---------------

Data Structures used by `stpi`:
    
    // A location is one of three types: Register, Memory, or a Constant (variable is not stored anywhere in memory or registers)
    // but we know it's value at compile time.
    // LocErr means that the variables location was too difficult to be determine (future support may remediate this).
    //     -> ie a variable's location is represented by two different registers (DW_OP_bit_piece)
    typedef enum { LocReg, LocMem, LocConst, LocErr } LocType;
    
    // the live_var_iter functions take in this callback and apply it to all vars that are live at the current program state
    typedef void (*liveVarCB)(const char *var_ty, const char *var_nm, LocType loc_t, target_ulong loc);


Example
-------
This is an example of a use of `stpi`.  Note that for this to be useful it needs to include a provider plugin to support the `stpi` callbacks for a specific debuggin format: DWARF, pdb, etc

    extern "C" {
    
    // usual panda includes . . .    
    
    // stpi specific includes
    #include "../stpi/stpi_types.h"
    #include "../stpi/stpi_ext.h"
    #include "../stpi/stpi.h"
    // stpi provider include
    #include "../dwarfp/dwarfp_ext.h"
        
        bool init_plugin(void *);
        void uninit_plugin(void *);
        
    }
    
    // stpi is only supported 32 bit linux
    #if defined(TARGET_I386) && !defined(TARGET_X86_64)
    void pfun(const char *var_ty, const char *var_nm, LocType loc_t, target_ulong loc){
        
        switch (loc_t){
            case LocReg:
                printf("VAR REG %s %s in 0x%x\n", var_ty, var_nm, loc);
                break;
            case LocMem:
                printf("VAR MEM %s %s @ 0x%x\n", var_ty, var_nm, loc);
                break;
            case LocConst:
                printf("VAR CONST %s %s as 0x%x\n", var_ty, var_nm, loc);
                break;
            case LocErr:
                printf("VAR does not have a location we could determine. Most likely because the var is split among multiple locations\n");
                break;
        }
    }
    void on_line_change(CPUState *env, target_ulong pc, const char *file_Name, const char *funct_name, unsigned long long lno){
        printf("[%s] %s(), ln: %4lld, pc @ 0x%x\n",file_Name, funct_name,lno,pc);
        //stpi_funct_livevar_iter(env, pc, pfun);
    }
    void on_fn_start(CPUState *env, target_ulong pc, const char *file_Name, const char *funct_name, unsigned long long lno){
        printf("fn-start: %s() [%s], ln: %4lld, pc @ 0x%x\n",funct_name,file_Name,lno,pc);
        stpi_funct_livevar_iter(env, pc, pfun);
    }
    #endif
    
    bool init_plugin(void *self) {
    
    #if defined(TARGET_I386) && !defined(TARGET_X86_64)
        printf("Initializing plugin dwarf_simple\n");
        //panda_arg_list *args = panda_get_args("dwarf_taint");
        panda_require("stpi");
        assert(init_stpi_api());
        panda_require("dwarfp");
        assert(init_dwarfp_api());
        
        //PPP_REG_CB("stpi", on_line_change, on_line_change);
        PPP_REG_CB("stpi", on_fn_start, on_fn_start);
    #endif
        return true;
    }
    void uninit_plugin(void *self) {
    }

