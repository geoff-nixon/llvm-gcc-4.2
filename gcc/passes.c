/* Top level of GCC compilers (cc1, cc1plus, etc.)
   Copyright (C) 1987, 1988, 1989, 1992, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.  */

/* This is the top level of cc1/c++.
   It parses command args, opens files, invokes the various passes
   in the proper order, and counts the time used by each.
   Error messages and low-level interface to malloc also handled here.  */

#include "config.h"
#undef FLOAT /* This is for hpux. They should change hpux.  */
#undef FFS  /* Some systems define this in param.h.  */
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include <signal.h>

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif

#ifdef HAVE_SYS_TIMES_H
# include <sys/times.h>
#endif

#include "line-map.h"
#include "input.h"
#include "tree.h"
#include "rtl.h"
#include "tm_p.h"
#include "flags.h"
#include "insn-attr.h"
#include "insn-config.h"
#include "insn-flags.h"
#include "hard-reg-set.h"
#include "recog.h"
#include "output.h"
#include "except.h"
#include "function.h"
#include "toplev.h"
#include "expr.h"
#include "basic-block.h"
#include "intl.h"
#include "ggc.h"
#include "graph.h"
#include "regs.h"
#include "timevar.h"
#include "diagnostic.h"
#include "params.h"
#include "reload.h"
#include "dwarf2asm.h"
#include "integrate.h"
#include "real.h"
#include "debug.h"
#include "target.h"
#include "langhooks.h"
#include "cfglayout.h"
#include "cfgloop.h"
#include "hosthooks.h"
#include "cgraph.h"
#include "opts.h"
#include "coverage.h"
#include "value-prof.h"
#include "tree-inline.h"
#include "tree-flow.h"
#include "tree-pass.h"
#include "tree-dump.h"
/* LLVM LOCAL begin */
#include "llvm.h"

#ifdef ENABLE_LLVM
/* LLVM doesn't need the GCC scheduler.  Clear this so it need not be linked
   in.  */
#undef INSN_SCHEDULING
#endif
/* LLVM LOCAL end */



#if defined (DWARF2_UNWIND_INFO) || defined (DWARF2_DEBUGGING_INFO)
#include "dwarf2out.h"
#endif

#if defined (DBX_DEBUGGING_INFO) || defined (XCOFF_DEBUGGING_INFO)
#include "dbxout.h"
#endif

#ifdef SDB_DEBUGGING_INFO
#include "sdbout.h"
#endif

#ifdef XCOFF_DEBUGGING_INFO
#include "xcoffout.h"		/* Needed for external data
				   declarations for e.g. AIX 4.x.  */
#endif

/* Global variables used to communicate with passes.  */
int dump_flags;
bool in_gimple_form;


/* This is called from various places for FUNCTION_DECL, VAR_DECL,
   and TYPE_DECL nodes.

   This does nothing for local (non-static) variables, unless the
   variable is a register variable with DECL_ASSEMBLER_NAME set.  In
   that case, or if the variable is not an automatic, it sets up the
   RTL and outputs any assembler code (label definition, storage
   allocation and initialization).

   DECL is the declaration.  TOP_LEVEL is nonzero
   if this declaration is not within a function.  */

void
rest_of_decl_compilation (tree decl,
			  int top_level,
			  int at_end)
{
  /* We deferred calling assemble_alias so that we could collect
     other attributes such as visibility.  Emit the alias now.  */
  {
    tree alias;
    alias = lookup_attribute ("alias", DECL_ATTRIBUTES (decl));
    if (alias)
      {
	alias = TREE_VALUE (TREE_VALUE (alias));
	alias = get_identifier (TREE_STRING_POINTER (alias));
	assemble_alias (decl, alias);
      }
  }

  /* Can't defer this, because it needs to happen before any
     later function definitions are processed.  */
  if (DECL_ASSEMBLER_NAME_SET_P (decl) && DECL_REGISTER (decl))
    /* LLVM LOCAL begin */
#ifndef ENABLE_LLVM
    make_decl_rtl (decl);
#else
    make_decl_llvm (decl);
#endif
    /* LLVM LOCAL end */

  /* Forward declarations for nested functions are not "external",
     but we need to treat them as if they were.  */
  if (TREE_STATIC (decl) || DECL_EXTERNAL (decl)
      || TREE_CODE (decl) == FUNCTION_DECL)
    {
      timevar_push (TV_VARCONST);

      /* Don't output anything when a tentative file-scope definition
	 is seen.  But at end of compilation, do output code for them.

	 We do output all variables when unit-at-a-time is active and rely on
	 callgraph code to defer them except for forward declarations
	 (see gcc.c-torture/compile/920624-1.c) */
      if ((at_end
	   || !DECL_DEFER_OUTPUT (decl)
	   || DECL_INITIAL (decl))
	  && !DECL_EXTERNAL (decl))
	{
	  if (TREE_CODE (decl) != FUNCTION_DECL)
	    cgraph_varpool_finalize_decl (decl);
	  else
	    assemble_variable (decl, top_level, at_end, 0);
	}

#ifdef ASM_FINISH_DECLARE_OBJECT
      if (decl == last_assemble_variable_decl)
	{
	  ASM_FINISH_DECLARE_OBJECT (asm_out_file, decl,
				     top_level, at_end);
	}
#endif

      timevar_pop (TV_VARCONST);
    }
  else if (TREE_CODE (decl) == TYPE_DECL
	   /* Like in rest_of_type_compilation, avoid confusing the debug
	      information machinery when there are errors.  */
	   && !(sorrycount || errorcount))
    {
    /* LLVM LOCAL begin */
#ifndef ENABLE_LLVM
    timevar_push (TV_SYMOUT);
    debug_hooks->type_decl (decl, !top_level);
    timevar_pop (TV_SYMOUT);
#else
    llvm_emit_typedef (decl);
#endif
    /* LLVM LOCAL end */
    }

  /* Let cgraph know about the existence of variables.  */
  if (TREE_CODE (decl) == VAR_DECL && !DECL_EXTERNAL (decl))
    cgraph_varpool_node (decl);
}

