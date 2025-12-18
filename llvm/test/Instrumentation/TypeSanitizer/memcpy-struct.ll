; Instrumenting memcpy intrinsic with attached tbaa.struct metadata.
; RUN: opt -passes='tysan' -tysan-outline-instrumentation=false -S %s | FileCheck %s --check-prefixes=CHECK,CHECK-INLINE
; RUN: opt -passes='tysan' -tysan-outline-instrumentation=true -S %s | FileCheck %s --check-prefixes=CHECK,CHECK-OUTLINE

; ModuleID = 'struct-copy.c'
source_filename = "struct-copy.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-unknown"

%struct.S = type { i64, i16, double, %struct.Inner }
%struct.Inner = type { i64, i64 }

; Function Attrs: noinline nounwind optnone sanitize_type
define dso_local i32 @main() #0 {
; CHECK-LABEL: define dso_local i32 @main()
entry:
  %a = alloca %struct.S, align 8
; CHECK: %a = alloca %struct.S, align 8
; CHECK-OUTLINE-NEXT: call void @__tysan_instrument_mem_inst(ptr %a, ptr null, i64 40, i1 false)
; CHECK-INLINE-NEXT: %0 = ptrtoint ptr %a to i64
; CHECK-INLINE-NEXT: %1 = and i64 %0, %app.mem.mask
; CHECK-INLINE-NEXT: %2 = shl i64 %1, 3
; CHECK-INLINE-NEXT: %3 = add i64 %2, %shadow.base
; CHECK-INLINE-NEXT: %4 = inttoptr i64 %3 to ptr
; CHECK-INLINE-NEXT: call void @llvm.memset.p0.i64(ptr align 8 %4, i8 0, i64 320, i1 false)
  %b = alloca %struct.S, align 8
