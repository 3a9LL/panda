#ifndef __STPI_INT_FNS_H__
#define __STPI_INT_FNS_H__

// Public Interface

// get dwarf info for a pc at current execution return -1 if not in dwarf-land
int stpi_get_pc_source_info (CPUState *env, target_ulong pc, PC_Info *info);

// iterate through the live vars at the current state of execution
void stpi_all_livevar_iter (CPUState *env, target_ulong pc, void (*f)(const char *var_ty, const char *var_nm, LocType loc_t, target_ulong loc));

// iterate through the function vars at the current state of execution
void stpi_funct_livevar_iter (CPUState *env, target_ulong pc, void (*f)(const char *var_ty, const char *var_nm, LocType loc_t, target_ulong loc));

// iterate through the global vars at the current state of execution
void stpi_global_livevar_iter (CPUState *env, target_ulong pc, void (*f)(const char *var_ty, const char *var_nm, LocType loc_t, target_ulong loc));


// Intended for use only by STPI Providers
void stpi_runcb_on_before_line_change(CPUState *env, target_ulong pc, const char *file_name, const char *funct_name, unsigned long long lno);
void stpi_runcb_on_after_line_change(CPUState *env, target_ulong pc, const char *file_name, const char *funct_name, unsigned long long lno);
void stpi_runcb_on_fn_start(CPUState *env, target_ulong pc, const char *file_name, const char *funct_name, unsigned long long lno);
#endif