/* Called after finishing a record, union or enumeral type.  */

void
rest_of_type_compilation (tree type, int toplev)
{
  /* Avoid confusing the debug information machinery when there are
     errors.  */
  if (errorcount != 0 || sorrycount != 0)
    return;

  /* LLVM LOCAL begin */
#ifndef ENABLE_LLVM
  timevar_push (TV_SYMOUT);
  debug_hooks->type_decl (TYPE_STUB_DECL (type), !toplev);
  timevar_pop (TV_SYMOUT);
#else
  llvm_emit_typedef (TYPE_STUB_DECL (type));
  toplev = toplev;  /* avoid unused var warning */
#endif
  /* LLVM LOCAL end */
}



void
finish_optimization_passes (void)
{
  enum tree_dump_index i;
  struct dump_file_info *dfi;
  char *name;
  
  /* LLVM LOCAL begin Dead code strip cc1. */
#ifdef ENABLE_LLVM
  return;
#endif
  /* LLVM LOCAL end Dead code strip cc1. */

  timevar_push (TV_DUMP);
  if (profile_arc_flag || flag_test_coverage || flag_branch_probabilities)
    {
      dump_file = dump_begin (pass_profile.static_pass_number, NULL);
      end_branch_prob ();
      if (dump_file)
	dump_end (pass_profile.static_pass_number, dump_file);
    }

  if (optimize > 0)
    {
      dump_file = dump_begin (pass_combine.static_pass_number, NULL);
      if (dump_file)
	{
	  dump_combine_total_stats (dump_file);
          dump_end (pass_combine.static_pass_number, dump_file);
	}
    }

  /* Do whatever is necessary to finish printing the graphs.  */
  if (graph_dump_format != no_graph)
    for (i = TDI_end; (dfi = get_dump_file_info (i)) != NULL; ++i)
      if (dump_initialized_p (i)
	  && (dfi->flags & TDF_GRAPH) != 0
	  && (name = get_dump_file_name (i)) != NULL)
	{
	  finish_graph_dump_file (name);
	  free (name);
	}

  timevar_pop (TV_DUMP);
}

static bool
gate_rest_of_compilation (void)
{
  /* Early return if there were errors.  We can run afoul of our
     consistency checks, and there's not really much point in fixing them.  */
  return !(rtl_dump_and_exit || flag_syntax_only || errorcount || sorrycount);
}

struct tree_opt_pass pass_rest_of_compilation =
{
  NULL,                                 /* name */
  gate_rest_of_compilation,             /* gate */
  NULL,                                 /* execute */
  NULL,                                 /* sub */
  NULL,                                 /* next */
  0,                                    /* static_pass_number */
  TV_REST_OF_COMPILATION,               /* tv_id */
  PROP_rtl,                             /* properties_required */
  0,                                    /* properties_provided */
  0,                                    /* properties_destroyed */
  0,                                    /* todo_flags_start */
  TODO_ggc_collect,                     /* todo_flags_finish */
  0                                     /* letter */
};

static bool
gate_postreload (void)
{
  return reload_completed;
}

struct tree_opt_pass pass_postreload =
{
  NULL,                                 /* name */
  gate_postreload,                      /* gate */
  NULL,                                 /* execute */
  NULL,                                 /* sub */
  NULL,                                 /* next */
  0,                                    /* static_pass_number */
  0,                                    /* tv_id */
  PROP_rtl,                             /* properties_required */
  0,                                    /* properties_provided */
  0,                                    /* properties_destroyed */
  0,                                    /* todo_flags_start */
  TODO_ggc_collect,                     /* todo_flags_finish */
  0					/* letter */
};



/* The root of the compilation pass tree, once constructed.  */
struct tree_opt_pass *all_passes, *all_ipa_passes, *all_lowering_passes;

/* Iterate over the pass tree allocating dump file numbers.  We want
   to do this depth first, and independent of whether the pass is
   enabled or not.  */

static void
register_one_dump_file (struct tree_opt_pass *pass, bool ipa, int properties)
{
  char *dot_name, *flag_name, *glob_name;
  const char *prefix;
  char num[10];
  int flags;

  /* See below in next_pass_1.  */
  num[0] = '\0';
  if (pass->static_pass_number != -1)
    sprintf (num, "%d", ((int) pass->static_pass_number < 0
			 ? 1 : pass->static_pass_number));

  dot_name = concat (".", pass->name, num, NULL);
  if (ipa)
    prefix = "ipa-", flags = TDF_IPA;
  else if (properties & PROP_trees)
    prefix = "tree-", flags = TDF_TREE;
  else
    prefix = "rtl-", flags = TDF_RTL;

  flag_name = concat (prefix, pass->name, num, NULL);
  glob_name = concat (prefix, pass->name, NULL);
  pass->static_pass_number = dump_register (dot_name, flag_name, glob_name,
                                            flags, pass->letter);
}

