; ModuleID = 'app.c'
source_filename = "app.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: mustprogress nofree norecurse nosync nounwind optsize willreturn memory(none) uwtable
define dso_local i32 @triangleWave(i32 noundef %0, i32 noundef %1, i32 noundef %2) local_unnamed_addr #0 {
  %4 = srem i32 %0, %1
  %5 = icmp slt i32 %4, 0
  %6 = select i1 %5, i32 %1, i32 0
  %7 = add nsw i32 %6, %4
  %8 = sdiv i32 %1, 2
  %9 = icmp slt i32 %7, %8
  %10 = sub nsw i32 %1, %7
  %11 = select i1 %9, i32 %7, i32 %10
  %12 = shl i32 %2, 2
  %13 = mul i32 %12, %11
  %14 = sdiv i32 %13, %1
  %15 = sub nsw i32 %14, %2
  ret i32 %15
}

; Function Attrs: nofree norecurse nosync nounwind optsize memory(none) uwtable
define dso_local i32 @calculateIterations(i32 noundef %0, i32 noundef %1, i32 noundef %2, i32 noundef %3) local_unnamed_addr #1 {
  br label %5

5:                                                ; preds = %4, %20
  %6 = phi i32 [ %0, %4 ], [ %23, %20 ]
  %7 = phi i32 [ %1, %4 ], [ %27, %20 ]
  %8 = phi i32 [ 0, %4 ], [ %28, %20 ]
  %9 = add i32 %6, -8193
  %10 = icmp ult i32 %9, -16385
  %11 = icmp sgt i32 %7, 8192
  %12 = select i1 %10, i1 true, i1 %11
  %13 = icmp slt i32 %7, -8192
  %14 = select i1 %12, i1 true, i1 %13
  br i1 %14, label %30, label %15

15:                                               ; preds = %5
  %16 = mul nsw i32 %6, %6
  %17 = mul nsw i32 %7, %7
  %18 = add nuw nsw i32 %16, %17
  %19 = icmp ugt i32 %18, 67108864
  br i1 %19, label %30, label %20

20:                                               ; preds = %15
  %21 = sub nsw i32 %16, %17
  %22 = sdiv i32 %21, 4096
  %23 = add nsw i32 %22, %2
  %24 = shl nsw i32 %7, 1
  %25 = mul i32 %24, %6
  %26 = sdiv i32 %25, 4096
  %27 = add nsw i32 %26, %3
  %28 = add nuw nsw i32 %8, 1
  %29 = icmp eq i32 %28, 48
  br i1 %29, label %30, label %5

30:                                               ; preds = %15, %5, %20
  %31 = phi i32 [ %8, %15 ], [ %8, %5 ], [ 48, %20 ]
  ret i32 %31
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind optsize willreturn memory(none) uwtable
define dso_local i32 @colorWave(i32 noundef %0) local_unnamed_addr #0 {
  %2 = and i32 %0, 255
  %3 = icmp ult i32 %2, 128
  %4 = shl nuw nsw i32 %2, 1
  %5 = xor i32 %4, 510
  %6 = select i1 %3, i32 %4, i32 %5
  ret i32 %6
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind optsize willreturn memory(none) uwtable
define dso_local i32 @makeColor(i32 noundef %0, i32 noundef %1) local_unnamed_addr #0 {
  %3 = icmp eq i32 %0, 48
  br i1 %3, label %38, label %4

4:                                                ; preds = %2
  %5 = mul nsw i32 %0, 9
  %6 = add nsw i32 %5, %1
  %7 = and i32 %6, 255
  %8 = mul nsw i32 %0, 207
  %9 = sdiv i32 %8, 48
  %10 = add nsw i32 %9, 48
  %11 = icmp ult i32 %7, 128
  %12 = shl nuw nsw i32 %7, 1
  %13 = xor i32 %12, 510
  %14 = select i1 %11, i32 %12, i32 %13
  %15 = mul nsw i32 %14, %10
  %16 = sdiv i32 %15, 255
  %17 = add i32 %6, 85
  %18 = and i32 %17, 255
  %19 = icmp ult i32 %18, 128
  %20 = shl nuw nsw i32 %18, 1
  %21 = xor i32 %20, 510
  %22 = select i1 %19, i32 %20, i32 %21
  %23 = mul nsw i32 %22, %10
  %24 = sdiv i32 %23, 255
  %25 = add i32 %6, 170
  %26 = and i32 %25, 255
  %27 = icmp ult i32 %26, 128
  %28 = shl nuw nsw i32 %26, 1
  %29 = xor i32 %28, 510
  %30 = select i1 %27, i32 %28, i32 %29
  %31 = mul nsw i32 %30, %10
  %32 = sdiv i32 %31, 255
  %33 = shl i32 %16, 16
  %34 = shl i32 %24, 8
  %35 = or i32 %33, %32
  %36 = or i32 %35, %34
  %37 = or i32 %36, -16777216
  br label %38

38:                                               ; preds = %2, %4
  %39 = phi i32 [ %37, %4 ], [ -16580600, %2 ]
  ret i32 %39
}