; CHECK-NEXT: %b = alloca %struct.S, align 8
; CHECK-OUTLINE-NEXT: call void @__tysan_instrument_mem_inst(ptr %b, ptr null, i64 40, i1 false)
; CHECK-INLINE-NEXT: %5 = ptrtoint ptr %b to i64
; CHECK-INLINE-NEXT: %6 = and i64 %5, %app.mem.mask
; CHECK-INLINE-NEXT: %7 = shl i64 %6, 3
; CHECK-INLINE-NEXT: %8 = add i64 %7, %shadow.base
; CHECK-INLINE-NEXT: %9 = inttoptr i64 %8 to ptr
; CHECK-INLINE-NEXT: call void @llvm.memset.p0.i64(ptr align 8 %9, i8 0, i64 320, i1 false)
; (no tysan.base_type_tbaa metadata)
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %b, ptr align 8 %a, i64 40, i1 false), !tbaa.struct !6
; CHECK-OUTLINE-NEXT: call void @__tysan_set_field_shadow_type(ptr %b, ptr @__tysan_v1_long_o_0, i64 8, i64 0)
; CHECK-OUTLINE-NEXT: call void @__tysan_set_field_shadow_type(ptr %b, ptr @__tysan_v1_short_o_0, i64 2, i64 8)
; CHECK-OUTLINE-NEXT: call void @__tysan_set_field_shadow_type(ptr %b, ptr @__tysan_v1_double_o_0, i64 8, i64 16)
; CHECK-OUTLINE-NEXT: call void @__tysan_set_field_shadow_type(ptr %b, ptr @__tysan_v1_long_o_0, i64 8, i64 24)
; CHECK-OUTLINE-NEXT: call void @__tysan_set_field_shadow_type(ptr %b, ptr @__tysan_v1_long_o_0, i64 8, i64 32)
; CHECK-INLINE-NEXT: %app.ptr.int = ptrtoint ptr %b to i64
; CHECK-INLINE-NEXT: %app.ptr.masked = and i64 %app.ptr.int, %app.mem.mask
; CHECK-INLINE-NEXT: %app.ptr.shifted = shl i64 %app.ptr.masked, 3
; CHECK-INLINE-NEXT: %shadow.ptr.int = add i64 %app.ptr.shifted, %shadow.base
; CHECK-INLINE-NEXT: %shadow.ptr = inttoptr i64 %shadow.ptr.int to ptr
; CHECK-INLINE-NEXT: store ptr @__tysan_v1_long_o_0, ptr %shadow.ptr, align 8
; CHECK-INLINE-NEXT: %shadow.byte.1.offset = add i64 %shadow.ptr.int, 8
; CHECK-INLINE-NEXT: %shadow.byte.1.ptr = inttoptr i64 %shadow.byte.1.offset to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -1 to ptr), ptr %shadow.byte.1.ptr, align 8
; CHECK-INLINE-NEXT: %shadow.byte.2.offset = add i64 %shadow.ptr.int, 16
; CHECK-INLINE-NEXT: %shadow.byte.2.ptr = inttoptr i64 %shadow.byte.2.offset to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -2 to ptr), ptr %shadow.byte.2.ptr, align 8
; CHECK-INLINE-NEXT: %shadow.byte.3.offset = add i64 %shadow.ptr.int, 24
; CHECK-INLINE-NEXT: %shadow.byte.3.ptr = inttoptr i64 %shadow.byte.3.offset to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -3 to ptr), ptr %shadow.byte.3.ptr, align 8
; CHECK-INLINE-NEXT: %shadow.byte.4.offset = add i64 %shadow.ptr.int, 32
; CHECK-INLINE-NEXT: %shadow.byte.4.ptr = inttoptr i64 %shadow.byte.4.offset to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -4 to ptr), ptr %shadow.byte.4.ptr, align 8
; CHECK-INLINE-NEXT: %shadow.byte.5.offset = add i64 %shadow.ptr.int, 40
; CHECK-INLINE-NEXT: %shadow.byte.5.ptr = inttoptr i64 %shadow.byte.5.offset to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -5 to ptr), ptr %shadow.byte.5.ptr, align 8
; CHECK-INLINE-NEXT: %shadow.byte.6.offset = add i64 %shadow.ptr.int, 48
; CHECK-INLINE-NEXT: %shadow.byte.6.ptr = inttoptr i64 %shadow.byte.6.offset to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -6 to ptr), ptr %shadow.byte.6.ptr, align 8
; CHECK-INLINE-NEXT: %shadow.byte.7.offset = add i64 %shadow.ptr.int, 56
; CHECK-INLINE-NEXT: %shadow.byte.7.ptr = inttoptr i64 %shadow.byte.7.offset to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -7 to ptr), ptr %shadow.byte.7.ptr, align 8
; CHECK-INLINE-NEXT: %10 = ptrtoint ptr %b to i64
; CHECK-INLINE-NEXT: %11 = add i64 %10, 8
; CHECK-INLINE-NEXT: %12 = inttoptr i64 %11 to ptr
; CHECK-INLINE-NEXT: %app.ptr.int1 = ptrtoint ptr %12 to i64
; CHECK-INLINE-NEXT: %app.ptr.masked2 = and i64 %app.ptr.int1, %app.mem.mask
; CHECK-INLINE-NEXT: %app.ptr.shifted3 = shl i64 %app.ptr.masked2, 3
; CHECK-INLINE-NEXT: %shadow.ptr.int4 = add i64 %app.ptr.shifted3, %shadow.base
; CHECK-INLINE-NEXT: %shadow.ptr5 = inttoptr i64 %shadow.ptr.int4 to ptr
; CHECK-INLINE-NEXT: store ptr @__tysan_v1_short_o_0, ptr %shadow.ptr5, align 8
; CHECK-INLINE-NEXT: %shadow.byte.1.offset6 = add i64 %shadow.ptr.int4, 8
; CHECK-INLINE-NEXT: %shadow.byte.1.ptr7 = inttoptr i64 %shadow.byte.1.offset6 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -1 to ptr), ptr %shadow.byte.1.ptr7, align 8
; CHECK-INLINE-NEXT: %13 = ptrtoint ptr %b to i64
; CHECK-INLINE-NEXT: %14 = add i64 %13, 16
; CHECK-INLINE-NEXT: %15 = inttoptr i64 %14 to ptr
; CHECK-INLINE-NEXT: %app.ptr.int8 = ptrtoint ptr %15 to i64
; CHECK-INLINE-NEXT: %app.ptr.masked9 = and i64 %app.ptr.int8, %app.mem.mask
; CHECK-INLINE-NEXT: %app.ptr.shifted10 = shl i64 %app.ptr.masked9, 3
; CHECK-INLINE-NEXT: %shadow.ptr.int11 = add i64 %app.ptr.shifted10, %shadow.base
; CHECK-INLINE-NEXT: %shadow.ptr12 = inttoptr i64 %shadow.ptr.int11 to ptr
; CHECK-INLINE-NEXT: store ptr @__tysan_v1_double_o_0, ptr %shadow.ptr12, align 8
; CHECK-INLINE-NEXT: %shadow.byte.1.offset13 = add i64 %shadow.ptr.int11, 8
; CHECK-INLINE-NEXT: %shadow.byte.1.ptr14 = inttoptr i64 %shadow.byte.1.offset13 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -1 to ptr), ptr %shadow.byte.1.ptr14, align 8
; CHECK-INLINE-NEXT: %shadow.byte.2.offset15 = add i64 %shadow.ptr.int11, 16
; CHECK-INLINE-NEXT: %shadow.byte.2.ptr16 = inttoptr i64 %shadow.byte.2.offset15 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -2 to ptr), ptr %shadow.byte.2.ptr16, align 8
; CHECK-INLINE-NEXT: %shadow.byte.3.offset17 = add i64 %shadow.ptr.int11, 24
; CHECK-INLINE-NEXT: %shadow.byte.3.ptr18 = inttoptr i64 %shadow.byte.3.offset17 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -3 to ptr), ptr %shadow.byte.3.ptr18, align 8
; CHECK-INLINE-NEXT: %shadow.byte.4.offset19 = add i64 %shadow.ptr.int11, 32
; CHECK-INLINE-NEXT: %shadow.byte.4.ptr20 = inttoptr i64 %shadow.byte.4.offset19 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -4 to ptr), ptr %shadow.byte.4.ptr20, align 8
; CHECK-INLINE-NEXT: %shadow.byte.5.offset21 = add i64 %shadow.ptr.int11, 40
; CHECK-INLINE-NEXT: %shadow.byte.5.ptr22 = inttoptr i64 %shadow.byte.5.offset21 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -5 to ptr), ptr %shadow.byte.5.ptr22, align 8
; CHECK-INLINE-NEXT: %shadow.byte.6.offset23 = add i64 %shadow.ptr.int11, 48
; CHECK-INLINE-NEXT: %shadow.byte.6.ptr24 = inttoptr i64 %shadow.byte.6.offset23 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -6 to ptr), ptr %shadow.byte.6.ptr24, align 8
; CHECK-INLINE-NEXT: %shadow.byte.7.offset25 = add i64 %shadow.ptr.int11, 56
; CHECK-INLINE-NEXT: %shadow.byte.7.ptr26 = inttoptr i64 %shadow.byte.7.offset25 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -7 to ptr), ptr %shadow.byte.7.ptr26, align 8
; CHECK-INLINE-NEXT: %16 = ptrtoint ptr %b to i64
; CHECK-INLINE-NEXT: %17 = add i64 %16, 24
; CHECK-INLINE-NEXT: %18 = inttoptr i64 %17 to ptr
; CHECK-INLINE-NEXT: %app.ptr.int27 = ptrtoint ptr %18 to i64
; CHECK-INLINE-NEXT: %app.ptr.masked28 = and i64 %app.ptr.int27, %app.mem.mask
; CHECK-INLINE-NEXT: %app.ptr.shifted29 = shl i64 %app.ptr.masked28, 3
; CHECK-INLINE-NEXT: %shadow.ptr.int30 = add i64 %app.ptr.shifted29, %shadow.base
; CHECK-INLINE-NEXT: %shadow.ptr31 = inttoptr i64 %shadow.ptr.int30 to ptr
; CHECK-INLINE-NEXT: store ptr @__tysan_v1_long_o_0, ptr %shadow.ptr31, align 8
; CHECK-INLINE-NEXT: %shadow.byte.1.offset32 = add i64 %shadow.ptr.int30, 8
; CHECK-INLINE-NEXT: %shadow.byte.1.ptr33 = inttoptr i64 %shadow.byte.1.offset32 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -1 to ptr), ptr %shadow.byte.1.ptr33, align 8
; CHECK-INLINE-NEXT: %shadow.byte.2.offset34 = add i64 %shadow.ptr.int30, 16
; CHECK-INLINE-NEXT: %shadow.byte.2.ptr35 = inttoptr i64 %shadow.byte.2.offset34 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -2 to ptr), ptr %shadow.byte.2.ptr35, align 8
; CHECK-INLINE-NEXT: %shadow.byte.3.offset36 = add i64 %shadow.ptr.int30, 24
; CHECK-INLINE-NEXT: %shadow.byte.3.ptr37 = inttoptr i64 %shadow.byte.3.offset36 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -3 to ptr), ptr %shadow.byte.3.ptr37, align 8
; CHECK-INLINE-NEXT: %shadow.byte.4.offset38 = add i64 %shadow.ptr.int30, 32
; CHECK-INLINE-NEXT: %shadow.byte.4.ptr39 = inttoptr i64 %shadow.byte.4.offset38 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -4 to ptr), ptr %shadow.byte.4.ptr39, align 8
; CHECK-INLINE-NEXT: %shadow.byte.5.offset40 = add i64 %shadow.ptr.int30, 40
; CHECK-INLINE-NEXT: %shadow.byte.5.ptr41 = inttoptr i64 %shadow.byte.5.offset40 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -5 to ptr), ptr %shadow.byte.5.ptr41, align 8
; CHECK-INLINE-NEXT: %shadow.byte.6.offset42 = add i64 %shadow.ptr.int30, 48
; CHECK-INLINE-NEXT: %shadow.byte.6.ptr43 = inttoptr i64 %shadow.byte.6.offset42 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -6 to ptr), ptr %shadow.byte.6.ptr43, align 8
; CHECK-INLINE-NEXT: %shadow.byte.7.offset44 = add i64 %shadow.ptr.int30, 56
; CHECK-INLINE-NEXT: %shadow.byte.7.ptr45 = inttoptr i64 %shadow.byte.7.offset44 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -7 to ptr), ptr %shadow.byte.7.ptr45, align 8
; CHECK-INLINE-NEXT: %19 = ptrtoint ptr %b to i64
; CHECK-INLINE-NEXT: %20 = add i64 %19, 32
; CHECK-INLINE-NEXT: %21 = inttoptr i64 %20 to ptr
; CHECK-INLINE-NEXT: %app.ptr.int46 = ptrtoint ptr %21 to i64
; CHECK-INLINE-NEXT: %app.ptr.masked47 = and i64 %app.ptr.int46, %app.mem.mask
; CHECK-INLINE-NEXT: %app.ptr.shifted48 = shl i64 %app.ptr.masked47, 3
; CHECK-INLINE-NEXT: %shadow.ptr.int49 = add i64 %app.ptr.shifted48, %shadow.base
; CHECK-INLINE-NEXT: %shadow.ptr50 = inttoptr i64 %shadow.ptr.int49 to ptr
; CHECK-INLINE-NEXT: store ptr @__tysan_v1_long_o_0, ptr %shadow.ptr50, align 8
; CHECK-INLINE-NEXT: %shadow.byte.1.offset51 = add i64 %shadow.ptr.int49, 8
; CHECK-INLINE-NEXT: %shadow.byte.1.ptr52 = inttoptr i64 %shadow.byte.1.offset51 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -1 to ptr), ptr %shadow.byte.1.ptr52, align 8
; CHECK-INLINE-NEXT: %shadow.byte.2.offset53 = add i64 %shadow.ptr.int49, 16
; CHECK-INLINE-NEXT: %shadow.byte.2.ptr54 = inttoptr i64 %shadow.byte.2.offset53 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -2 to ptr), ptr %shadow.byte.2.ptr54, align 8
; CHECK-INLINE-NEXT: %shadow.byte.3.offset55 = add i64 %shadow.ptr.int49, 24
; CHECK-INLINE-NEXT: %shadow.byte.3.ptr56 = inttoptr i64 %shadow.byte.3.offset55 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -3 to ptr), ptr %shadow.byte.3.ptr56, align 8
; CHECK-INLINE-NEXT: %shadow.byte.4.offset57 = add i64 %shadow.ptr.int49, 32
; CHECK-INLINE-NEXT: %shadow.byte.4.ptr58 = inttoptr i64 %shadow.byte.4.offset57 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -4 to ptr), ptr %shadow.byte.4.ptr58, align 8
; CHECK-INLINE-NEXT: %shadow.byte.5.offset59 = add i64 %shadow.ptr.int49, 40
; CHECK-INLINE-NEXT: %shadow.byte.5.ptr60 = inttoptr i64 %shadow.byte.5.offset59 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -5 to ptr), ptr %shadow.byte.5.ptr60, align 8
; CHECK-INLINE-NEXT: %shadow.byte.6.offset61 = add i64 %shadow.ptr.int49, 48
; CHECK-INLINE-NEXT: %shadow.byte.6.ptr62 = inttoptr i64 %shadow.byte.6.offset61 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -6 to ptr), ptr %shadow.byte.6.ptr62, align 8
; CHECK-INLINE-NEXT: %shadow.byte.7.offset63 = add i64 %shadow.ptr.int49, 56
; CHECK-INLINE-NEXT: %shadow.byte.7.ptr64 = inttoptr i64 %shadow.byte.7.offset63 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -7 to ptr), ptr %shadow.byte.7.ptr64, align 8
; CHECK-NEXT: call void @llvm.memcpy.p0.p0.i64(ptr align 8 %b, ptr align 8 %a, i64 40, i1 false), !tbaa.struct !6
; (with tysan.base_type_tbaa metadata)
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %b, ptr align 8 %a, i64 40, i1 false), !tbaa.struct !6, !tysan.base_type_tbaa !13
; CHECK-OUTLINE-NEXT: call void @__tysan_set_field_shadow_type(ptr %b, ptr @__tysan_v1_S_o_0, i64 8, i64 0)
; CHECK-OUTLINE-NEXT: call void @__tysan_set_field_shadow_type(ptr %b, ptr @__tysan_v1_S_o_8, i64 2, i64 8)
; CHECK-OUTLINE-NEXT: call void @__tysan_set_field_shadow_type(ptr %b, ptr @__tysan_v1_S_o_16, i64 8, i64 16)
; CHECK-OUTLINE-NEXT: call void @__tysan_set_field_shadow_type(ptr %b, ptr @__tysan_v1_S_o_24, i64 8, i64 24)
; CHECK-OUTLINE-NEXT: call void @__tysan_set_field_shadow_type(ptr %b, ptr @__tysan_v1_S_o_32, i64 8, i64 32)
; CHECK-INLINE-NEXT: %app.ptr.int65 = ptrtoint ptr %b to i64
; CHECK-INLINE-NEXT: %app.ptr.masked66 = and i64 %app.ptr.int65, %app.mem.mask
; CHECK-INLINE-NEXT: %app.ptr.shifted67 = shl i64 %app.ptr.masked66, 3
; CHECK-INLINE-NEXT: %shadow.ptr.int68 = add i64 %app.ptr.shifted67, %shadow.base
; CHECK-INLINE-NEXT: %shadow.ptr69 = inttoptr i64 %shadow.ptr.int68 to ptr
; CHECK-INLINE-NEXT: store ptr @__tysan_v1_S_o_0, ptr %shadow.ptr69, align 8
; CHECK-INLINE-NEXT: %shadow.byte.1.offset70 = add i64 %shadow.ptr.int68, 8
; CHECK-INLINE-NEXT: %shadow.byte.1.ptr71 = inttoptr i64 %shadow.byte.1.offset70 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -1 to ptr), ptr %shadow.byte.1.ptr71, align 8
; CHECK-INLINE-NEXT: %shadow.byte.2.offset72 = add i64 %shadow.ptr.int68, 16
; CHECK-INLINE-NEXT: %shadow.byte.2.ptr73 = inttoptr i64 %shadow.byte.2.offset72 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -2 to ptr), ptr %shadow.byte.2.ptr73, align 8
; CHECK-INLINE-NEXT: %shadow.byte.3.offset74 = add i64 %shadow.ptr.int68, 24
; CHECK-INLINE-NEXT: %shadow.byte.3.ptr75 = inttoptr i64 %shadow.byte.3.offset74 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -3 to ptr), ptr %shadow.byte.3.ptr75, align 8
; CHECK-INLINE-NEXT: %shadow.byte.4.offset76 = add i64 %shadow.ptr.int68, 32
; CHECK-INLINE-NEXT: %shadow.byte.4.ptr77 = inttoptr i64 %shadow.byte.4.offset76 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -4 to ptr), ptr %shadow.byte.4.ptr77, align 8
; CHECK-INLINE-NEXT: %shadow.byte.5.offset78 = add i64 %shadow.ptr.int68, 40
; CHECK-INLINE-NEXT: %shadow.byte.5.ptr79 = inttoptr i64 %shadow.byte.5.offset78 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -5 to ptr), ptr %shadow.byte.5.ptr79, align 8
; CHECK-INLINE-NEXT: %shadow.byte.6.offset80 = add i64 %shadow.ptr.int68, 48
; CHECK-INLINE-NEXT: %shadow.byte.6.ptr81 = inttoptr i64 %shadow.byte.6.offset80 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -6 to ptr), ptr %shadow.byte.6.ptr81, align 8
; CHECK-INLINE-NEXT: %shadow.byte.7.offset82 = add i64 %shadow.ptr.int68, 56
; CHECK-INLINE-NEXT: %shadow.byte.7.ptr83 = inttoptr i64 %shadow.byte.7.offset82 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -7 to ptr), ptr %shadow.byte.7.ptr83, align 8
; CHECK-INLINE-NEXT: %22 = ptrtoint ptr %b to i64
; CHECK-INLINE-NEXT: %23 = add i64 %22, 8
; CHECK-INLINE-NEXT: %24 = inttoptr i64 %23 to ptr
; CHECK-INLINE-NEXT: %app.ptr.int84 = ptrtoint ptr %24 to i64
; CHECK-INLINE-NEXT: %app.ptr.masked85 = and i64 %app.ptr.int84, %app.mem.mask
; CHECK-INLINE-NEXT: %app.ptr.shifted86 = shl i64 %app.ptr.masked85, 3
; CHECK-INLINE-NEXT: %shadow.ptr.int87 = add i64 %app.ptr.shifted86, %shadow.base
; CHECK-INLINE-NEXT: %shadow.ptr88 = inttoptr i64 %shadow.ptr.int87 to ptr
; CHECK-INLINE-NEXT: store ptr @__tysan_v1_S_o_8, ptr %shadow.ptr88, align 8
; CHECK-INLINE-NEXT: %shadow.byte.1.offset89 = add i64 %shadow.ptr.int87, 8
; CHECK-INLINE-NEXT: %shadow.byte.1.ptr90 = inttoptr i64 %shadow.byte.1.offset89 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -1 to ptr), ptr %shadow.byte.1.ptr90, align 8
; CHECK-INLINE-NEXT: %25 = ptrtoint ptr %b to i64
; CHECK-INLINE-NEXT: %26 = add i64 %25, 16
; CHECK-INLINE-NEXT: %27 = inttoptr i64 %26 to ptr
; CHECK-INLINE-NEXT: %app.ptr.int91 = ptrtoint ptr %27 to i64
; CHECK-INLINE-NEXT: %app.ptr.masked92 = and i64 %app.ptr.int91, %app.mem.mask
; CHECK-INLINE-NEXT: %app.ptr.shifted93 = shl i64 %app.ptr.masked92, 3
; CHECK-INLINE-NEXT: %shadow.ptr.int94 = add i64 %app.ptr.shifted93, %shadow.base
; CHECK-INLINE-NEXT: %shadow.ptr95 = inttoptr i64 %shadow.ptr.int94 to ptr
; CHECK-INLINE-NEXT: store ptr @__tysan_v1_S_o_16, ptr %shadow.ptr95, align 8
; CHECK-INLINE-NEXT: %shadow.byte.1.offset96 = add i64 %shadow.ptr.int94, 8
; CHECK-INLINE-NEXT: %shadow.byte.1.ptr97 = inttoptr i64 %shadow.byte.1.offset96 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -1 to ptr), ptr %shadow.byte.1.ptr97, align 8
; CHECK-INLINE-NEXT: %shadow.byte.2.offset98 = add i64 %shadow.ptr.int94, 16
; CHECK-INLINE-NEXT: %shadow.byte.2.ptr99 = inttoptr i64 %shadow.byte.2.offset98 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -2 to ptr), ptr %shadow.byte.2.ptr99, align 8
; CHECK-INLINE-NEXT: %shadow.byte.3.offset100 = add i64 %shadow.ptr.int94, 24
; CHECK-INLINE-NEXT: %shadow.byte.3.ptr101 = inttoptr i64 %shadow.byte.3.offset100 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -3 to ptr), ptr %shadow.byte.3.ptr101, align 8
; CHECK-INLINE-NEXT: %shadow.byte.4.offset102 = add i64 %shadow.ptr.int94, 32
; CHECK-INLINE-NEXT: %shadow.byte.4.ptr103 = inttoptr i64 %shadow.byte.4.offset102 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -4 to ptr), ptr %shadow.byte.4.ptr103, align 8
; CHECK-INLINE-NEXT: %shadow.byte.5.offset104 = add i64 %shadow.ptr.int94, 40
; CHECK-INLINE-NEXT: %shadow.byte.5.ptr105 = inttoptr i64 %shadow.byte.5.offset104 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -5 to ptr), ptr %shadow.byte.5.ptr105, align 8
; CHECK-INLINE-NEXT: %shadow.byte.6.offset106 = add i64 %shadow.ptr.int94, 48
; CHECK-INLINE-NEXT: %shadow.byte.6.ptr107 = inttoptr i64 %shadow.byte.6.offset106 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -6 to ptr), ptr %shadow.byte.6.ptr107, align 8
; CHECK-INLINE-NEXT: %shadow.byte.7.offset108 = add i64 %shadow.ptr.int94, 56
; CHECK-INLINE-NEXT: %shadow.byte.7.ptr109 = inttoptr i64 %shadow.byte.7.offset108 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -7 to ptr), ptr %shadow.byte.7.ptr109, align 8
; CHECK-INLINE-NEXT: %28 = ptrtoint ptr %b to i64
; CHECK-INLINE-NEXT: %29 = add i64 %28, 24
; CHECK-INLINE-NEXT: %30 = inttoptr i64 %29 to ptr
; CHECK-INLINE-NEXT: %app.ptr.int110 = ptrtoint ptr %30 to i64
; CHECK-INLINE-NEXT: %app.ptr.masked111 = and i64 %app.ptr.int110, %app.mem.mask
; CHECK-INLINE-NEXT: %app.ptr.shifted112 = shl i64 %app.ptr.masked111, 3
; CHECK-INLINE-NEXT: %shadow.ptr.int113 = add i64 %app.ptr.shifted112, %shadow.base
; CHECK-INLINE-NEXT: %shadow.ptr114 = inttoptr i64 %shadow.ptr.int113 to ptr
; CHECK-INLINE-NEXT: store ptr @__tysan_v1_S_o_24, ptr %shadow.ptr114, align 8
; CHECK-INLINE-NEXT: %shadow.byte.1.offset115 = add i64 %shadow.ptr.int113, 8
; CHECK-INLINE-NEXT: %shadow.byte.1.ptr116 = inttoptr i64 %shadow.byte.1.offset115 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -1 to ptr), ptr %shadow.byte.1.ptr116, align 8
; CHECK-INLINE-NEXT: %shadow.byte.2.offset117 = add i64 %shadow.ptr.int113, 16
; CHECK-INLINE-NEXT: %shadow.byte.2.ptr118 = inttoptr i64 %shadow.byte.2.offset117 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -2 to ptr), ptr %shadow.byte.2.ptr118, align 8
; CHECK-INLINE-NEXT: %shadow.byte.3.offset119 = add i64 %shadow.ptr.int113, 24
; CHECK-INLINE-NEXT: %shadow.byte.3.ptr120 = inttoptr i64 %shadow.byte.3.offset119 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -3 to ptr), ptr %shadow.byte.3.ptr120, align 8
; CHECK-INLINE-NEXT: %shadow.byte.4.offset121 = add i64 %shadow.ptr.int113, 32
; CHECK-INLINE-NEXT: %shadow.byte.4.ptr122 = inttoptr i64 %shadow.byte.4.offset121 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -4 to ptr), ptr %shadow.byte.4.ptr122, align 8
; CHECK-INLINE-NEXT: %shadow.byte.5.offset123 = add i64 %shadow.ptr.int113, 40
; CHECK-INLINE-NEXT: %shadow.byte.5.ptr124 = inttoptr i64 %shadow.byte.5.offset123 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -5 to ptr), ptr %shadow.byte.5.ptr124, align 8
; CHECK-INLINE-NEXT: %shadow.byte.6.offset125 = add i64 %shadow.ptr.int113, 48
; CHECK-INLINE-NEXT: %shadow.byte.6.ptr126 = inttoptr i64 %shadow.byte.6.offset125 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -6 to ptr), ptr %shadow.byte.6.ptr126, align 8
; CHECK-INLINE-NEXT: %shadow.byte.7.offset127 = add i64 %shadow.ptr.int113, 56
; CHECK-INLINE-NEXT: %shadow.byte.7.ptr128 = inttoptr i64 %shadow.byte.7.offset127 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -7 to ptr), ptr %shadow.byte.7.ptr128, align 8
; CHECK-INLINE-NEXT: %31 = ptrtoint ptr %b to i64
; CHECK-INLINE-NEXT: %32 = add i64 %31, 32
; CHECK-INLINE-NEXT: %33 = inttoptr i64 %32 to ptr
; CHECK-INLINE-NEXT: %app.ptr.int129 = ptrtoint ptr %33 to i64
; CHECK-INLINE-NEXT: %app.ptr.masked130 = and i64 %app.ptr.int129, %app.mem.mask
; CHECK-INLINE-NEXT: %app.ptr.shifted131 = shl i64 %app.ptr.masked130, 3
; CHECK-INLINE-NEXT: %shadow.ptr.int132 = add i64 %app.ptr.shifted131, %shadow.base
; CHECK-INLINE-NEXT: %shadow.ptr133 = inttoptr i64 %shadow.ptr.int132 to ptr
; CHECK-INLINE-NEXT: store ptr @__tysan_v1_S_o_32, ptr %shadow.ptr133, align 8
; CHECK-INLINE-NEXT: %shadow.byte.1.offset134 = add i64 %shadow.ptr.int132, 8
; CHECK-INLINE-NEXT: %shadow.byte.1.ptr135 = inttoptr i64 %shadow.byte.1.offset134 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -1 to ptr), ptr %shadow.byte.1.ptr135, align 8
; CHECK-INLINE-NEXT: %shadow.byte.2.offset136 = add i64 %shadow.ptr.int132, 16
; CHECK-INLINE-NEXT: %shadow.byte.2.ptr137 = inttoptr i64 %shadow.byte.2.offset136 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -2 to ptr), ptr %shadow.byte.2.ptr137, align 8
; CHECK-INLINE-NEXT: %shadow.byte.3.offset138 = add i64 %shadow.ptr.int132, 24
; CHECK-INLINE-NEXT: %shadow.byte.3.ptr139 = inttoptr i64 %shadow.byte.3.offset138 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -3 to ptr), ptr %shadow.byte.3.ptr139, align 8
; CHECK-INLINE-NEXT: %shadow.byte.4.offset140 = add i64 %shadow.ptr.int132, 32
; CHECK-INLINE-NEXT: %shadow.byte.4.ptr141 = inttoptr i64 %shadow.byte.4.offset140 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -4 to ptr), ptr %shadow.byte.4.ptr141, align 8
; CHECK-INLINE-NEXT: %shadow.byte.5.offset142 = add i64 %shadow.ptr.int132, 40
; CHECK-INLINE-NEXT: %shadow.byte.5.ptr143 = inttoptr i64 %shadow.byte.5.offset142 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -5 to ptr), ptr %shadow.byte.5.ptr143, align 8
; CHECK-INLINE-NEXT: %shadow.byte.6.offset144 = add i64 %shadow.ptr.int132, 48
; CHECK-INLINE-NEXT: %shadow.byte.6.ptr145 = inttoptr i64 %shadow.byte.6.offset144 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -6 to ptr), ptr %shadow.byte.6.ptr145, align 8
; CHECK-INLINE-NEXT: %shadow.byte.7.offset146 = add i64 %shadow.ptr.int132, 56
; CHECK-INLINE-NEXT: %shadow.byte.7.ptr147 = inttoptr i64 %shadow.byte.7.offset146 to ptr
; CHECK-INLINE-NEXT: store ptr inttoptr (i64 -7 to ptr), ptr %shadow.byte.7.ptr147, align 8
; CHECK-NEXT: call void @llvm.memcpy.p0.p0.i64(ptr align 8 %b, ptr align 8 %a, i64 40, i1 false), !tbaa.struct !6, !tysan.base_type_tbaa !13
  ret i32 0
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias writeonly captures(none), ptr noalias readonly captures(none), i64, i1 immarg) #1

attributes #0 = { noinline nounwind optnone sanitize_type "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+cx8,+mmx,+sse,+sse2,+x87" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}
!llvm.errno.tbaa = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 22.0.0git (https://github.com/BStott6/llvm-project 65e344695f4e70185690dc38f1b58a08757872dc)"}
!2 = !{!3, !3, i64 0}
!3 = !{!"int", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C/C++ TBAA"}
!6 = !{i64 0, i64 8, !7, i64 8, i64 2, !9, i64 16, i64 8, !11, i64 24, i64 8, !7, i64 32, i64 8, !7}
!7 = !{!8, !8, i64 0}
!8 = !{!"long", !4, i64 0}
!9 = !{!10, !10, i64 0}
!10 = !{!"short", !4, i64 0}
!11 = !{!12, !12, i64 0}
!12 = !{!"double", !4, i64 0}
!13 = !{!"S", !8, i64 0, !10, i64 8, !12, i64 16, !14, i64 24}
!14 = !{!"Inner", !8, i64 0, !8, i64 8}