/* Recursive worker function for register_dump_files.  */

static int 
register_dump_files_1 (struct tree_opt_pass *pass, bool ipa, int properties)
{
  do
    {
      int new_properties = (properties | pass->properties_provided)
			   & ~pass->properties_destroyed;

      if (pass->name)
        register_one_dump_file (pass, ipa, new_properties);

      if (pass->sub)
        new_properties = register_dump_files_1 (pass->sub, false,
						new_properties);

      /* If we have a gate, combine the properties that we could have with
         and without the pass being examined.  */
      if (pass->gate)
        properties &= new_properties;
      else
        properties = new_properties;

      pass = pass->next;
    }
  while (pass);

  return properties;
}

/* Register the dump files for the pipeline starting at PASS.  IPA is
   true if the pass is inter-procedural, and PROPERTIES reflects the
   properties that are guaranteed to be available at the beginning of
   the pipeline.  */

static void 
register_dump_files (struct tree_opt_pass *pass, bool ipa, int properties)
{
  pass->properties_required |= properties;
  pass->todo_flags_start |= TODO_set_props;
  register_dump_files_1 (pass, ipa, properties);
}

/* Add a pass to the pass list. Duplicate the pass if it's already
   in the list.  */

static struct tree_opt_pass **
next_pass_1 (struct tree_opt_pass **list, struct tree_opt_pass *pass)
{
  /* A nonzero static_pass_number indicates that the
     pass is already in the list.  */
  if (pass->static_pass_number)
    {
      struct tree_opt_pass *new;

      new = xmalloc (sizeof (*new));
      memcpy (new, pass, sizeof (*new));

      /* Indicate to register_dump_files that this pass has duplicates,
         and so it should rename the dump file.  The first instance will
         be -1, and be number of duplicates = -static_pass_number - 1.
         Subsequent instances will be > 0 and just the duplicate number.  */
      if (pass->name)
        {
          pass->static_pass_number -= 1;
          new->static_pass_number = -pass->static_pass_number;
	}
      
      *list = new;
    }
  else
    {
      pass->static_pass_number = -1;
      *list = pass;
    }  
  
  return &(*list)->next;
          
}

/* Construct the pass tree.  The sequencing of passes is driven by
   the cgraph routines:

   cgraph_finalize_compilation_unit ()
       for each node N in the cgraph
	   cgraph_analyze_function (N)
	       cgraph_lower_function (N) -> all_lowering_passes

   If we are optimizing, cgraph_optimize is then invoked:

   cgraph_optimize ()
       ipa_passes () 			-> all_ipa_passes
       cgraph_expand_all_functions ()
           for each node N in the cgraph
	       cgraph_expand_function (N)
	           cgraph_lower_function (N)	-> Now a NOP.
		   lang_hooks.callgraph.expand_function (DECL (N))
		   	tree_rest_of_compilation (DECL (N))  -> all_passes
*/