; Function Attrs: nounwind optsize uwtable
define dso_local void @drawCell(i32 noundef %0, i32 noundef %1, i32 noundef %2) local_unnamed_addr #2 {
  %4 = shl nsw i32 %0, 2
  %5 = or disjoint i32 %4, 1
  %6 = shl nsw i32 %1, 2
  %7 = or disjoint i32 %6, 1
  tail call void @simPutPixel(i32 noundef %5, i32 noundef %7, i32 noundef %2) #5
  %8 = or disjoint i32 %4, 2
  tail call void @simPutPixel(i32 noundef %8, i32 noundef %7, i32 noundef %2) #5
  %9 = or disjoint i32 %6, 2
  tail call void @simPutPixel(i32 noundef %5, i32 noundef %9, i32 noundef %2) #5
  %10 = or disjoint i32 %4, 2
  tail call void @simPutPixel(i32 noundef %10, i32 noundef %9, i32 noundef %2) #5
  ret void
}

; Function Attrs: optsize
declare void @simPutPixel(i32 noundef, i32 noundef, i32 noundef) local_unnamed_addr #3

; Function Attrs: noreturn nounwind optsize uwtable
define dso_local void @app() local_unnamed_addr #4 {
  br label %1

1:                                                ; preds = %48, %0
  %2 = phi i32 [ 0, %0 ], [ %51, %48 ]
  %3 = srem i32 %2, 600
  %4 = icmp slt i32 %3, 0
  %5 = select i1 %4, i32 600, i32 0
  %6 = add nsw i32 %5, %3
  %7 = icmp slt i32 %6, 300
  %8 = sub nsw i32 600, %6
  %9 = select i1 %7, i32 %6, i32 %8
  %10 = mul nsw i32 %9, 880
  %11 = sdiv i32 %10, 600
  %12 = add nsw i32 %11, -3430
  %13 = add nsw i32 %2, 170
  %14 = srem i32 %13, 840
  %15 = icmp slt i32 %14, 0
  %16 = select i1 %15, i32 840, i32 0
  %17 = add nsw i32 %16, %14
  %18 = icmp slt i32 %17, 420
  %19 = sub nsw i32 840, %17
  %20 = select i1 %18, i32 %17, i32 %19
  %21 = mul nsw i32 %20, 1040
  %22 = sdiv i32 %21, 840
  %23 = add nsw i32 %22, 380
  %24 = sdiv i32 %2, 3
  br label %25

25:                                               ; preds = %1, %45
  %26 = phi i32 [ 0, %1 ], [ %46, %45 ]
  %27 = mul nuw nsw i32 %26, 7372
  %28 = udiv i32 %27, 191
  %29 = add nsw i32 %28, -3686
  %30 = shl nuw nsw i32 %26, 2
  %31 = or disjoint i32 %30, 1
  %32 = or disjoint i32 %30, 2
  br label %33

33:                                               ; preds = %25, %33
  %34 = phi i32 [ 0, %25 ], [ %43, %33 ]
  %35 = mul nuw nsw i32 %34, 14746
  %36 = udiv i32 %35, 383
  %37 = add nsw i32 %36, -7373
  %38 = tail call i32 @calculateIterations(i32 noundef %37, i32 noundef %29, i32 noundef %12, i32 noundef %23) #6
  %39 = tail call i32 @makeColor(i32 noundef %38, i32 noundef %24) #6, !range !5
  %40 = shl nuw nsw i32 %34, 2
  %41 = or disjoint i32 %40, 1
  tail call void @simPutPixel(i32 noundef %41, i32 noundef %31, i32 noundef %39) #5
  %42 = or disjoint i32 %40, 2
  tail call void @simPutPixel(i32 noundef %42, i32 noundef %31, i32 noundef %39) #5
  tail call void @simPutPixel(i32 noundef %41, i32 noundef %32, i32 noundef %39) #5
  tail call void @simPutPixel(i32 noundef %42, i32 noundef %32, i32 noundef %39) #5
  %43 = add nuw nsw i32 %34, 1
  %44 = icmp eq i32 %43, 384
  br i1 %44, label %45, label %33, !llvm.loop !6

45:                                               ; preds = %33
  %46 = add nuw nsw i32 %26, 1
  %47 = icmp eq i32 %46, 192
  br i1 %47, label %48, label %25, !llvm.loop !8

48:                                               ; preds = %45
  tail call void @simFlush() #5
  %49 = add nsw i32 %2, 1
  %50 = icmp eq i32 %49, 1000000
  %51 = select i1 %50, i32 0, i32 %49
  br label %1
}

; Function Attrs: optsize
declare void @simFlush() local_unnamed_addr #3

attributes #0 = { mustprogress nofree norecurse nosync nounwind optsize willreturn memory(none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nofree norecurse nosync nounwind optsize memory(none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { nounwind optsize uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { optsize "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { noreturn nounwind optsize uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { nounwind optsize }
attributes #6 = { optsize }

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{!"Ubuntu clang version 18.1.3 (1ubuntu1)"}
!5 = !{i32 -16777216, i32 0}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
!8 = distinct !{!8, !7}
