/* LLVM LOCAL begin (ENTIRE FILE!)  */
/* Some target-specific hooks for gcc->llvm conversion
Copyright (C) 2008 Free Software Foundation, Inc.
Contributed by Bruno Cardoso Lopes (bruno.cardoso@gmail.com)

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
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#ifdef LLVM_ABI_H

extern bool llvm_mips_should_pass_aggregate_in_memory(tree, const Type *);

#define LLVM_SHOULD_PASS_AGGREGATE_USING_BYVAL_ATTR(X, TY)      \
  llvm_mips_should_pass_aggregate_in_memory(X, TY)

/* LLVM_TARGET_NAME - This specifies the name of the target, which correlates to
 * the llvm::InitializeXXXTarget() function.
 */
#define LLVM_TARGET_NAME Mips

#endif /* LLVM_ABI_H */

/* LLVM LOCAL end (ENTIRE FILE!)  */