void
init_optimization_passes (void)
{
  struct tree_opt_pass **p;

#define NEXT_PASS(PASS)  (p = next_pass_1 (p, &PASS))
  /* Interprocedural optimization passes.  */
  p = &all_ipa_passes;
  NEXT_PASS (pass_early_ipa_inline);
  NEXT_PASS (pass_early_local_passes);
  NEXT_PASS (pass_ipa_cp);
  NEXT_PASS (pass_ipa_inline);
  NEXT_PASS (pass_ipa_reference);
  NEXT_PASS (pass_ipa_pure_const); 
  NEXT_PASS (pass_ipa_type_escape);
  NEXT_PASS (pass_ipa_pta);
  *p = NULL;

  /* All passes needed to lower the function into shape optimizers can
     operate on.  */
  p = &all_lowering_passes;
  NEXT_PASS (pass_remove_useless_stmts);
  NEXT_PASS (pass_mudflap_1);
  NEXT_PASS (pass_lower_omp);
  NEXT_PASS (pass_lower_cf);
  NEXT_PASS (pass_lower_eh);
  NEXT_PASS (pass_build_cfg);
  NEXT_PASS (pass_lower_complex_O0);
  /* LLVM LOCAL */
  /* NEXT_PASS (pass_lower_vector); */
  NEXT_PASS (pass_warn_function_return);
  NEXT_PASS (pass_early_tree_profile);
  *p = NULL;

  p = &pass_early_local_passes.sub;
  NEXT_PASS (pass_tree_profile);
  NEXT_PASS (pass_cleanup_cfg);
  NEXT_PASS (pass_rebuild_cgraph_edges);
  *p = NULL;

  p = &all_passes;
  NEXT_PASS (pass_fixup_cfg);
  NEXT_PASS (pass_init_datastructures);
  NEXT_PASS (pass_expand_omp);
  NEXT_PASS (pass_all_optimizations);
  NEXT_PASS (pass_warn_function_noreturn);
  NEXT_PASS (pass_mudflap_2);
  NEXT_PASS (pass_free_datastructures);
  NEXT_PASS (pass_free_cfg_annotations);
  NEXT_PASS (pass_expand);
  NEXT_PASS (pass_rest_of_compilation);
  NEXT_PASS (pass_clean_state);
  *p = NULL;

  p = &pass_all_optimizations.sub;
  NEXT_PASS (pass_referenced_vars);
  NEXT_PASS (pass_reset_cc_flags);
  NEXT_PASS (pass_create_structure_vars);
  NEXT_PASS (pass_build_ssa);
  NEXT_PASS (pass_may_alias);
  NEXT_PASS (pass_return_slot);
  NEXT_PASS (pass_rename_ssa_copies);
  NEXT_PASS (pass_early_warn_uninitialized);

  /* Initial scalar cleanups.  */
  NEXT_PASS (pass_ccp);
  NEXT_PASS (pass_fre);
  NEXT_PASS (pass_dce);
  NEXT_PASS (pass_forwprop);
  NEXT_PASS (pass_copy_prop);
  NEXT_PASS (pass_merge_phi);
  NEXT_PASS (pass_vrp);
  NEXT_PASS (pass_dce);
  NEXT_PASS (pass_dominator);

  /* The only const/copy propagation opportunities left after
     DOM should be due to degenerate PHI nodes.  So rather than
     run the full propagators, run a specialized pass which
     only examines PHIs to discover const/copy propagation
     opportunities.  */
  NEXT_PASS (pass_phi_only_cprop);

  NEXT_PASS (pass_phiopt);
  NEXT_PASS (pass_may_alias);
  NEXT_PASS (pass_tail_recursion);
  NEXT_PASS (pass_profile);
  NEXT_PASS (pass_ch);
  NEXT_PASS (pass_stdarg);
  NEXT_PASS (pass_lower_complex);
  NEXT_PASS (pass_sra);
  /* FIXME: SRA may generate arbitrary gimple code, exposing new
     aliased and call-clobbered variables.  As mentioned below,
     pass_may_alias should be a TODO item.  */
  NEXT_PASS (pass_may_alias);
  NEXT_PASS (pass_rename_ssa_copies);
  NEXT_PASS (pass_dominator);

  /* The only const/copy propagation opportunities left after
     DOM should be due to degenerate PHI nodes.  So rather than
     run the full propagators, run a specialized pass which
     only examines PHIs to discover const/copy propagation
     opportunities.  */
  NEXT_PASS (pass_phi_only_cprop);

  NEXT_PASS (pass_reassoc);
  NEXT_PASS (pass_dce);
  NEXT_PASS (pass_dse);
  NEXT_PASS (pass_may_alias);
  NEXT_PASS (pass_forwprop);
  NEXT_PASS (pass_phiopt);
  NEXT_PASS (pass_object_sizes);
  NEXT_PASS (pass_store_ccp);
  NEXT_PASS (pass_store_copy_prop);
  NEXT_PASS (pass_fold_builtins);
  /* FIXME: May alias should a TODO but for 4.0.0,
     we add may_alias right after fold builtins
     which can create arbitrary GIMPLE.  */
  NEXT_PASS (pass_may_alias);
  NEXT_PASS (pass_split_crit_edges);
  NEXT_PASS (pass_pre);
  NEXT_PASS (pass_may_alias);
  NEXT_PASS (pass_sink_code);
  NEXT_PASS (pass_tree_loop);
  NEXT_PASS (pass_cse_reciprocals);
  NEXT_PASS (pass_reassoc);
  NEXT_PASS (pass_vrp);
  NEXT_PASS (pass_dominator);

  /* The only const/copy propagation opportunities left after
     DOM should be due to degenerate PHI nodes.  So rather than
     run the full propagators, run a specialized pass which
     only examines PHIs to discover const/copy propagation
     opportunities.  */
  NEXT_PASS (pass_phi_only_cprop);

  NEXT_PASS (pass_cd_dce);

  /* FIXME: If DCE is not run before checking for uninitialized uses,
     we may get false warnings (e.g., testsuite/gcc.dg/uninit-5.c).
     However, this also causes us to misdiagnose cases that should be
     real warnings (e.g., testsuite/gcc.dg/pr18501.c).
     
     To fix the false positives in uninit-5.c, we would have to
     account for the predicates protecting the set and the use of each
     variable.  Using a representation like Gated Single Assignment
     may help.  */
  NEXT_PASS (pass_late_warn_uninitialized);
  NEXT_PASS (pass_dse);
  NEXT_PASS (pass_forwprop);
  NEXT_PASS (pass_phiopt);
  NEXT_PASS (pass_tail_calls);
  NEXT_PASS (pass_rename_ssa_copies);
  NEXT_PASS (pass_uncprop);
  NEXT_PASS (pass_del_ssa);
  NEXT_PASS (pass_nrv);
  NEXT_PASS (pass_mark_used_blocks);
  NEXT_PASS (pass_cleanup_cfg_post_optimizing);
  *p = NULL;

  p = &pass_tree_loop.sub;
  NEXT_PASS (pass_tree_loop_init);
  NEXT_PASS (pass_copy_prop);
  NEXT_PASS (pass_lim);
  NEXT_PASS (pass_tree_unswitch);
  NEXT_PASS (pass_scev_cprop);
  NEXT_PASS (pass_empty_loop);
  NEXT_PASS (pass_record_bounds);
  NEXT_PASS (pass_linear_transform);
  NEXT_PASS (pass_iv_canon);
  /* APPLE LOCAL begin loops-to-memset */
  NEXT_PASS (pass_memset);
  /* APPLE LOCAL end loops-to-memset */
  NEXT_PASS (pass_if_conversion);
  NEXT_PASS (pass_vectorize);
  /* NEXT_PASS (pass_may_alias) cannot be done again because the
     vectorizer creates alias relations that are not supported by
     pass_may_alias.  */
  NEXT_PASS (pass_complete_unroll);
  NEXT_PASS (pass_loop_prefetch);
  NEXT_PASS (pass_iv_optimize);
  NEXT_PASS (pass_tree_loop_done);
  *p = NULL;

  p = &pass_vectorize.sub;
  NEXT_PASS (pass_lower_vector_ssa);
  NEXT_PASS (pass_dce_loop);
  *p = NULL;

  p = &pass_loop2.sub;
  NEXT_PASS (pass_rtl_loop_init);
  NEXT_PASS (pass_rtl_move_loop_invariants);
  NEXT_PASS (pass_rtl_unswitch);
  NEXT_PASS (pass_rtl_unroll_and_peel_loops);
  NEXT_PASS (pass_rtl_doloop);
  NEXT_PASS (pass_rtl_loop_done);
  *p = NULL;
  
  p = &pass_rest_of_compilation.sub;
  NEXT_PASS (pass_init_function);
  NEXT_PASS (pass_jump);
  NEXT_PASS (pass_insn_locators_initialize);
  NEXT_PASS (pass_rtl_eh);
  NEXT_PASS (pass_initial_value_sets);
  NEXT_PASS (pass_unshare_all_rtl);
  NEXT_PASS (pass_instantiate_virtual_regs);
  NEXT_PASS (pass_jump2);
  NEXT_PASS (pass_cse);
  NEXT_PASS (pass_gcse);
  NEXT_PASS (pass_jump_bypass);
  NEXT_PASS (pass_rtl_ifcvt);
  NEXT_PASS (pass_tracer);
  /* Perform loop optimizations.  It might be better to do them a bit
     sooner, but we want the profile feedback to work more
     efficiently.  */
  NEXT_PASS (pass_loop2);
  NEXT_PASS (pass_web);
  NEXT_PASS (pass_cse2);
  NEXT_PASS (pass_life);
  NEXT_PASS (pass_combine);
  NEXT_PASS (pass_if_after_combine);
  NEXT_PASS (pass_partition_blocks);
  NEXT_PASS (pass_regmove);
  NEXT_PASS (pass_split_all_insns);
  NEXT_PASS (pass_mode_switching);
  NEXT_PASS (pass_see);
  NEXT_PASS (pass_recompute_reg_usage);
  NEXT_PASS (pass_sms);
  NEXT_PASS (pass_sched);
  NEXT_PASS (pass_local_alloc);
  NEXT_PASS (pass_global_alloc);
  NEXT_PASS (pass_postreload);
  *p = NULL;

  p = &pass_postreload.sub;
  NEXT_PASS (pass_postreload_cse);
  NEXT_PASS (pass_gcse2);
  NEXT_PASS (pass_flow2);
  NEXT_PASS (pass_rtl_seqabstr);
  NEXT_PASS (pass_stack_adjustments);
  NEXT_PASS (pass_peephole2);
  NEXT_PASS (pass_if_after_reload);
  NEXT_PASS (pass_regrename);
  NEXT_PASS (pass_reorder_blocks);
  NEXT_PASS (pass_branch_target_load_optimize);
  NEXT_PASS (pass_leaf_regs);
  NEXT_PASS (pass_sched2);
  NEXT_PASS (pass_split_before_regstack);
  NEXT_PASS (pass_stack_regs);
  NEXT_PASS (pass_compute_alignments);
  NEXT_PASS (pass_duplicate_computed_gotos);
  NEXT_PASS (pass_variable_tracking);
  NEXT_PASS (pass_free_cfg);
  NEXT_PASS (pass_machine_reorg);
  NEXT_PASS (pass_purge_lineno_notes);
  NEXT_PASS (pass_cleanup_barriers);
  NEXT_PASS (pass_delay_slots);
  NEXT_PASS (pass_split_for_shorten_branches);
  NEXT_PASS (pass_convert_to_eh_region_ranges);
  NEXT_PASS (pass_shorten_branches);
  NEXT_PASS (pass_set_nothrow_function_flags);
  NEXT_PASS (pass_final);
  *p = NULL;

#undef NEXT_PASS

  /* Register the passes with the tree dump code.  */
  register_dump_files (all_ipa_passes, true,
		       PROP_gimple_any | PROP_gimple_lcf | PROP_gimple_leh
		       | PROP_cfg);
  register_dump_files (all_lowering_passes, false, PROP_gimple_any);
  register_dump_files (all_passes, false,
		       PROP_gimple_any | PROP_gimple_lcf | PROP_gimple_leh
		       | PROP_cfg);
}

