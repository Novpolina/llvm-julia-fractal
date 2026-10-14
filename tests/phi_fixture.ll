; Synthetic app.c: untaken branch, nested PHIs, self-cycle, duplicate operands.
source_filename = "app.c"

define i32 @fixture(i32 %argument, i1 %condition) {
entry:
  br i1 %condition, label %left, label %right
left:
  %sum = add i32 %argument, 1
  br label %merge
right:
  %difference = sub i32 %argument, 1
  br label %merge
merge:
  %choice = phi i32 [ %sum, %left ], [ %difference, %right ]
  br label %loop
loop:
  %nested = phi i32 [ %choice, %merge ], [ %nested, %loop ]
  %index = phi i32 [ 0, %merge ], [ %next, %loop ]
  %repeated = mul i32 %nested, %nested
  %next = add i32 %index, 1
  %again = icmp slt i32 %next, 2
  br i1 %again, label %loop, label %exit
exit:
  %result = shl i32 %nested, 1
  ret i32 %result
}