static unsigned int last_verified;
static unsigned int curr_properties;

static void
execute_todo (unsigned int flags)
{
#if defined ENABLE_CHECKING
  if (need_ssa_update_p ())
    gcc_assert (flags & TODO_update_ssa_any);
#endif

  if (curr_properties & PROP_ssa)
    flags |= TODO_verify_ssa;
  flags &= ~last_verified;
  if (!flags)
    return;
  
  /* Always recalculate SMT usage before doing anything else.  */
  if (flags & TODO_update_smt_usage)
    recalculate_used_alone ();

  /* Always cleanup the CFG before trying to update SSA .  */
  if (flags & TODO_cleanup_cfg)
    {
      /* CFG Cleanup can cause a constant to prop into an ARRAY_REF.  */
      updating_used_alone = true;

      if (current_loops)
	cleanup_tree_cfg_loop ();
      else
	cleanup_tree_cfg ();

      /* Update the used alone after cleanup cfg.  */
      recalculate_used_alone ();

      /* When cleanup_tree_cfg merges consecutive blocks, it may
	 perform some simplistic propagation when removing single
	 valued PHI nodes.  This propagation may, in turn, cause the
	 SSA form to become out-of-date (see PR 22037).  So, even
	 if the parent pass had not scheduled an SSA update, we may
	 still need to do one.  */
      if (!(flags & TODO_update_ssa_any) && need_ssa_update_p ())
	flags |= TODO_update_ssa;
    }

  if (flags & TODO_update_ssa_any)
    {
      unsigned update_flags = flags & TODO_update_ssa_any;
      update_ssa (update_flags);
      last_verified &= ~TODO_verify_ssa;
    }

  if (flags & TODO_remove_unused_locals)
    remove_unused_locals ();

  if ((flags & TODO_dump_func)
      && dump_file && current_function_decl)
    {
      if (curr_properties & PROP_trees)
        dump_function_to_file (current_function_decl,
                               dump_file, dump_flags);
      else
	{
	  if (dump_flags & TDF_SLIM)
	    print_rtl_slim_with_bb (dump_file, get_insns (), dump_flags);
	  else if ((curr_properties & PROP_cfg) && (dump_flags & TDF_BLOCKS))
	    print_rtl_with_bb (dump_file, get_insns ());
          else
	    print_rtl (dump_file, get_insns ());

	  if (curr_properties & PROP_cfg
	      && graph_dump_format != no_graph
	      && (dump_flags & TDF_GRAPH))
	    print_rtl_graph_with_bb (dump_file_name, get_insns ());
	}

      /* Flush the file.  If verification fails, we won't be able to
	 close the file before aborting.  */
      fflush (dump_file);
    }
  if ((flags & TODO_dump_cgraph)
      && dump_file && !current_function_decl)
    {
      dump_cgraph (dump_file);
      /* Flush the file.  If verification fails, we won't be able to
	 close the file before aborting.  */
      fflush (dump_file);
    }

  if (flags & TODO_ggc_collect)
    {
      ggc_collect ();
    }

#if defined ENABLE_CHECKING
  if (flags & TODO_verify_ssa)
    verify_ssa (true);
  if (flags & TODO_verify_flow)
    verify_flow_info ();
  if (flags & TODO_verify_stmts)
    verify_stmts ();
  if (flags & TODO_verify_loops)
    verify_loop_closed_ssa ();
#endif

  last_verified = flags & TODO_verify_all;
}

/* Verify invariants that should hold between passes.  This is a place
   to put simple sanity checks.  */

static void
verify_interpass_invariants (void)
{
#ifdef ENABLE_CHECKING
  gcc_assert (!fold_deferring_overflow_warnings_p ());
#endif
}

static bool
execute_one_pass (struct tree_opt_pass *pass)
{
  bool initializing_dump;
  unsigned int todo_after = 0;

  /* See if we're supposed to run this pass.  */
  if (pass->gate && !pass->gate ())
    return false;

  if (pass->todo_flags_start & TODO_set_props)
    curr_properties = pass->properties_required;

  /* Note that the folders should only create gimple expressions.
     This is a hack until the new folder is ready.  */
  in_gimple_form = (curr_properties & PROP_trees) != 0;

  /* Run pre-pass verification.  */
  execute_todo (pass->todo_flags_start);

  gcc_assert ((curr_properties & pass->properties_required)
	      == pass->properties_required);

  if (pass->properties_destroyed & PROP_smt_usage)
    updating_used_alone = true;

  /* If a dump file name is present, open it if enabled.  */
  if (pass->static_pass_number != -1)
    {
      initializing_dump = !dump_initialized_p (pass->static_pass_number);
      dump_file_name = get_dump_file_name (pass->static_pass_number);
      dump_file = dump_begin (pass->static_pass_number, &dump_flags);
      if (dump_file && current_function_decl)
	{
	  const char *dname, *aname;
	  dname = lang_hooks.decl_printable_name (current_function_decl, 2);
	  aname = (IDENTIFIER_POINTER
		   (DECL_ASSEMBLER_NAME (current_function_decl)));
	  fprintf (dump_file, "\n;; Function %s (%s)%s\n\n", dname, aname,
	     cfun->function_frequency == FUNCTION_FREQUENCY_HOT
	     ? " (hot)"
	     : cfun->function_frequency == FUNCTION_FREQUENCY_UNLIKELY_EXECUTED
	     ? " (unlikely executed)"
	     : "");
	}
    }
  else
    initializing_dump = false;

  /* If a timevar is present, start it.  */
  if (pass->tv_id)
    timevar_push (pass->tv_id);

  /* Do it!  */
  if (pass->execute)
    {
      todo_after = pass->execute ();
      last_verified = 0;
    }

  /* Stop timevar.  */
  if (pass->tv_id)
    timevar_pop (pass->tv_id);

  curr_properties = (curr_properties | pass->properties_provided)
		    & ~pass->properties_destroyed;

  if (initializing_dump
      && dump_file
      && graph_dump_format != no_graph
      && (curr_properties & (PROP_cfg | PROP_rtl)) == (PROP_cfg | PROP_rtl))
    {
      get_dump_file_info (pass->static_pass_number)->flags |= TDF_GRAPH;
      dump_flags |= TDF_GRAPH;
      clean_graph_dump_file (dump_file_name);
    }

  /* Run post-pass cleanup and verification.  */
  execute_todo (todo_after | pass->todo_flags_finish);
  verify_interpass_invariants ();

  /* Flush and close dump file.  */
  if (dump_file_name)
    {
      free ((char *) dump_file_name);
      dump_file_name = NULL;
    }
  if (dump_file)
    {
      dump_end (pass->static_pass_number, dump_file);
      dump_file = NULL;
    }

  if (pass->properties_destroyed & PROP_smt_usage)
    updating_used_alone = false;

  /* Reset in_gimple_form to not break non-unit-at-a-time mode.  */
  in_gimple_form = false;

  return true;
}

void
execute_pass_list (struct tree_opt_pass *pass)
{
  do
    {
      if (execute_one_pass (pass) && pass->sub)
        execute_pass_list (pass->sub);
      pass = pass->next;
    }
  while (pass);
}

/* APPLE LOCAL begin MERGE FIXME 4708086 - 4095567 4229407 4120689 */

#if 0
/* This function is called from the pass manager in tree-optimize.c
   after all tree passes have finished for a single function, and we
   have expanded the function body from trees to RTL.
   Once we are here, we have decided that we're supposed to output
   that function, i.e. that we should write assembler code for it.

   We run a series of low-level passes here on the function's RTL
   representation.  Each pass is called via a rest_of_* function.  */

static void
rest_of_compilation (void)
{
  /* If we're emitting a nested function, make sure its parent gets
     emitted as well.  Doing otherwise confuses debug info.  */
  {
    tree parent;
    for (parent = DECL_CONTEXT (current_function_decl);
	 parent != NULL_TREE;
	 parent = get_containing_scope (parent))
      if (TREE_CODE (parent) == FUNCTION_DECL)
	TREE_SYMBOL_REFERENCED (DECL_ASSEMBLER_NAME (parent)) = 1;
  }

  /* We are now committed to emitting code for this function.  Do any
     preparation, such as emitting abstract debug info for the inline
     before it gets mangled by optimization.  */
/* APPLE LOCAL begin mainline 2006-05-15 rewrite 4548482  */
  if (cgraph_function_possibly_inlined_p (current_function_decl))
    (*debug_hooks->outlining_inline_function) (current_function_decl, NULL);
/* APPLE LOCAL end mainline 2006-05-15 rewrite 4548482  */

  /* Remove any notes we don't need.  That will make iterating
     over the instruction sequence faster, and allow the garbage
     collector to reclaim the memory used by the notes.  */
  remove_unnecessary_notes ();

  /* Initialize some variables used by the optimizers.  */
  init_function_for_compilation ();

  TREE_ASM_WRITTEN (current_function_decl) = 1;

  /* Early return if there were errors.  We can run afoul of our
     consistency checks, and there's not really much point in fixing them.  */
  if (rtl_dump_and_exit || flag_syntax_only || errorcount || sorrycount)
    goto exit_rest_of_compilation;

  rest_of_handle_jump ();

  rest_of_handle_eh ();

  /* Delay emitting hard_reg_initial_value sets until after EH landing pad
     generation, which might create new sets.  */
  emit_initial_value_sets ();

#ifdef FINALIZE_PIC
  /* If we are doing position-independent code generation, now
     is the time to output special prologues and epilogues.
     We do not want to do this earlier, because it just clutters
     up inline functions with meaningless insns.  */
  if (flag_pic)
    FINALIZE_PIC;
#endif

  /* Copy any shared structure that should not be shared.  */
  unshare_all_rtl ();

#ifdef SETJMP_VIA_SAVE_AREA
  /* This must be performed before virtual register instantiation.
     Please be aware that everything in the compiler that can look
     at the RTL up to this point must understand that REG_SAVE_AREA
     is just like a use of the REG contained inside.  */
  if (current_function_calls_alloca)
    optimize_save_area_alloca ();
#endif

  /* Instantiate all virtual registers.  */
  instantiate_virtual_regs ();

  rest_of_handle_jump2 ();

  if (optimize > 0)
    rest_of_handle_cse ();

  if (optimize > 0)
    {
      if (flag_gcse)
	rest_of_handle_gcse ();

      if (flag_loop_optimize)
	rest_of_handle_loop_optimize ();

      if (flag_gcse)
	rest_of_handle_jump_bypass ();
    }

  timevar_push (TV_FLOW);
  rest_of_handle_cfg ();

  if (!flag_tree_based_profiling
      && (optimize > 0 || profile_arc_flag
	  || flag_test_coverage || flag_branch_probabilities))
    {
      rtl_register_profile_hooks ();
      rtl_register_value_prof_hooks ();
      rest_of_handle_branch_prob ();

      if (flag_branch_probabilities
	  && flag_profile_values
	  && (flag_value_profile_transformations
	      || flag_speculative_prefetching))
	rest_of_handle_value_profile_transformations ();

      /* Remove the death notes created for vpt.  */
      if (flag_profile_values)
	count_or_remove_death_notes (NULL, 1);
    }

  if (optimize > 0)
    rest_of_handle_if_conversion ();

  if (optimize > 0 && flag_tracer)
    rest_of_handle_tracer ();

  if (optimize > 0
      && flag_loop_optimize2)
    rest_of_handle_loop2 ();

  if (optimize > 0 && flag_web)
    rest_of_handle_web ();

  if (optimize > 0 && flag_rerun_cse_after_loop)
    rest_of_handle_cse2 ();

  cse_not_expected = 1;

  rest_of_handle_life ();
  timevar_pop (TV_FLOW);

  if (optimize > 0)
    rest_of_handle_combine ();

  if (optimize > 0 && flag_if_conversion)
    rest_of_handle_if_after_combine ();

  /* The optimization to partition hot/cold basic blocks into separate
     sections of the .o file does not work well with linkonce or with
     user defined section attributes.  Don't call it if either case
     arises.  */

  if (flag_reorder_blocks_and_partition 
      && !DECL_ONE_ONLY (current_function_decl)
      && !user_defined_section_attribute)
    rest_of_handle_partition_blocks ();

  if (optimize > 0 && flag_regmove)
    rest_of_handle_regmove ();

  /* Do unconditional splitting before register allocation to allow machine
     description to add extra information not needed previously.  */
  split_all_insns (1);

#ifdef OPTIMIZE_MODE_SWITCHING
  rest_of_handle_mode_switching ();
#endif

  /* Any of the several passes since flow1 will have munged register
     lifetime data a bit.  We need it to be up to date for scheduling
     (see handling of reg_known_equiv in init_alias_analysis).  */
  recompute_reg_usage ();

#ifdef INSN_SCHEDULING
  if (optimize > 0 && flag_modulo_sched)
    rest_of_handle_sms ();

  if (flag_schedule_insns)
    rest_of_handle_sched ();
#endif

  /* Determine if the current function is a leaf before running reload
     since this can impact optimizations done by the prologue and
     epilogue thus changing register elimination offsets.  */
  current_function_is_leaf = leaf_function_p ();

  if (rest_of_handle_old_regalloc ())
    goto exit_rest_of_compilation;

  if (optimize > 0)
    rest_of_handle_postreload ();

  if (optimize > 0 && flag_gcse_after_reload)
    rest_of_handle_gcse2 ();

  rest_of_handle_flow2 ();

#ifdef HAVE_peephole2
  if (optimize > 0 && flag_peephole2)
    rest_of_handle_peephole2 ();
#endif

  if (optimize > 0)
    rest_of_handle_if_after_reload ();

  if (optimize > 0)
    {
      if (flag_rename_registers || flag_cprop_registers)
	rest_of_handle_regrename ();

      rest_of_handle_reorder_blocks ();
    }

  if (flag_branch_target_load_optimize2)
    rest_of_handle_branch_target_load_optimize ();

#ifdef LEAF_REGISTERS
  current_function_uses_only_leaf_regs
    = optimize > 0 && only_leaf_regs_used () && leaf_function_p ();
#endif

#ifdef INSN_SCHEDULING
  if (optimize > 0 && flag_schedule_insns_after_reload)
    rest_of_handle_sched2 ();
#endif

#ifdef STACK_REGS
  rest_of_handle_stack_regs ();
#endif

  compute_alignments ();

  /* Aggressively duplicate basic blocks ending in computed gotos to the
     tails of their predecessors, unless we are optimizing for size.  */
  if (flag_expensive_optimizations && !optimize_size)
    duplicate_computed_gotos ();

  if (flag_var_tracking)
    rest_of_handle_variable_tracking ();

  /* CFG is no longer maintained up-to-date.  */
  free_bb_for_insn ();

  if (targetm.machine_dependent_reorg != 0)
    rest_of_handle_machine_reorg ();

  purge_line_number_notes (get_insns ());
  cleanup_barriers ();

#ifdef DELAY_SLOTS
  if (flag_delayed_branch)
    rest_of_handle_delay_slots ();
#endif

#if defined (HAVE_ATTR_length) && !defined (STACK_REGS)
  timevar_push (TV_SHORTEN_BRANCH);
  split_all_insns_noflow ();
  timevar_pop (TV_SHORTEN_BRANCH);
#endif

  convert_to_eh_region_ranges ();

  rest_of_handle_shorten_branches ();

  set_nothrow_function_flags ();

  rest_of_handle_final ();

 exit_rest_of_compilation:

  rest_of_clean_state ();
}
#endif
/* APPLE LOCAL end MERGE FIXME 4708086 - 4095567 4229407 4120689 */

/* Same as execute_pass_list but assume that subpasses of IPA passes
   are local passes.  */
void
execute_ipa_pass_list (struct tree_opt_pass *pass)
{
  do
    {
      if (execute_one_pass (pass) && pass->sub)
	{
	  struct cgraph_node *node;
	  for (node = cgraph_nodes; node; node = node->next)
	    if (node->analyzed)
	      {
		push_cfun (DECL_STRUCT_FUNCTION (node->decl));
		current_function_decl = node->decl;
		execute_pass_list (pass->sub);
		free_dominance_info (CDI_DOMINATORS);
		free_dominance_info (CDI_POST_DOMINATORS);
		current_function_decl = NULL;
		pop_cfun ();
		ggc_collect ();
	      }
	}
      pass = pass->next;
    }
  while (pass);
}
